#pragma once
#include <Arduino.h>

namespace OTA_Utils {
    void loop();
    void checkAndUpdate(bool force = false);
}
