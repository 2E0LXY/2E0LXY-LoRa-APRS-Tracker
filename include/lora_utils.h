#pragma once
#include <Arduino.h>

namespace LoRa_Utils {
    bool     setup();
    void     loop();
    bool     sendPacket(const String& payload);
    bool     hasPacket();
    String   getPacket();
    float    lastRSSI();
    float    lastSNR();
    uint32_t getTxCount();
    uint32_t getRxCount();
}
