#include "aprs_utils.h"
#include "configuration.h"
#include "gps_utils.h"
#include "board_pins.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// APRS-IS over WebSocket (wss://www.aprsnet.uk/ws), not raw TCP:14580.
// Raw APRS-IS TCP repeatedly failed to connect on the operator's network
// even with DNS resolving correctly and a generous timeout, while this
// same server's WebSocket endpoint (used by the website/desktop/Android
// clients, all over port 443) and MQTT (port 1883) both worked fine from
// the same device — strong evidence port 14580 specifically is blocked
// by the network's router/firewall, not a server or firmware fault.
// WebSocket rides on 443 (HTTPS), which is essentially never blocked.
static WebSocketsClient ws;
static bool       aprsConnected  = false;
static bool       wsAuthAcked    = false;   // got auth_ack:success back yet?
static uint32_t   lastConnectMs  = 0;
static uint32_t   rxCount = 0, txCount = 0;
static uint8_t    connectFailStreak = 0;   // consecutive failures — backs off the retry interval

// Parsed received station
StationList APRS_Utils::heardStations;

// ── Packet builders ───────────────────────────────────────────────────────

String APRS_Utils::buildPositionPacket() {
    // !DDmm.hhN/DDDmm.hhE[sym][comment]
    char spd[8], cse[8];
    // APRS spec: speed field is KNOTS, course 001-360 (000 = unknown)
    int knots  = (int)(GPS_Utils::speedKph() / 1.852f);
    int course = (int)GPS_Utils::courseDeg();
    if (course == 0) course = 360;   // 0 means "unknown" per spec
    snprintf(spd, sizeof(spd), "%03d", knots);
    snprintf(cse, sizeof(cse), "%03d", course);

    String sym = Config.aprs.symbol.length() >= 2 ? Config.aprs.symbol : "/>";
    String packet = fullCallsign() + ">APLT00," + Config.aprs.path + ":!"
        + GPS_Utils::aprsLat(GPS_Utils::lat())
        + sym[0]
        + GPS_Utils::aprsLon(GPS_Utils::lon())
        + sym[1]
        + String(cse) + "/" + String(spd);

    if (GPS_Utils::altM() > 0) {
        char alt[12];
        snprintf(alt, sizeof(alt), "/A=%06d", (int)(GPS_Utils::altM() * 3.28084f));
        packet += alt;
    }
    if (Config.aprs.comment.length() > 0) {
        packet += " " + Config.aprs.comment;
    }
    return packet;
}

String APRS_Utils::buildMessagePacket(const String& toCall, const String& text, int msgID) {
    // :CALLSIGN :text{ID
    String dest = toCall;
    while (dest.length() < 9) dest += ' ';
    char id[8];
    snprintf(id, sizeof(id), "{%03d", msgID % 1000);
    return fullCallsign() + ">APLT00," + Config.aprs.path
         + "::" + dest + ":" + text + String(id);
}

String APRS_Utils::buildAckPacket(const String& toCall, const String& msgID) {
    String dest = toCall;
    while (dest.length() < 9) dest += ' ';
    return fullCallsign() + ">APLT00," + Config.aprs.path
         + "::" + dest + ":ack" + msgID;
}

// ── Parse received packet ────────────────────────────────────────────────

ParsedPacket APRS_Utils::parsePacket(const String& raw) {
    ParsedPacket p;
    p.raw = raw;
    int gtIdx = raw.indexOf('>');
    if (gtIdx < 2) return p;
    p.fromCall = raw.substring(0, gtIdx);
    int colonIdx = raw.indexOf(':');
    if (colonIdx < 0) return p;
    p.path = raw.substring(gtIdx + 1, colonIdx);
    String info = raw.substring(colonIdx + 1);
    if (info.length() == 0) return p;
    p.type = info[0];

    // Basic position decode for ! or = types
    if (p.type == '!' || p.type == '=' || p.type == '@' || p.type == '/') {
        int pos = 1;
        if (p.type == '@' || p.type == '/') pos = 8; // skip timestamp
        if (pos < (int)info.length()) {
            // Try strict uncompressed format first: DDmm.hhN/DDDmm.hhE —
            // validated by position/type of every character, not just
            // "starts with a digit" (that alone isn't enough: compressed
            // format's symbol-table char can also be 0-9, so a lone digit
            // check misidentifies compressed packets whose table char
            // happens to be numeric).
            bool uncompressedOk = false;
            if (pos + 19 < (int)info.length()) {
                auto isDigit = [](char c){ return c >= '0' && c <= '9'; };
                String latStr = info.substring(pos, pos + 8);
                char sym1 = info[pos + 8];
                String lonStr = info.substring(pos + 9, pos + 18);
                char sym2 = info[pos + 18];
                uncompressedOk = isDigit(latStr[0]) && isDigit(latStr[1])
                    && isDigit(latStr[2]) && isDigit(latStr[3]) && latStr[4] == '.'
                    && isDigit(latStr[5]) && isDigit(latStr[6])
                    && (latStr[7] == 'N' || latStr[7] == 'S')
                    && isDigit(lonStr[0]) && isDigit(lonStr[1]) && isDigit(lonStr[2])
                    && isDigit(lonStr[3]) && isDigit(lonStr[4]) && lonStr[5] == '.'
                    && isDigit(lonStr[6]) && isDigit(lonStr[7])
                    && (lonStr[8] == 'E' || lonStr[8] == 'W');

                if (uncompressedOk) {
                    p.symbol = String(sym1) + String(sym2);
                    int latDeg = latStr.substring(0,2).toInt();
                    float latMin = latStr.substring(2,7).toFloat();
                    p.lat = latDeg + latMin / 60.0f;
                    if (latStr[7] == 'S') p.lat = -p.lat;
                    int lonDeg = lonStr.substring(0,3).toInt();
                    float lonMin = lonStr.substring(3,8).toFloat();
                    p.lon = lonDeg + lonMin / 60.0f;
                    if (lonStr[8] == 'W') p.lon = -p.lon;
                    p.hasPosition = (p.lat != 0 || p.lon != 0)
                                  && p.lat >= -90.0f && p.lat <= 90.0f
                                  && p.lon >= -180.0f && p.lon <= 180.0f;
                    p.comment = info.substring(pos + 19);
                }
            }

            // Compressed (Base91) format — used by OE5BPA/LoRa_APRS_Tracker/
            // most ESP32 LoRa nodes, confirmed against the aprsnet.uk Go
            // server's own compPosRegex: DTI, table-char [/\\0-9A-Z] (NOT
            // just '/' or '\\' — this iGate's own compression can and does
            // use a letter here), 4 Base91 lat chars, 4 Base91 lon chars,
            // symbol code, 2-char course/speed, 1-byte compression type.
            if (!uncompressedOk && pos + 13 <= (int)info.length()) {
                char table = info[pos];
                bool tableOk = (table == '/' || table == '\\')
                    || (table >= '0' && table <= '9')
                    || (table >= 'A' && table <= 'Z');
                auto inB91Range = [](char c) { return c >= 0x21 && c <= 0x7b; };
                bool b91Ok = true;
                for (int i = 1; i <= 8; i++) {
                    if (!inB91Range(info[pos + i])) { b91Ok = false; break; }
                }
                if (tableOk && b91Ok) {
                    auto b91 = [](char c) -> long { return (long)c - 33; };
                    long latVal = b91(info[pos+1]);
                    latVal = latVal * 91 + b91(info[pos+2]);
                    latVal = latVal * 91 + b91(info[pos+3]);
                    latVal = latVal * 91 + b91(info[pos+4]);
                    long lonVal = b91(info[pos+5]);
                    lonVal = lonVal * 91 + b91(info[pos+6]);
                    lonVal = lonVal * 91 + b91(info[pos+7]);
                    lonVal = lonVal * 91 + b91(info[pos+8]);
                    char symCode = info[pos+9];

                    p.lat = 90.0f - (latVal / 380926.0f);
                    p.lon = -180.0f + (lonVal / 190463.0f);
                    p.symbol = String(table) + String(symCode);
                    p.hasPosition = p.lat >= -90.0f && p.lat <= 90.0f
                                  && p.lon >= -180.0f && p.lon <= 180.0f;
                    p.comment = info.substring(pos + 13);
                }
            }
        }
    }

    // Message decode
    if (p.type == ':' && info.length() > 10) {
        p.toCall = info.substring(1, 10);
        p.toCall.trim();
        int brk = info.indexOf('{', 10);
        p.text = info.substring(11, brk >= 0 ? brk : info.length());
        if (brk >= 0) p.msgID = info.substring(brk + 1);
        p.isMessage = true;
    }

    p.valid = true;
    return p;
}

// ── APRS-IS over WebSocket ─────────────────────────────────────────────────

// Handles one full text frame from the server. Message shapes (from the
// Go server's wsMessage struct): {"type":"auth_ack","status":"success"},
// {"type":"rx","packet":"..."}, plus others we don't need here.
static void handleWsText(const String& payload) {
    JsonDocument doc;   // ArduinoJson v7 — no fixed capacity needed
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return;
    const char* type = doc["type"] | "";

    if (strcmp(type, "auth_ack") == 0) {
        const char* status = doc["status"] | "";
        if (strcmp(status, "success") == 0) {
            wsAuthAcked = true;
            Serial.println("APRS-IS (WS): authenticated");
        } else {
            Serial.println("APRS-IS (WS): auth failed — check callsign/passcode");
            ws.disconnect();
        }
        return;
    }

    if (strcmp(type, "rx") == 0) {
        const char* pkt = doc["packet"] | "";
        if (!pkt || strlen(pkt) < 5) return;
        rxCount++;
        ParsedPacket p = APRS_Utils::parsePacket(String(pkt));
        p.via = HeardVia::INET;
        if (p.valid && p.hasPosition) {
            APRS_Utils::updateStation(p.fromCall, p);
        }
        if (p.isMessage && p.toCall == fullCallsign()) {
            APRS_Utils::onMessageReceived(p);
        }
    }
}

static void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            Serial.println("APRS-IS (WS): connected, authenticating...");
            wsAuthAcked = false;
            JsonDocument doc;
            doc["type"] = "auth";
            doc["callsign"] = fullCallsign();
            doc["passcode"] = String(Config.aprs.passcode);
            doc["software"] = "APRSNet-T-Deck " FW_VERSION;
            String out;
            serializeJson(doc, out);
            ws.sendTXT(out);
            aprsConnected = true;
            connectFailStreak = 0;
            break;
        }
        case WStype_DISCONNECTED:
            Serial.println("APRS-IS (WS): disconnected");
            aprsConnected = false;
            wsAuthAcked = false;
            break;
        case WStype_TEXT:
            handleWsText(String((char*)payload, length));
            break;
        case WStype_ERROR:
            Serial.println("APRS-IS (WS): error");
            break;
        default:
            break;
    }
}

void APRS_Utils::connect() {
    if (!WiFi.isConnected() || !Config.beacon.aprsIsEnabled) return;
    if (aprsConnected) return;
    uint32_t now = millis();
    // Back off on repeated failures (10s, 20s, 40s... capped at 5 min)
    // instead of hammering the server every 10s regardless.
    uint32_t retryMs = 10000UL << min((int)connectFailStreak, 5);   // caps at 320s
    if (now - lastConnectMs < retryMs) return;
    lastConnectMs = now;

    Serial.println("APRS-IS: connecting via WebSocket (wss://www.aprsnet.uk/ws)...");
    ws.beginSSL("www.aprsnet.uk", 443, "/ws");
    ws.onEvent(onWsEvent);
    ws.setReconnectInterval(0);   // we drive reconnect ourselves via loop()/connect()
    // WStype_CONNECTED above flips aprsConnected — begin() itself is
    // async/non-blocking, so there's nothing further to check here.
}

void APRS_Utils::disconnect() {
    ws.disconnect();
    aprsConnected = false;
    wsAuthAcked = false;
}

bool APRS_Utils::isConnected() { return ws.isConnected() && wsAuthAcked; }

void APRS_Utils::loop() {
    ws.loop();
    // Use the library's own live socket state, not just our event-driven
    // aprsConnected bool — a silent drop that never fires
    // WStype_DISCONNECTED (seen in testing) left aprsConnected stuck true
    // forever, so the status bar showed connected/green while nothing was
    // actually flowing and no reconnect was ever attempted.
    if (!ws.isConnected()) {
        aprsConnected = false;
        wsAuthAcked = false;
    }
    if (!aprsConnected && WiFi.isConnected()) {
        connect();
    }

    static uint32_t lastDbgMs = 0;
    if (millis() - lastDbgMs > 10000) {
        lastDbgMs = millis();
        Serial.printf("APRS-IS (WS) debug: ws.isConnected=%d aprsConnected=%d wsAuthAcked=%d rx=%u\n",
            ws.isConnected(), aprsConnected, wsAuthAcked, rxCount);
    }
}

bool APRS_Utils::sendPacketIS(const String& packet) {
    if (!isConnected()) return false;
    JsonDocument doc;
    doc["type"] = "tx";
    doc["packet"] = packet;
    String out;
    serializeJson(doc, out);
    ws.sendTXT(out);
    txCount++;
    return true;
}


// ── Station tracking ──────────────────────────────────────────────────────

void APRS_Utils::updateStation(const String& call, const ParsedPacket& p) {
    for (auto& s : heardStations) {
        if (s.callsign.equalsIgnoreCase(call)) {
            s.lat = p.lat; s.lon = p.lon;
            s.symbol = p.symbol; s.comment = p.comment;
            s.lastHeardMs = millis();
            s.via = p.via;
            if (p.via == HeardVia::RF) {
                s.rssi = p.rssi; s.snr = p.snr;
                s.everHeardRF = true;
            }
            // An internet-relayed packet doesn't carry real RF signal
            // metrics — keep showing the last genuine RF reading (if any)
            // rather than overwrite it with zeros.
            return;
        }
    }
    if (heardStations.size() >= MAX_STATIONS) {
        // Remove oldest
        uint32_t oldest = UINT32_MAX;
        size_t oldIdx = 0;
        for (size_t i = 0; i < heardStations.size(); i++) {
            if (heardStations[i].lastHeardMs < oldest) {
                oldest = heardStations[i].lastHeardMs; oldIdx = i;
            }
        }
        heardStations.erase(heardStations.begin() + oldIdx);
    }
    HeardStation s;
    s.callsign = call; s.lat = p.lat; s.lon = p.lon;
    s.symbol = p.symbol; s.comment = p.comment;
    s.lastHeardMs = millis();
    s.via = p.via;
    if (p.via == HeardVia::RF) {
        s.rssi = p.rssi; s.snr = p.snr;
        s.everHeardRF = true;
    }
    heardStations.push_back(s);
}

float APRS_Utils::distanceKm(float lat1, float lon1, float lat2, float lon2) {
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;
    float a = sin(dLat/2)*sin(dLat/2) +
              cos(lat1*DEG_TO_RAD)*cos(lat2*DEG_TO_RAD)*sin(dLon/2)*sin(dLon/2);
    return 6371.0f * 2 * atan2(sqrt(a), sqrt(1-a));
}

float APRS_Utils::bearingDeg(float lat1, float lon1, float lat2, float lon2) {
    float dLon = (lon2 - lon1) * DEG_TO_RAD;
    float y = sin(dLon) * cos(lat2 * DEG_TO_RAD);
    float x = cos(lat1*DEG_TO_RAD)*sin(lat2*DEG_TO_RAD)
             - sin(lat1*DEG_TO_RAD)*cos(lat2*DEG_TO_RAD)*cos(dLon);
    return fmod(atan2(y, x) * RAD_TO_DEG + 360.0f, 360.0f);
}

uint32_t APRS_Utils::getRxCount() { return rxCount; }
uint32_t APRS_Utils::getTxCount() { return txCount; }

// Weak callbacks — override in main
__attribute__((weak)) void APRS_Utils::onMessageReceived(const ParsedPacket&) {}
