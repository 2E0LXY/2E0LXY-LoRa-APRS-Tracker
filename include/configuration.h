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

// ── Messaging / theme ────────────────────────────────────────────────────
struct MsgConfig {
    // Colours are RGB565 (16-bit) to match the ST7789 TFT.
    // Defaults chosen to mirror the website / Android app palette.
    uint16_t bgColour        = 0x1082;   // near-black background
    uint16_t outBubble       = 0x04A9;   // teal-ish (sent, right)
    uint16_t inBubble        = 0x3186;   // slate-grey (received, left)
    uint16_t outText         = 0xFFFF;   // white
    uint16_t inText          = 0xFFFF;   // white
    // Preferred send route when both are available:
    //   "lora"   — RF only
    //   "aprsis" — APRS-IS over WiFi/TCP
    //   "server" — aprsnet.uk store-and-forward via MQTT (no RF)
    String   defaultRoute    = "lora";
};

// ── Region ───────────────────────────────────────────────────────────────
struct RegionConfig {
    String  profileId       = "uk";     // matches a RegionalProfile id
    bool    txConfirmed     = false;    // TX disabled until operator confirms
    String  timezone        = "GMT0BST,M3.5.0/1,M10.5.0/2";
};

// ── Root config ──────────────────────────────────────────────────────────
struct Configuration {
    APRSConfig    aprs;
    LoRaConfig    lora;
    BeaconConfig  beacon;
    WiFiConfig    wifi;
    MQTTConfig    mqtt;
    DisplayConfig display;
    MsgConfig     msg;
    RegionConfig  region;
    String        fwVersion = "1.2.0";
};

extern Configuration Config;

bool loadConfig();
bool saveConfig();
void resetConfig();
String  fullCallsign();   // e.g. "2E0LXY-9"
int     calcPasscode(const String& call);
bool    applyRegionProfile(const String& id);  // sets freq/sf/bw/cr/power/server/tz
