#pragma once
#include <Arduino.h>

// Optional I2C BME280 weather sensor. Auto-detected on the T-Deck I2C bus
// (shared with keyboard/trackball) at 0x76 or 0x77. When present and enabled,
// the tracker can transmit APRS weather beacons.
namespace Weather_Utils {
    bool   setup();            // returns true if a BME280 was found
    void   loop();             // periodic WX beacon if enabled
    bool   available();
    float  temperatureC();
    float  humidity();
    float  pressureHpa();
    String buildWxPacket();    // APRS positionless WX report
}
