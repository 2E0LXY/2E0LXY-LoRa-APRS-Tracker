#include "configuration.h"
#include "regional.h"
#include <LittleFS.h>

Configuration Config;
static const char* CFG_FILE = "/config.json";

int calcPasscode(const String& call) {
    String c = call;
    c.toUpperCase();
    if (c.indexOf('-') >= 0) c = c.substring(0, c.indexOf('-'));
    int hash = 0x73e2;
    for (size_t i = 0; i < c.length(); i += 2) {
        hash ^= (int)c[i] << 8;
        if (i + 1 < c.length()) hash ^= (int)c[i + 1];
    }
    return hash & 0x7FFF;
}

String fullCallsign() {
    return Config.aprs.callsign + "-" + String(Config.aprs.ssid);
}

bool loadConfig() {
    if (!LittleFS.begin(true)) return false;
    if (!LittleFS.exists(CFG_FILE)) return false;
    File f = LittleFS.open(CFG_FILE, "r");
    if (!f) return false;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return false; }
    f.close();

    Config.aprs.callsign    = doc["aprs"]["callsign"] | "N0CALL";
    Config.aprs.ssid        = doc["aprs"]["ssid"]     | 9;
    Config.aprs.passcode    = doc["aprs"]["passcode"] | -1;
    Config.aprs.path        = doc["aprs"]["path"]     | "WIDE1-1,WIDE2-1";
    Config.aprs.comment     = doc["aprs"]["comment"]  | "2E0LXY T-Deck";
    Config.aprs.symbol      = doc["aprs"]["symbol"]   | "/>";

    Config.lora.freq        = doc["lora"]["freq"]     | 439.9125f;
    Config.lora.sf          = doc["lora"]["sf"]       | 12;
    Config.lora.bw          = doc["lora"]["bw"]       | 125.0f;
    Config.lora.cr          = doc["lora"]["cr"]       | 5;
    Config.lora.txPower     = doc["lora"]["power"]    | 17;

    Config.beacon.smartEnabled   = doc["beacon"]["smart"]    | true;
    Config.beacon.slowRate       = doc["beacon"]["slow"]     | 300;
    Config.beacon.fastRate       = doc["beacon"]["fast"]     | 30;
    Config.beacon.speedThreshold = doc["beacon"]["speed_th"] | 5;
    Config.beacon.turnAngle      = doc["beacon"]["turn"]     | 30;
    Config.beacon.minDistance    = doc["beacon"]["min_dist"] | 100;
    Config.beacon.aprsIsEnabled  = doc["beacon"]["aprsis"]   | true;
    Config.beacon.loraEnabled    = doc["beacon"]["lora"]     | true;

    Config.wifi.ssid        = doc["wifi"]["ssid"]     | "";
    Config.wifi.password    = doc["wifi"]["password"] | "";
    Config.wifi.enabled     = doc["wifi"]["enabled"]  | true;

    Config.mqtt.active      = doc["mqtt"]["active"]   | false;
    Config.mqtt.server      = doc["mqtt"]["server"]   | "80.64.216.113";
    Config.mqtt.port        = doc["mqtt"]["port"]     | 1883;
    Config.mqtt.topic       = doc["mqtt"]["topic"]    | "aprsnet";
    Config.mqtt.username    = doc["mqtt"]["user"]     | "";
    Config.mqtt.password    = doc["mqtt"]["pass"]     | "";

    Config.display.brightness = doc["display"]["brightness"] | 128;
    Config.display.nightMode  = doc["display"]["night"]      | false;
    Config.display.timeout    = doc["display"]["timeout"]    | 60;

    Config.msg.bgColour     = doc["msg"]["bg"]        | 0x1082;
    Config.msg.outBubble    = doc["msg"]["out_bubble"]| 0x04A9;
    Config.msg.inBubble     = doc["msg"]["in_bubble"] | 0x3186;
    Config.msg.outText      = doc["msg"]["out_text"]  | 0xFFFF;
    Config.msg.inText       = doc["msg"]["in_text"]   | 0xFFFF;
    Config.msg.defaultRoute = doc["msg"]["route"]     | "lora";

    Config.region.profileId   = doc["region"]["profile"]  | "uk";
    Config.region.txConfirmed = doc["region"]["tx_ok"]    | false;
    Config.region.timezone    = doc["region"]["tz"]       | "GMT0BST,M3.5.0/1,M10.5.0/2";

    Config.weather.enabled    = doc["weather"]["enabled"]  | false;
    Config.weather.txWx       = doc["weather"]["tx"]       | false;
    Config.weather.wxInterval = doc["weather"]["interval"] | 600;
    Config.weather.tempOffset = doc["weather"]["t_offset"] | 0.0f;

    initDefaultProfiles();
    Config.activeProfile = doc["active_profile"] | 1;
    JsonArray profs = doc["profiles"].as<JsonArray>();
    if (!profs.isNull()) {
        int i = 0;
        for (JsonObject po : profs) {
            if (i >= 4) break;
            Config.profiles[i].name      = po["name"]      | Config.profiles[i].name;
            Config.profiles[i].ssid      = po["ssid"]      | Config.profiles[i].ssid;
            Config.profiles[i].symbol    = po["symbol"]    | Config.profiles[i].symbol;
            Config.profiles[i].comment   = po["comment"]   | Config.profiles[i].comment;
            Config.profiles[i].smart     = po["smart"]     | Config.profiles[i].smart;
            Config.profiles[i].slowRate  = po["slow"]      | Config.profiles[i].slowRate;
            Config.profiles[i].fastRate  = po["fast"]      | Config.profiles[i].fastRate;
            Config.profiles[i].speedThr  = po["speed_th"]  | Config.profiles[i].speedThr;
            Config.profiles[i].turnAngle = po["turn"]      | Config.profiles[i].turnAngle;
            Config.profiles[i].minDist   = po["min_dist"]  | Config.profiles[i].minDist;
            i++;
        }
    }
    applyOpProfile(Config.activeProfile);

    if (Config.aprs.passcode < 0)
        Config.aprs.passcode = calcPasscode(Config.aprs.callsign);

    return true;
}

bool saveConfig() {
    if (!LittleFS.begin(true)) return false;
    JsonDocument doc;
    doc["aprs"]["callsign"] = Config.aprs.callsign;
    doc["aprs"]["ssid"]     = Config.aprs.ssid;
    doc["aprs"]["passcode"] = Config.aprs.passcode;
    doc["aprs"]["path"]     = Config.aprs.path;
    doc["aprs"]["comment"]  = Config.aprs.comment;
    doc["aprs"]["symbol"]   = Config.aprs.symbol;

    doc["lora"]["freq"]     = Config.lora.freq;
    doc["lora"]["sf"]       = Config.lora.sf;
    doc["lora"]["bw"]       = Config.lora.bw;
    doc["lora"]["cr"]       = Config.lora.cr;
    doc["lora"]["power"]    = Config.lora.txPower;

    doc["beacon"]["smart"]    = Config.beacon.smartEnabled;
    doc["beacon"]["slow"]     = Config.beacon.slowRate;
    doc["beacon"]["fast"]     = Config.beacon.fastRate;
    doc["beacon"]["speed_th"] = Config.beacon.speedThreshold;
    doc["beacon"]["turn"]     = Config.beacon.turnAngle;
    doc["beacon"]["min_dist"] = Config.beacon.minDistance;
    doc["beacon"]["aprsis"]   = Config.beacon.aprsIsEnabled;
    doc["beacon"]["lora"]     = Config.beacon.loraEnabled;

    doc["wifi"]["ssid"]     = Config.wifi.ssid;
    doc["wifi"]["password"] = Config.wifi.password;
    doc["wifi"]["enabled"]  = Config.wifi.enabled;

    doc["mqtt"]["active"]   = Config.mqtt.active;
    doc["mqtt"]["server"]   = Config.mqtt.server;
    doc["mqtt"]["port"]     = Config.mqtt.port;
    doc["mqtt"]["topic"]    = Config.mqtt.topic;
    doc["mqtt"]["user"]     = Config.mqtt.username;
    doc["mqtt"]["pass"]     = Config.mqtt.password;

    doc["display"]["brightness"] = Config.display.brightness;
    doc["display"]["night"]      = Config.display.nightMode;
    doc["display"]["timeout"]    = Config.display.timeout;

    doc["msg"]["bg"]         = Config.msg.bgColour;
    doc["msg"]["out_bubble"] = Config.msg.outBubble;
    doc["msg"]["in_bubble"]  = Config.msg.inBubble;
    doc["msg"]["out_text"]   = Config.msg.outText;
    doc["msg"]["in_text"]    = Config.msg.inText;
    doc["msg"]["route"]      = Config.msg.defaultRoute;

    doc["region"]["profile"] = Config.region.profileId;
    doc["region"]["tx_ok"]   = Config.region.txConfirmed;
    doc["region"]["tz"]      = Config.region.timezone;

    doc["weather"]["enabled"]  = Config.weather.enabled;
    doc["weather"]["tx"]       = Config.weather.txWx;
    doc["weather"]["interval"] = Config.weather.wxInterval;
    doc["weather"]["t_offset"] = Config.weather.tempOffset;

    doc["active_profile"] = Config.activeProfile;
    JsonArray profs = doc["profiles"].to<JsonArray>();
    for (int i = 0; i < 4; i++) {
        JsonObject po = profs.add<JsonObject>();
        po["name"]     = Config.profiles[i].name;
        po["ssid"]     = Config.profiles[i].ssid;
        po["symbol"]   = Config.profiles[i].symbol;
        po["comment"]  = Config.profiles[i].comment;
        po["smart"]    = Config.profiles[i].smart;
        po["slow"]     = Config.profiles[i].slowRate;
        po["fast"]     = Config.profiles[i].fastRate;
        po["speed_th"] = Config.profiles[i].speedThr;
        po["turn"]     = Config.profiles[i].turnAngle;
        po["min_dist"] = Config.profiles[i].minDist;
    }

    File f = LittleFS.open(CFG_FILE, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

void resetConfig() {
    Config = Configuration();
    saveConfig();
}

// Apply a regional preset — sets RF params, APRS-IS server, timezone, path.
// TX is disabled (txConfirmed=false) whenever the profile changes, forcing
// the operator to review and confirm before transmitting (safety).
bool applyRegionProfile(const String& id) {
    const RegionalProfile* p = findProfile(id);
    if (!p) return false;

    Config.region.profileId = p->id;
    Config.region.timezone  = p->timezone;

    Config.lora.freq     = p->freqMHz;
    Config.lora.sf       = p->sf;
    Config.lora.bw       = p->bwKHz;
    Config.lora.cr       = p->cr;
    Config.lora.txPower  = p->powerDbm;

    Config.aprs.path     = p->beaconPath;

    // Only overwrite the server if it's still the default aprsnet.uk or empty,
    // so a user who set a custom server isn't clobbered when tweaking regions.
    // (Custom profile leaves everything user-editable.)
    if (String(p->id) != "custom") {
        // Note: APRS-IS host is compiled default; regional server preference
        // is surfaced in the web UI. We keep the aprsnet.uk default unless the
        // user explicitly changes it, matching the iGate behaviour.
    }

    // Changing region disables TX until the operator confirms.
    Config.region.txConfirmed = false;
    Config.beacon.loraEnabled = false;

    return true;
}

// Populate the four built-in operating profiles with sensible defaults.
// Only sets them if they look uninitialised (name empty).
void initDefaultProfiles() {
    static bool done = false;
    if (done) return;
    done = true;

    // Walking — slow, hiker symbol
    Config.profiles[0].name="Walking"; Config.profiles[0].ssid=7; Config.profiles[0].symbol="/[";
    Config.profiles[0].comment="2E0LXY on foot"; Config.profiles[0].smart=true;
    Config.profiles[0].slowRate=600; Config.profiles[0].fastRate=120;
    Config.profiles[0].speedThr=3; Config.profiles[0].turnAngle=45; Config.profiles[0].minDist=50;
    // Car — the default, faster beaconing
    Config.profiles[1].name="Car"; Config.profiles[1].ssid=9; Config.profiles[1].symbol="/>";
    Config.profiles[1].comment="2E0LXY mobile"; Config.profiles[1].smart=true;
    Config.profiles[1].slowRate=300; Config.profiles[1].fastRate=30;
    Config.profiles[1].speedThr=8; Config.profiles[1].turnAngle=28; Config.profiles[1].minDist=150;
    // Bicycle
    Config.profiles[2].name="Bicycle"; Config.profiles[2].ssid=8; Config.profiles[2].symbol="/b";
    Config.profiles[2].comment="2E0LXY cycling"; Config.profiles[2].smart=true;
    Config.profiles[2].slowRate=300; Config.profiles[2].fastRate=60;
    Config.profiles[2].speedThr=5; Config.profiles[2].turnAngle=35; Config.profiles[2].minDist=80;
    // Stationary — fixed
    Config.profiles[3].name="Stationary"; Config.profiles[3].ssid=0; Config.profiles[3].symbol="/-";
    Config.profiles[3].comment="2E0LXY fixed"; Config.profiles[3].smart=false;
    Config.profiles[3].slowRate=1800; Config.profiles[3].fastRate=1800;
    Config.profiles[3].speedThr=99; Config.profiles[3].turnAngle=360; Config.profiles[3].minDist=9999;
}

// Switch the active operating profile — copies its symbol/SSID/comment and
// SmartBeacon parameters into the live APRS + beacon config.
void applyOpProfile(int idx) {
    if (idx < 0 || idx > 3) idx = 1;
    Config.activeProfile = idx;
    const OpProfile& p = Config.profiles[idx];
    Config.aprs.ssid    = p.ssid;
    Config.aprs.symbol  = p.symbol;
    Config.aprs.comment = p.comment;
    Config.beacon.smartEnabled   = p.smart;
    Config.beacon.slowRate       = p.slowRate;
    Config.beacon.fastRate       = p.fastRate;
    Config.beacon.speedThreshold = p.speedThr;
    Config.beacon.turnAngle      = p.turnAngle;
    Config.beacon.minDistance    = p.minDist;
}
