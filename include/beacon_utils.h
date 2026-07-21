#pragma once
#include <Arduino.h>

namespace Beacon_Utils {
    bool     shouldBeacon();
    void     sendBeacon();
    void     loop();
    uint32_t getCount();
    uint32_t lastMs();
}
