#pragma once
#include <Arduino.h>

// LilyGO T-Deck Plus trackball — 4 quadrature-style direction pins
// (TBOX_UP/DOWN/LEFT/RIGHT) plus a centre-press button on GPIO0 (shared
// with BOOT — safe to read as a plain digital input during normal
// runtime; the bootloader-entry meaning only applies at power-on/reset).
// See board_pins.h for the actual pin numbers.
namespace Trackball_Utils {
    void setup();
    void loop();

    // Consumes and returns the pending direction event, or 0 if none.
    // Values: 'U'/'D'/'L'/'R'. Debounced + rate-limited internally so a
    // held direction repeats at a controlled rate rather than flooding.
    char getDirection();

    // True for exactly one loop() call when the centre button is
    // pressed (debounced). Treat like a keypress — e.g. map it to Enter
    // in the caller, same as this session's on-device Setup/Map views
    // already do for other actions.
    bool clickPressed();
}
