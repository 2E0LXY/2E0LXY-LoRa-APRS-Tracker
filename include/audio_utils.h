#pragma once
#include <Arduino.h>

// Notification sound over the T-Deck Plus's I2S speaker. Generates a short
// tone in software (no sample files needed) — used for incoming APRS
// messages. Enable/disable and volume are user-configurable (Config.audio,
// exposed in the web setup portal).
namespace Audio_Utils {
    void setup();
    // Plays the message-received notification tone (non-blocking: kicks off
    // I2S playback and returns — Audio_Utils::loop() feeds the buffer).
    void playMessageTone();
    void loop();
}
