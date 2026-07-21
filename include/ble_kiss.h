#pragma once
#include <Arduino.h>

// BLE KISS TNC — exposes the LoRa radio as a Bluetooth Low Energy KISS TNC
// so phone apps (APRSdroid, YAAC, etc.) can send/receive APRS through the
// T-Deck as a modem. Uses the Nordic UART Service (NUS) UUIDs that APRSdroid
// expects for a "Bluetooth TNC (KISS)" connection.
namespace BLE_KISS {
    void begin();
    void loop();
    bool isConnected();
    // Called by the LoRa receiver to forward a received frame to the phone.
    void sendToPhone(const uint8_t* data, size_t len);
    void sendToPhone(const String& apreqFrame);
}
