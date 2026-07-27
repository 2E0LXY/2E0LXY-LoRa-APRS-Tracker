#pragma once
#include <Arduino.h>

// GT911 capacitive touch (T-Deck Plus). Confirmed present and working via
// LilyGO's own factory UnitTest firmware (examples/UnitTest/UnitTest.ino):
// TouchDrvGT911 on the shared I2C bus (SDA=18, SCL=8), interrupt on
// TOUCH_INT (GPIO16), 320x240 coordinate space with X/Y swapped and Y
// mirrored to match this panel's rotation. Some units may genuinely lack
// the touch controller (LilyGO's own factory code handles that case
// explicitly — "Failed to find GT911 - check your wiring!" is a normal,
// handled outcome, not an error) — Touch_Utils reports that via
// isPresent() rather than assuming success.
namespace Touch_Utils {
    void setup();
    bool isPresent();

    // True for exactly one loop() call when a tap is detected (touch
    // down then up, not a drag) — mirrors Trackball_Utils::clickPressed()
    // in shape. x/y are the screen-space coordinates (0-319, 0-239,
    // already rotation-corrected) of that tap.
    bool tapped(int& x, int& y);

    void loop();
}
