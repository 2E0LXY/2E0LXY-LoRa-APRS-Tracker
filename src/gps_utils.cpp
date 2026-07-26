#include "gps_utils.h"
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include "board_pins.h"

TinyGPSPlus gps;   // definition of the extern declared in gps_utils.h
static HardwareSerial gpsSerial(1);

GPSData gpsData;

void GPS_Utils::setup() {
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
    Serial.println("GPS: UART initialised");
}

void GPS_Utils::loop() {
    // TEMP diagnostic: print raw bytes from the GPS UART so we can see
    // directly whether anything is arriving at all, independent of
    // whether TinyGPS++ can parse it as valid NMEA.
    static uint32_t rawByteCount = 0;
    static uint32_t lastRawPrintMs = 0;
    while (gpsSerial.available()) {
        int c = gpsSerial.read();
        rawByteCount++;
        gps.encode(c);
    }
    if (millis() - lastRawPrintMs > 5000) {
        lastRawPrintMs = millis();
        Serial.printf("GPS UART: %u bytes received in the last 5s\n", rawByteCount);
        rawByteCount = 0;
    }
    // Satellite count/HDOP update independently of location — these come
    // from $GPGSV/$GPGGA sentences the module sends even with no fix yet,
    // so gating them behind location.isUpdated() left the status bar
    // stuck on "NO GPS 0 sats" indefinitely whenever a fix hadn't been
    // acquired, even if satellites were already in view and counting up.
    if (gps.satellites.isUpdated()) {
        gpsData.sats = gps.satellites.value();
    }
    if (gps.hdop.isUpdated()) {
        gpsData.hdop = gps.hdop.hdop();
    }
    if (gps.location.isUpdated()) {
        gpsData.lat       = gps.location.lat();
        gpsData.lon       = gps.location.lng();
        gpsData.valid     = gps.location.isValid();
        gpsData.altM      = gps.altitude.meters();
        gpsData.speedKph  = gps.speed.kmph();
        gpsData.courseDeg = gps.course.deg();
        gpsData.updatedMs = millis();
    }
}

bool GPS_Utils::hasFix()    { return gpsData.valid; }
float GPS_Utils::lat()      { return gpsData.lat; }
float GPS_Utils::lon()      { return gpsData.lon; }
float GPS_Utils::speedKph() { return gpsData.speedKph; }
float GPS_Utils::courseDeg(){ return gpsData.courseDeg; }
float GPS_Utils::altM()     { return gpsData.altM; }
int   GPS_Utils::sats()     { return gpsData.sats; }
float GPS_Utils::hdop()     { return gpsData.hdop; }

// APRS-format latitude: DDMM.hhN
String GPS_Utils::aprsLat(float lat) {
    bool north = lat >= 0;
    lat = fabs(lat);
    int deg = (int)lat;
    float min = (lat - deg) * 60.0f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d%05.2f%c", deg, min, north ? 'N' : 'S');
    return String(buf);
}

// APRS-format longitude: DDDMM.hhE
String GPS_Utils::aprsLon(float lon) {
    bool east = lon >= 0;
    lon = fabs(lon);
    int deg = (int)lon;
    float min = (lon - deg) * 60.0f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%03d%05.2f%c", deg, min, east ? 'E' : 'W');
    return String(buf);
}

// Degrees between two headings (for corner-pegging SmartBeacon)
float GPS_Utils::headingDelta(float a, float b) {
    float d = fmod(fabs(a - b), 360.0f);
    return d > 180.0f ? 360.0f - d : d;
}
