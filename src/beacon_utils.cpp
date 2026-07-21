#include "beacon_utils.h"
#include "configuration.h"
#include "gps_utils.h"
#include "aprs_utils.h"
#include "lora_utils.h"

static uint32_t lastBeaconMs    = 0;
static float    lastBeaconLat   = 0, lastBeaconLon = 0;
static float    lastCourse      = 0;
static uint32_t beaconCount     = 0;

bool Beacon_Utils::shouldBeacon() {
    if (!GPS_Utils::hasFix()) return false;

    uint32_t now = millis();
    float speed  = GPS_Utils::speedKph();
    float course = GPS_Utils::courseDeg();

    if (!Config.beacon.smartEnabled) {
        return (now - lastBeaconMs) >= (uint32_t)(Config.beacon.slowRate * 1000);
    }

    // Determine interval based on speed
    uint32_t interval;
    if (speed < Config.beacon.speedThreshold) {
        interval = Config.beacon.slowRate * 1000;
    } else {
        interval = Config.beacon.fastRate * 1000;
    }

    // Corner pegging — significant turn triggers immediate beacon
    float turnDelta = GPS_Utils::headingDelta(course, lastCourse);
    if (turnDelta >= Config.beacon.turnAngle && (now - lastBeaconMs) >= 5000) {
        return true;
    }

    // Minimum distance trigger
    if (lastBeaconLat != 0 || lastBeaconLon != 0) {
        float d = APRS_Utils::distanceKm(lastBeaconLat, lastBeaconLon,
                                          GPS_Utils::lat(), GPS_Utils::lon()) * 1000.0f;
        if (d >= Config.beacon.minDistance && (now - lastBeaconMs) >= 10000) {
            return true;
        }
    }

    return (now - lastBeaconMs) >= interval;
}

void Beacon_Utils::sendBeacon() {
    String packet = APRS_Utils::buildPositionPacket();
    bool sent = false;

    if (Config.beacon.loraEnabled) {
        sent = LoRa_Utils::sendPacket(packet);
    }
    if (Config.beacon.aprsIsEnabled && APRS_Utils::isConnected()) {
        APRS_Utils::sendPacketIS(packet);
        sent = true;
    }

    if (sent) {
        lastBeaconMs  = millis();
        lastBeaconLat = GPS_Utils::lat();
        lastBeaconLon = GPS_Utils::lon();
        lastCourse    = GPS_Utils::courseDeg();
        beaconCount++;
        Serial.println("Beacon TX: " + packet);
    }
}

void Beacon_Utils::loop() {
    if (shouldBeacon()) {
        sendBeacon();
    }
}

uint32_t Beacon_Utils::getCount() { return beaconCount; }
uint32_t Beacon_Utils::lastMs()   { return lastBeaconMs; }
