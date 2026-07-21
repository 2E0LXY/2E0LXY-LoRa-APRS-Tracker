#pragma once
#include <Arduino.h>

namespace MQTT_Utils {
    bool connect();
    void loop();
    void publishTelemetry();
    bool isConnected();
}
