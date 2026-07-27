#include "touch_utils.h"
#include "board_pins.h"
#include <Wire.h>
#include "TouchDrvGT911.hpp"

namespace {
    TouchDrvGT911 touch;
    bool present = false;

    // Debounced tap detection: a "tap" is touch-down followed by touch-up
    // without much movement, not a drag. We track whether we were
    // touching last loop() and report a tap on the down->up transition.
    bool wasTouching = false;
    int  downX = 0, downY = 0;
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

    TouchPoints points = touch.getTouchPoints();
    bool touching = points.hasPoints();

    if (touching && !wasTouching) {
        // Touch-down: record where it started.
        auto& p = points.getPoint(0);
        downX = p.x;
        downY = p.y;
    } else if (!touching && wasTouching) {
        // Touch-up: if it didn't move far from where it started, count
        // it as a tap. (downX/downY are from the last-seen down point;
        // since we don't have the up coordinate once released, this is
        // deliberately lenient — a real drag gesture isn't something the
        // icon grid needs to distinguish from a tap at this stage.)
        tapPending = true;
        tapX = downX;
        tapY = downY;
    }
    wasTouching = touching;
}

bool Touch_Utils::tapped(int& x, int& y) {
    if (!tapPending) return false;
    tapPending = false;
    x = tapX;
    y = tapY;
    return true;
}
