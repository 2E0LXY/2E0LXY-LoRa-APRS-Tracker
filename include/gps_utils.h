#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>

// Shared TinyGPSPlus instance — satellites_utils.cpp attaches TinyGPSCustom
// watchers to this same object to pull per-satellite GSV data, rather than
// running a second parser against the same UART stream.
extern TinyGPSPlus gps;

struct GPSData {
    float  lat       = 0, lon = 0;
    float  altM      = 0;
    float  speedKph  = 0;
    float  courseDeg = 0;
    int    sats      = 0;
    float  hdop      = 99;
    bool   valid     = false;
    uint32_t updatedMs = 0;
};
extern GPSData gpsData;

enum class GpsModule : uint8_t { UNKNOWN, L76K, UBLOX };

namespace GPS_Utils {
    void   setup();
    void   loop();
    bool   hasFix();
    float  lat();
    float  lon();
    float  speedKph();
    float  courseDeg();
    float  altM();
    int    sats();
    float  hdop();
    GpsModule detectedModule();
    const char* moduleName();
    String aprsLat(float lat);
    String aprsLon(float lon);
    float  headingDelta(float a, float b);
}
