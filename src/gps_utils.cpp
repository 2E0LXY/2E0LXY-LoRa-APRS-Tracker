#include "gps_utils.h"
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include "board_pins.h"

TinyGPSPlus gps;   // definition of the extern declared in gps_utils.h
static HardwareSerial gpsSerial(1);
static GpsModule gpsModule = GpsModule::UNKNOWN;

GPSData gpsData;

void GPS_Utils::setup() {
    // ── Phase 1: passive probe at the u-blox rate ───────────────────────
    // A real u-blox M10Q streams NMEA on its own the moment it's powered —
    // no config needed. An L76K, per this firmware's own testing, stays
    // silent until actively configured, and is on the wrong baud here
    // besides. So: listen passively for a short window; if we see several
    // genuinely checksum-valid sentences, it's u-blox and we're already
    // done — no commands to send, nothing more to configure. Getting 3+
    // coincidentally-valid NMEA checksums out of line noise in under two
    // seconds is astronomically unlikely, so this threshold is a safe
    // positive signal rather than a fluke.
    gpsSerial.begin(GPS_UBLOX_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
    uint32_t probeStart = millis();
    while (millis() - probeStart < 1500) {
        while (gpsSerial.available()) gps.encode(gpsSerial.read());
    }

    if (gps.passedChecksum() >= 3) {
        gpsModule = GpsModule::UBLOX;
        Serial.printf("GPS: u-blox module detected (%d baud, %lu sentences OK) — no config needed\n",
                      GPS_UBLOX_BAUD, gps.passedChecksum());
        return;
    }

    // ── Phase 2: fall back to the L76K probe/config path ────────────────
    // L76K init sequence, matching LilyGO's own factory UnitTest firmware
    // (examples/UnitTest/UnitTest.ino, setupGPS()) exactly: sends $PCAS03
    // to stop/reset NMEA output, $PCAS06 to read firmware info and
    // confirm an L76K responded, $PCAS04 to enable GPS+GLONASS, a second
    // $PCAS03 to enable every NMEA sentence type, and $PCAS11 for vehicle
    // dynamics mode.
    gpsSerial.end();
    gpsSerial.begin(GPS_L76K_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

    bool l76kConfirmed = false;
    for (int i = 0; i < 3 && !l76kConfirmed; i++) {
        gpsSerial.write("$PCAS03,0,0,0,0,0,0,0,0,0,0,,,0,0*02\r\n");
        delay(5);
        uint32_t stopTimeout = millis() + 3000;
        while (gpsSerial.available()) {
            gpsSerial.read();
            if (millis() > stopTimeout) break;
        }
        gpsSerial.flush();
        delay(200);
        gpsSerial.write("$PCAS06,0*1B\r\n");
        uint32_t verTimeout = millis() + 500;
        while (!gpsSerial.available() && millis() < verTimeout) { /* wait */ }
        gpsSerial.setTimeout(10);
        String ver = gpsSerial.readStringUntil('\n');
        if (ver.startsWith("$GPTXT,01,01,02")) {
            l76kConfirmed = true;
            Serial.println("GPS: L76K confirmed via $PCAS06 version query");
        } else {
            delay(500);
        }
    }
    gpsModule = l76kConfirmed ? GpsModule::L76K : GpsModule::UNKNOWN;
    if (!l76kConfirmed) {
        Serial.println("GPS: neither u-blox nor L76K confirmed — configuring for L76K anyway (module may still respond even if both probes missed)");
    }

    // GPS + GLONASS
    gpsSerial.write("$PCAS04,5*1C\r\n");
    delay(250);
    // Enable every NMEA sentence type (matches the factory default —
    // more than we strictly need, but TinyGPS++ only reads what it
    // recognises and ignores the rest)
    gpsSerial.write("$PCAS03,1,1,1,1,1,1,1,1,1,1,,,0,0*02\r\n");
    delay(250);
    // Vehicle dynamics mode
    gpsSerial.write("$PCAS11,3*1E\r\n");

    Serial.printf("GPS: UART initialised (%d baud, %s)\n", GPS_L76K_BAUD, GPS_Utils::moduleName());
}

void GPS_Utils::loop() {
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
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
GpsModule GPS_Utils::detectedModule() { return gpsModule; }
const char* GPS_Utils::moduleName() {
    switch (gpsModule) {
        case GpsModule::UBLOX: return "u-blox";
        case GpsModule::L76K:  return "L76K";
        default:                return "unknown";
    }
}

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
