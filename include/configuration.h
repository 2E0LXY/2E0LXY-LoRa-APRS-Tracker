#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ── Callsign / APRS-IS ───────────────────────────────────────────────────
struct APRSConfig {
    String  callsign    = "N0CALL";
    int     ssid        = 9;
    int     passcode    = -1;
    String  path        = "WIDE1-1,WIDE2-1";
    String  comment     = "2E0LXY T-Deck";
    String  symbol      = "/>"; // car
};

// ── LoRa RF ─────────────────────────────────────────────────────────────
struct LoRaConfig {
    float   freq        = 439.9125;
    int     sf          = 12;
    float   bw          = 125.0;
    int     cr          = 5;
    int     txPower     = 17;
    int     preamble    = 8;
};

// ── SmartBeacon ──────────────────────────────────────────────────────────
struct BeaconConfig {
    bool    smartEnabled    = true;
    int     slowRate        = 300;   // seconds when stationary
    int     fastRate        = 30;    // seconds at speed
    int     speedThreshold  = 5;     // km/h above = fast rate
    int     turnAngle       = 30;    // degrees change triggers beacon
    int     minDistance     = 100;   // metres between forced beacons
    bool    aprsIsEnabled   = true;
    bool    loraEnabled     = true;
};

// ── WiFi ─────────────────────────────────────────────────────────────────
struct WiFiConfig {
    String  ssid        = "";
    String  password    = "";
    bool    enabled     = true;
};

// ── MQTT (aprsnet.uk) ────────────────────────────────────────────────────
struct MQTTConfig {
    bool    active      = false;
    String  server      = "80.64.216.113";
    int     port        = 1883;
    String  topic       = "aprsnet";
    String  username    = "";       // member callsign
    String  password    = "";       // member password
};

// ── Display ──────────────────────────────────────────────────────────────
struct DisplayConfig {
    int     brightness  = 128;      // 0-255
    bool    nightMode   = false;
    int     timeout     = 60;       // seconds before dim; 0 = always on
};

// ── Root config ──────────────────────────────────────────────────────────
struct Configuration {
    APRSConfig    aprs;
    LoRaConfig    lora;
    BeaconConfig  beacon;
    WiFiConfig    wifi;
    MQTTConfig    mqtt;
    DisplayConfig display;
    String        fwVersion = "1.0.0";
};

extern Configuration Config;

bool loadConfig();
bool saveConfig();
void resetConfig();
String  fullCallsign();   // e.g. "2E0LXY-9"
int     calcPasscode(const String& call);
