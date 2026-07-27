#include "touch_utils.h"
#include "board_pins.h"
#include <Wire.h>
#include "TouchDrvGT911.hpp"

namespace {
    TouchDrvGT911 touch;
    bool present = false;

    // Simplified from an earlier down-then-up transition design: that
    // needed to catch a touch-down AND its later touch-up across
    // separate loop() polls, and a real finger tap is often brief enough
    // that the main loop (LoRa/WiFi/display work in between polls) missed
    // one edge or the other entirely — confirmed in testing as
    // "intermittent, but does eventually [register]". Reporting on any
    // detected touch, debounced so a held finger doesn't repeat-fire, is
    // far more forgiving: it only needs to catch the touch during
    // whichever poll happens to land while a finger is down, not two
    // specific polls bracketing the whole gesture.
    const uint32_t TAP_DEBOUNCE_MS = 350;
    uint32_t lastTapMs = 0;
    bool tapPending = false;
    int  tapX = 0, tapY = 0;
}

void Touch_Utils::setup() {
    // Reset pin -1 (not wired independently — shares the board's own
    // reset), interrupt on TOUCH_INT — matches LilyGO's own factory
    // UnitTest.ino exactly (touch.setPins(-1, BOARD_TOUCH_INT)).
    touch.setPins(-1, TOUCH_INT);
    present = touch.begin(Wire, GT911_SLAVE_ADDRESS_L, I2C_SDA, I2C_SCL);
    if (present) {
        Serial.println("Touch: GT911 found");
        touch.setMaxCoordinates(320, 240);
        touch.setSwapXY(true);
        touch.setMirrorXY(false, true);
    } else {
        Serial.println("Touch: GT911 not found (this unit may not have the touch panel fitted)");
    }
}

bool Touch_Utils::isPresent() { return present; }

void Touch_Utils::loop() {
    if (!present) return;

    uint32_t now = millis();
    if (now - lastTapMs < TAP_DEBOUNCE_MS) return;   // still in lockout from the last reported tap

    TouchPoints points = touch.getTouchPoints();
    if (points.hasPoints()) {
        auto& p = points.getPoint(0);
        tapPending = true;
        tapX = p.x;
        tapY = p.y;
        lastTapMs = now;
    }
}

bool Touch_Utils::tapped(int& x, int& y) {
    if (!tapPending) return false;
    tapPending = false;
    x = tapX;
    y = tapY;
    return true;
}
