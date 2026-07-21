#include "keyboard_utils.h"
#include "board_pins.h"
#include <Wire.h>

static String inputBuffer;
static char   lastKey = 0;
static uint32_t lastPollMs = 0;

// T-Deck keyboard: ESP32-C3 co-processor on I2C 0x55.
// Reading one byte returns the pending keypress, or 0 if none.
// The stock firmware POLLS — the INT line is unreliable across board
// revisions, so we poll at 20ms intervals instead of trusting KB_INT.
static char readKeyRaw() {
    if (Wire.requestFrom((uint8_t)KB_ADDR, (uint8_t)1) != 1) return 0;
    if (!Wire.available()) return 0;
    char k = (char)Wire.read();
    return k;
}

void Keyboard_Utils::setup() {
    Wire.begin(I2C_SDA, I2C_SCL, 400000);
    pinMode(KB_INT, INPUT_PULLUP);
}

char Keyboard_Utils::getKey() {
    uint32_t now = millis();
    if (now - lastPollMs < 20) return 0;  // 50Hz poll cap
    lastPollMs = now;
    char k = readKeyRaw();
    if (k == 0) return 0;
    lastKey = k;
    return k;
}

bool Keyboard_Utils::available() {
    // Poll-based: always allow getKey() to try (it rate-limits itself)
    return true;
}

void Keyboard_Utils::appendToBuffer(char k) {
    if (k == '\b' || k == 0x08 || k == 0x7F) {
        if (inputBuffer.length() > 0)
            inputBuffer.remove(inputBuffer.length() - 1);
    } else if (k == '\r' || k == '\n') {
        // handled by caller
    } else if (k >= 0x20 && k < 0x7F && inputBuffer.length() < 67) {
        inputBuffer += k;   // APRS message max 67 chars
    }
}

String Keyboard_Utils::getBuffer()     { return inputBuffer; }
void   Keyboard_Utils::clearBuffer()   { inputBuffer = ""; }
char   Keyboard_Utils::lastKeyPressed(){ return lastKey; }
