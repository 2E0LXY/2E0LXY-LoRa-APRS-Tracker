#include "configuration.h"
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
