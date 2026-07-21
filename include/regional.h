#pragma once
#include <Arduino.h>

// ── Regional LoRa APRS presets ────────────────────────────────────────────
// Presets are STARTING POINTS ONLY. The licensed operator remains
// responsible for national regulations, band plan, local coordination,
// antenna gain and permitted power. Selecting a region sets frequency,
// SF/BW/CR, default power, APRS-IS server and beacon path — but TX stays
// OFF until the operator confirms the profile (safety, mirrors the iGate).

struct RegionalProfile {
    const char* id;
    const char* name;
    const char* countryCode;
    float       freqMHz;        // RX = TX for LoRa APRS
    int         sf;
    float       bwKHz;
    int         cr;             // 4/x → store x
    int         powerDbm;
    const char* aprsIsServer;
    int         aprsIsPort;
    const char* timezone;       // POSIX TZ rule
    const char* beaconPath;
};

// The canonical LoRa APRS frequencies by region. Sources: IARU R1 VHF
// Handbook, national society band plans, and the community lora-aprs
// frequency list. 70cm (433/439) is the dominant LoRa APRS band in
// Region 1; 2m (144.39) is used in North America.
static const RegionalProfile REGIONAL_PROFILES[] = {
    // id            name                          cc     freq       sf  bw     cr  pwr  server              port    tz                                          path
    { "uk",          "United Kingdom",             "GB",  439.9125f, 12, 125.0f, 5, 10, "www.aprsnet.uk",   14580, "GMT0BST,M3.5.0/1,M10.5.0/2",              "WIDE1-1" },
    { "iaru1",       "IARU R1 common (70cm)",      "",    433.775f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "eu_433",      "Europe 433.775",             "",    433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "de",          "Germany 433.775",            "DE",  433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "nl",          "Netherlands 433.775",        "NL",  433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "fr",          "France 433.775",             "FR",  433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "it",          "Italy 433.775",              "IT",  433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "es",          "Spain 433.775",              "ES",  433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "pl",          "Poland 433.775",             "PL",  433.775f,  12, 125.0f, 5, 10, "euro.aprs2.net",   14580, "CET-1CEST,M3.5.0/2,M10.5.0/3",            "WIDE1-1" },
    { "us",          "USA / Canada (2m)",          "US",  144.390f,  12, 125.0f, 5, 10, "noam.aprs2.net",   14580, "EST5EDT,M3.2.0,M11.1.0",                  "WIDE1-1,WIDE2-1" },
    { "us_433",      "USA 433.775 (LoRa)",         "US",  433.775f,  12, 125.0f, 5, 10, "noam.aprs2.net",   14580, "EST5EDT,M3.2.0,M11.1.0",                  "WIDE1-1,WIDE2-1" },
    { "au",          "Australia 433.775",          "AU",  433.775f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "AEST-10AEDT,M10.1.0,M4.1.0/3",            "WIDE1-1,WIDE2-1" },
    { "nz",          "New Zealand 433.775",        "NZ",  433.775f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "NZST-12NZDT,M9.5.0,M4.1.0/3",             "WIDE1-1,WIDE2-1" },
    { "za",          "South Africa 433.775",       "ZA",  433.775f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "SAST-2",                                  "WIDE1-1" },
    { "jp",          "Japan 431.020",              "JP",  431.020f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "JST-9",                                   "WIDE1-1,WIDE2-1" },
    { "br",          "Brazil 433.775",             "BR",  433.775f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "BRT3",                                    "WIDE1-1,WIDE2-1" },
    { "custom",      "Custom / manual",            "",    433.775f,  12, 125.0f, 5, 10, "rotate.aprs2.net", 14580, "UTC0",                                    "WIDE1-1" },
};

static const int REGIONAL_PROFILE_COUNT =
    sizeof(REGIONAL_PROFILES) / sizeof(REGIONAL_PROFILES[0]);

// Look up a profile by id; returns nullptr if not found.
inline const RegionalProfile* findProfile(const String& id) {
    for (int i = 0; i < REGIONAL_PROFILE_COUNT; i++) {
        if (id.equalsIgnoreCase(REGIONAL_PROFILES[i].id)) {
            return &REGIONAL_PROFILES[i];
        }
    }
    return nullptr;
}
