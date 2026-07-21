#include "keyboard_utils.h"
#include "board_pins.h"
#include <Wire.h>

static String inputBuffer;
static char   lastKey = 0;
static bool   altMode = false;   // Alt key held
static bool   symMode = false;   // Sym key held

// Key map for T-Deck keyboard (I2C at 0x55)
// Returns 0 if no key pressed
static char readKeyRaw() {
    Wire.requestFrom((uint8_t)KB_ADDR, (uint8_t)1);
    if (!Wire.available()) return 0;
    return (char)Wire.read();
}

void Keyboard_Utils::setup() {
    Wire.begin(I2C_SDA, I2C_SCL);
    pinMode(KB_INT, INPUT_PULLUP);
}

char Keyboard_Utils::getKey() {
    if (!digitalRead(KB_INT)) return 0; // no interrupt
    char k = readKeyRaw();
    if (k == 0) return 0;
    lastKey = k;
    return k;
}

bool Keyboard_Utils::available() {
    return !digitalRead(KB_INT);
}

void Keyboard_Utils::appendToBuffer(char k) {
    if (k == '\b' || k == 0x7F) {
        if (inputBuffer.length() > 0)
            inputBuffer.remove(inputBuffer.length() - 1);
    } else if (k == '\r' || k == '\n') {
        // handled by caller
    } else if (inputBuffer.length() < 64) {
        inputBuffer += k;
    }
}

String Keyboard_Utils::getBuffer()    { return inputBuffer; }
void   Keyboard_Utils::clearBuffer()  { inputBuffer = ""; }
char   Keyboard_Utils::lastKeyPressed(){ return lastKey; }
