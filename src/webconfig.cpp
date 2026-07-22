#include "webconfig.h"
#include "board_pins.h"
#include "configuration.h"
#include "regional.h"
#include "beacon_utils.h"
#include "lora_utils.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);
static bool running = false;

// Forward declarations (used in buildPage before their definitions below)
static String   rgb565ToHex(uint16_t c);
static uint16_t hexToRgb565(const String& hex);

// ── Settings page (single self-contained HTML) ────────────────────────────
static String buildPage() {
    String h = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>2E0LXY Tracker Setup</title><style>"
        "body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:16px;max-width:640px;margin:0 auto}"
        "h1{font-size:20px;color:#38bdf8}h2{font-size:15px;color:#7dd3fc;margin-top:20px;border-bottom:1px solid #334155;padding-bottom:4px}"
        "label{display:block;font-size:13px;margin:10px 0 3px;color:#94a3b8}"
        "input,select{width:100%;box-sizing:border-box;background:#1e293b;border:1px solid #334155;color:#fff;padding:8px;border-radius:6px;font-size:14px}"
        "input[type=checkbox]{width:auto}.row{display:flex;gap:10px}.row>div{flex:1}"
        "button{background:#0891b2;color:#fff;border:0;padding:12px;border-radius:8px;font-size:15px;font-weight:bold;width:100%;margin-top:16px;cursor:pointer}"
        ".warn{background:#7f1d1d;color:#fecaca;padding:8px;border-radius:6px;font-size:12px;margin:8px 0}"
        ".ok{background:#064e3b;color:#a7f3d0;padding:8px;border-radius:6px;font-size:12px;margin:8px 0}"
        "small{color:#64748b;font-size:11px}"
        "</style></head><body><h1>&#128225; 2E0LXY Tracker Setup "
        "<span style='font-size:11px;color:#64748b'>fw ")
        + String(FW_VERSION) + F("</span></h1>"
        "<form method='POST' action='/save'>");

    // ── Identity ──
    h += F("<h2>Identity</h2>");
    h += "<div class='row'><div><label>Callsign</label><input name='callsign' value='" + Config.aprs.callsign + "' maxlength='6'></div>";
    h += "<div><label>SSID</label><input name='ssid' type='number' min='0' max='15' value='" + String(Config.aprs.ssid) + "'></div></div>";
    h += "<label>Comment</label><input name='comment' value='" + Config.aprs.comment + "' maxlength='43'>";
    h += "<label>Symbol (2 chars, e.g. /&gt; for car)</label><input name='symbol' value='" + Config.aprs.symbol + "' maxlength='2'>";

    // ── Region ──
    h += F("<h2>Region &amp; Frequency</h2>");
    h += F("<label>Regional preset</label><select name='region' onchange='regionChanged(this.value)'>");
    for (int i = 0; i < REGIONAL_PROFILE_COUNT; i++) {
        h += "<option value='" + String(REGIONAL_PROFILES[i].id) + "'";
        if (Config.region.profileId.equalsIgnoreCase(REGIONAL_PROFILES[i].id)) h += " selected";
        h += ">" + String(REGIONAL_PROFILES[i].name) + "  (" + String(REGIONAL_PROFILES[i].freqMHz, 4) + " MHz)</option>";
    }
    h += F("</select><small>Selecting a preset fills the fields below and DISABLES TX until you confirm.</small>");

    h += "<div class='row'><div><label>Frequency (MHz)</label><input name='freq' type='number' step='0.0001' value='" + String(Config.lora.freq, 4) + "'></div>";
    h += "<div><label>TX Power (dBm)</label><input name='power' type='number' min='2' max='22' value='" + String(Config.lora.txPower) + "'></div></div>";
    h += "<div class='row'><div><label>Spreading Factor</label><input name='sf' type='number' min='7' max='12' value='" + String(Config.lora.sf) + "'></div>";
    h += "<div><label>Bandwidth (kHz)</label><input name='bw' type='number' step='0.1' value='" + String(Config.lora.bw, 1) + "'></div>";
    h += "<div><label>Coding Rate (4/x)</label><input name='cr' type='number' min='5' max='8' value='" + String(Config.lora.cr) + "'></div></div>";

    h += "<label><input type='checkbox' name='tx_ok' " + String(Config.region.txConfirmed ? "checked" : "") + "> I confirm this frequency &amp; power are legal for my licence and region — enable TX</label>";
    if (!Config.region.txConfirmed) h += F("<div class='warn'>&#9888; TX is currently DISABLED. Tick the box above to enable transmitting.</div>");

    // ── Beaconing ──
    h += F("<h2>Beaconing</h2>");
    h += "<label><input type='checkbox' name='smart' " + String(Config.beacon.smartEnabled ? "checked" : "") + "> SmartBeaconing (adaptive rate by speed/turns)</label>";
    h += "<div class='row'><div><label>Slow rate (s, stationary)</label><input name='slow' type='number' value='" + String(Config.beacon.slowRate) + "'></div>";
    h += "<div><label>Fast rate (s, moving)</label><input name='fast' type='number' value='" + String(Config.beacon.fastRate) + "'></div></div>";
    h += "<div class='row'><div><label>Speed threshold (km/h)</label><input name='speed_th' type='number' value='" + String(Config.beacon.speedThreshold) + "'></div>";
    h += "<div><label>Turn angle (deg)</label><input name='turn' type='number' value='" + String(Config.beacon.turnAngle) + "'></div>";
    h += "<div><label>Min distance (m)</label><input name='min_dist' type='number' value='" + String(Config.beacon.minDistance) + "'></div></div>";
    h += "<label>Path</label><input name='path' value='" + Config.aprs.path + "'>";
    h += "<label><input type='checkbox' name='b_lora' " + String(Config.beacon.loraEnabled ? "checked" : "") + "> Beacon over LoRa RF</label>";
    h += "<label><input type='checkbox' name='b_aprsis' " + String(Config.beacon.aprsIsEnabled ? "checked" : "") + "> Beacon over APRS-IS (WiFi)</label>";

    // ── WiFi ──
    h += F("<h2>WiFi</h2>");
    h += "<label>SSID</label><input name='wifi_ssid' value='" + Config.wifi.ssid + "'>";
    h += "<label>Password</label><input name='wifi_pass' type='password' value='" + Config.wifi.password + "'>";

    // ── aprsnet.uk server (MQTT) ──
    h += F("<h2>aprsnet.uk Server</h2>");
    h += "<label><input type='checkbox' name='mqtt_on' " + String(Config.mqtt.active ? "checked" : "") + "> Connect to aprsnet.uk (telemetry + remote control + server messaging)</label>";
    h += "<label>Member callsign</label><input name='mqtt_user' value='" + Config.mqtt.username + "'>";
    h += "<label>Member password</label><input name='mqtt_pass' type='password' value='" + Config.mqtt.password + "'>";

    // ── Messaging theme ──
    h += F("<h2>Messaging</h2>");
    h += F("<label>Default send route</label><select name='route'>");
    const char* routes[] = {"lora","aprsis","server"};
    const char* rlabels[] = {"LoRa RF","APRS-IS (WiFi)","aprsnet.uk server (no RF)"};
    for (int i=0;i<3;i++){ h += "<option value='"+String(routes[i])+"'"+(Config.msg.defaultRoute==routes[i]?" selected":"")+">"+rlabels[i]+"</option>"; }
    h += F("</select>");
    h += "<div class='row'><div><label>Sent bubble</label><input name='c_out' type='color' value='" + rgb565ToHex(Config.msg.outBubble) + "'></div>";
    h += "<div><label>Received bubble</label><input name='c_in' type='color' value='" + rgb565ToHex(Config.msg.inBubble) + "'></div>";
    h += "<div><label>Background</label><input name='c_bg' type='color' value='" + rgb565ToHex(Config.msg.bgColour) + "'></div></div>";

    // ── Display ──
    // ── Bluetooth ──
    h += F("<h2>Bluetooth (KISS TNC)</h2>");
    h += "<label><input type='checkbox' name='ble_on' " + String(Config.device.bleEnabled ? "checked" : "") + "> Enable BLE KISS TNC (use tracker as a Bluetooth modem for phone apps)</label>";
    h += F("<small>Leave OFF unless you use APRSdroid/YAAC over Bluetooth. Requires a reboot; uses extra memory.</small>");

    h += F("<h2>Display</h2>");
    h += "<label>Brightness (0-255)</label><input name='bright' type='number' min='0' max='255' value='" + String(Config.display.brightness) + "'>";

    h += F("<button type='submit'>&#128190; Save &amp; Reboot</button></form>");

    // Beacon Now (separate form so it doesn't save the whole page)
    h += F("<form method='POST' action='/beacon'><button type='submit' style='background:#166534'>&#128225; Beacon Now</button></form>");

    // Region JS: fill fields from preset
    h += F("<script>var P=");
    // Emit profiles as JS
    h += "[";
    for (int i = 0; i < REGIONAL_PROFILE_COUNT; i++) {
        auto& p = REGIONAL_PROFILES[i];
        h += "{id:'" + String(p.id) + "',f:" + String(p.freqMHz,4) + ",sf:" + String(p.sf) + ",bw:" + String(p.bwKHz,1) + ",cr:" + String(p.cr) + ",pw:" + String(p.powerDbm) + ",path:'" + String(p.beaconPath) + "'}";
        if (i < REGIONAL_PROFILE_COUNT - 1) h += ",";
    }
    h += F("];function regionChanged(id){var p=P.find(x=>x.id==id);if(!p)return;"
        "document.querySelector('[name=freq]').value=p.f;"
        "document.querySelector('[name=sf]').value=p.sf;"
        "document.querySelector('[name=bw]').value=p.bw;"
        "document.querySelector('[name=cr]').value=p.cr;"
        "document.querySelector('[name=power]').value=p.pw;"
        "document.querySelector('[name=path]').value=p.path;"
        "document.querySelector('[name=tx_ok]').checked=false;}"
        "</script></body></html>");
    return h;
}

// RGB565 → #rrggbb for the colour pickers
static String rgb565ToHex(uint16_t c) {
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >> 5) & 0x3F) << 2;
    uint8_t b = (c & 0x1F) << 3;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return String(buf);
}

static uint16_t hexToRgb565(const String& hex) {
    long v = strtol(hex.c_str() + (hex[0] == '#' ? 1 : 0), nullptr, 16);
    uint8_t r = (v >> 16) & 0xFF, g = (v >> 8) & 0xFF, b = v & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void handleRoot() { server.send(200, "text/html", buildPage()); }

static void handleSave() {
    auto arg = [&](const char* n){ return server.arg(n); };
    auto has = [&](const char* n){ return server.hasArg(n); };

    Config.aprs.callsign = arg("callsign"); Config.aprs.callsign.toUpperCase();
    Config.aprs.ssid     = arg("ssid").toInt();
    Config.aprs.comment  = arg("comment");
    Config.aprs.symbol   = arg("symbol");
    Config.aprs.passcode = calcPasscode(Config.aprs.callsign);

    Config.region.profileId   = arg("region");
    Config.lora.freq     = arg("freq").toFloat();
    Config.lora.txPower  = arg("power").toInt();
    Config.lora.sf       = arg("sf").toInt();
    Config.lora.bw       = arg("bw").toFloat();
    Config.lora.cr       = arg("cr").toInt();
    Config.region.txConfirmed = has("tx_ok");

    Config.beacon.smartEnabled   = has("smart");
    Config.beacon.slowRate       = arg("slow").toInt();
    Config.beacon.fastRate       = arg("fast").toInt();
    Config.beacon.speedThreshold = arg("speed_th").toInt();
    Config.beacon.turnAngle      = arg("turn").toInt();
    Config.beacon.minDistance    = arg("min_dist").toInt();
    Config.aprs.path             = arg("path");
    // TX over LoRa only allowed if operator confirmed the region
    Config.beacon.loraEnabled    = has("b_lora") && Config.region.txConfirmed;
    Config.beacon.aprsIsEnabled  = has("b_aprsis");

    Config.wifi.ssid     = arg("wifi_ssid");
    Config.wifi.password = arg("wifi_pass");

    Config.mqtt.active   = has("mqtt_on");
    Config.device.bleEnabled = has("ble_on");
    Config.mqtt.username = arg("mqtt_user");
    Config.mqtt.password = arg("mqtt_pass");

    Config.msg.defaultRoute = arg("route");
    Config.msg.outBubble = hexToRgb565(arg("c_out"));
    Config.msg.inBubble  = hexToRgb565(arg("c_in"));
    Config.msg.bgColour  = hexToRgb565(arg("c_bg"));

    Config.display.brightness = arg("bright").toInt();

    saveConfig();
    server.send(200, "text/html",
        "<html><body style='background:#0f172a;color:#a7f3d0;font-family:sans-serif;text-align:center;padding:40px'>"
        "<h2>&#10003; Saved. Rebooting...</h2></body></html>");
    delay(800);
    ESP.restart();
}

static void handleBeacon() {
    Beacon_Utils::sendBeacon();
    server.send(200, "text/html",
        "<html><body style='background:#0f172a;color:#a7f3d0;font-family:sans-serif;text-align:center;padding:40px'>"
        "<h2>&#128225; Beacon sent!</h2><a href='/' style='color:#38bdf8'>Back</a></body></html>");
}

void WebConfig::begin() {
    // AP mode: SSID "2E0LXY-Tracker", open (operator is physically present)
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("2E0LXY-Tracker-Setup");
    delay(200);
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/beacon", HTTP_POST, handleBeacon);
    server.begin();
    running = true;
    Serial.print("WebConfig AP up: http://");
    Serial.println(WiFi.softAPIP());
}

void WebConfig::loop() { if (running) server.handleClient(); }
bool WebConfig::isRunning() { return running; }
void WebConfig::stop() { if (running) { server.stop(); WiFi.softAPdisconnect(true); running = false; } }
