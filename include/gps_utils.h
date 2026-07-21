#pragma once
#include <Arduino.h>

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
    String aprsLat(float lat);
    String aprsLon(float lon);
    float  headingDelta(float a, float b);
}
