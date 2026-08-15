#include "keyboard_utils.h"
#include "board_pins.h"
#include <Wire.h>

static String inputBuffer;
static char   lastKey = 0;
static uint32_t lastPollMs = 0;
static uint32_t settleUntilMs = 0;   // ignore reads until this time — see setup()

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
    // Ignore reads for the first 500ms — the keyboard co-processor may
    // not have its I2C register in a clean idle state (returning 0)
    // immediately after this MCU powers it up, and a stray byte that
    // happened to land on a bound navigation key (e.g. 't'/'2' for
    // Stations) could silently fire a real, valid-looking keypress with
    // no trace it wasn't genuine — a very plausible explanation for a
    // home screen that reproducibly failed to render even under a
    // forced, unconditional setView(VIEW_HOME) immediately before the
    // first draw call.
    settleUntilMs = millis() + 500;
}

char Keyboard_Utils::getKey() {
    uint32_t now = millis();
    if (now < settleUntilMs) return 0;
    if (now - lastPollMs < 20) return 0;  // 50Hz poll cap
    lastPollMs = now;
    char k = readKeyRaw();
    if (k == 0) return 0;
    // Reject anything that isn't a plausible key: printable ASCII, or
    // one of the specific control codes this firmware actually treats
    // as a key (Enter/CR/LF, Backspace/DEL). The I2C keyboard register
    // has no guarantee of returning a clean 0 when idle — especially
    // early in boot, before the co-processor has fully initialised —
    // and a stray garbage byte that happened to decode as a bound
    // navigation key (e.g. 't'/'2' for Stations) would silently
    // navigate away from whatever view was meant to show, with no
    // trace of a real keypress ever happening. This was very likely
    // the actual explanation for a home screen that reproducibly never
    // rendered even under a forced, unconditional override immediately
    // before the first draw — something between that override and the
    // draw was still successfully calling setView() with a real
    // (bogus) key value.
    bool printable = (k >= 0x20 && k < 0x7F);
    bool controlOk = (k == '\r' || k == '\n' || k == '\b' || k == 0x08 || k == 0x7F);
    if (!printable && !controlOk) return 0;
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
