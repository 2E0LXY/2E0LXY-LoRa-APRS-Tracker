#pragma once
#include <Arduino.h>

// Web-based configuration portal. On boot (or on demand via the keyboard),
// the tracker can bring up a WiFi access point and serve a full settings
// page covering every option: callsign/SSID, region preset dropdown,
// frequency/SF/BW/CR/power, SmartBeacon timings, beacon-now, WiFi, MQTT,
// message colours + route, and display. Mirrors the iGate's WebUI approach.
namespace WebConfig {
    void begin();      // start AP + web server
    void loop();       // handle clients
    bool isRunning();
    void stop();
}
