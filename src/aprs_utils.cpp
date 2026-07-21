#include "aprs_utils.h"
#include "configuration.h"
#include "gps_utils.h"
#include "board_pins.h"
#include <WiFi.h>
#include <WiFiClient.h>

static WiFiClient aprsClient;
static bool       aprsConnected  = false;
static uint32_t   lastConnectMs  = 0;
static uint32_t   rxCount = 0, txCount = 0;

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
        // Try to parse DDmm.hhN format
        int pos = 1;
        if (p.type == '@' || p.type == '/') pos = 8; // skip timestamp
        if (pos + 19 < (int)info.length()) {
            // Extract lat/lon from fixed-format position
            String latStr = info.substring(pos, pos + 8);    // DDmm.hhN
            char sym1 = info[pos + 8];
            String lonStr = info.substring(pos + 9, pos + 18); // DDDmm.hhE
            char sym2 = info[pos + 18];
            p.symbol = String(sym1) + String(sym2);

            int latDeg = latStr.substring(0,2).toInt();
            float latMin = latStr.substring(2,7).toFloat();
            char latHemi = latStr[7];
            p.lat = latDeg + latMin / 60.0f;
            if (latHemi == 'S') p.lat = -p.lat;

            int lonDeg = lonStr.substring(0,3).toInt();
            float lonMin = lonStr.substring(3,8).toFloat();
            char lonHemi = lonStr[8];
            p.lon = lonDeg + lonMin / 60.0f;
            if (lonHemi == 'W') p.lon = -p.lon;
            // Sanity: reject out-of-range coordinates from malformed packets
            p.hasPosition = (p.lat != 0 || p.lon != 0)
                          && p.lat >= -90.0f && p.lat <= 90.0f
                          && p.lon >= -180.0f && p.lon <= 180.0f;

            // Comment (everything after symbol)
            p.comment = info.substring(pos + 19);
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

// ── APRS-IS TCP connection ────────────────────────────────────────────────

void APRS_Utils::connect() {
    if (!WiFi.isConnected() || !Config.beacon.aprsIsEnabled) return;
    if (aprsConnected) return;
    uint32_t now = millis();
    if (now - lastConnectMs < 10000) return;
    lastConnectMs = now;

    Serial.printf("APRS-IS: connecting to %s:%d\n", APRSIS_HOST, APRSIS_PORT);
    if (!aprsClient.connect(APRSIS_HOST, APRSIS_PORT)) {
        Serial.println("APRS-IS: connection failed");
        return;
    }
    aprsClient.setTimeout(50);  // never block the main loop on partial lines
    // Login
    String login = "user " + fullCallsign()
        + " pass " + String(Config.aprs.passcode)
        + " vers 2E0LXY-Tracker " FW_VERSION
        + " filter m/50\r\n";
    aprsClient.print(login);
    aprsConnected = true;
    Serial.println("APRS-IS: connected");
}

void APRS_Utils::disconnect() {
    aprsClient.stop();
    aprsConnected = false;
}

bool APRS_Utils::isConnected() { return aprsConnected && aprsClient.connected(); }

void APRS_Utils::loop() {
    if (!aprsConnected && WiFi.isConnected()) {
        connect();
        return;
    }
    if (!aprsClient.connected()) {
        aprsConnected = false;
        return;
    }
    // Read incoming packets
    while (aprsClient.available()) {
        String line = aprsClient.readStringUntil('\n');
        line.trim();
        if (line.startsWith("#")) continue; // server comment
        if (line.length() < 5) continue;
        rxCount++;
        ParsedPacket p = parsePacket(line);
        if (p.valid && p.hasPosition) {
            updateStation(p.fromCall, p);
        }
        if (p.isMessage && p.toCall == fullCallsign()) {
            onMessageReceived(p);
        }
    }
}

bool APRS_Utils::sendPacketIS(const String& packet) {
    if (!isConnected()) return false;
    aprsClient.println(packet);
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
            s.rssi = p.rssi; s.snr = p.snr;
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
    s.lastHeardMs = millis(); s.rssi = p.rssi; s.snr = p.snr;
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
