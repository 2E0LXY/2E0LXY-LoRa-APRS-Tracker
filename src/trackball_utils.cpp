#include "trackball_utils.h"
#include "board_pins.h"

// Trackball reading — simple majority vote per accumulation window
// ("fuzzy logic": whichever direction had the most low-pin readings
// during the window wins), per explicit request after several more
// elaborate filtering strategies (fixed thresholds, self-calibrating
// baselines, spike-vs-history comparison) each hit their own failure
// mode. This is deliberately the simplest possible version: no minimum
// count, no margin-over-runner-up requirement, just "most votes wins".
namespace {
    const uint32_t DIR_DEBOUNCE_MS  = 400;   // lockout after a reported event
    const uint32_t ACCUM_WINDOW_MS  = 80;    // how long to count low-pin readings per cycle
    const uint32_t CLICK_DEBOUNCE_MS = 250;

    uint32_t accumStart = 0;
    int upCount = 0, downCount = 0, leftCount = 0, rightCount = 0;
    char pendingDir = 0;

    uint32_t lastDirMs = 0;
    uint32_t lastClickMs = 0;
    bool lastClickState = false;
    bool clickEventPending = false;

    void resetAccum(uint32_t now) {
        accumStart = now;
        upCount = downCount = leftCount = rightCount = 0;
    }

    // Simple majority: whichever pin was low most often this window
    // wins, provided at least one reading happened at all (all-zero
    // windows — the ball wasn't touched — report nothing).
    char resolveWinner() {
        int counts[4] = { upCount, downCount, leftCount, rightCount };
        char labels[4] = { 'U', 'D', 'L', 'R' };
        int bestIdx = 0;
        for (int i = 1; i < 4; i++) if (counts[i] > counts[bestIdx]) bestIdx = i;
        if (counts[bestIdx] == 0) return 0;
        return labels[bestIdx];
    }
}

void Trackball_Utils::setup() {
    pinMode(TBOX_UP, INPUT_PULLUP);
    pinMode(TBOX_DOWN, INPUT_PULLUP);
    pinMode(TBOX_LEFT, INPUT_PULLUP);
    pinMode(TBOX_RIGHT, INPUT_PULLUP);
    pinMode(0, INPUT_PULLUP);
    lastClickState = (digitalRead(0) == LOW);
    resetAccum(millis());
}

void Trackball_Utils::loop() {
    uint32_t now = millis();

    bool pressed = (digitalRead(0) == LOW);
    if (pressed && !lastClickState && (now - lastClickMs) > CLICK_DEBOUNCE_MS) {
        clickEventPending = true;
        lastClickMs = now;
    }
    lastClickState = pressed;

    if (digitalRead(TBOX_UP)    == LOW) upCount++;
    if (digitalRead(TBOX_DOWN)  == LOW) downCount++;
    if (digitalRead(TBOX_LEFT)  == LOW) leftCount++;
    if (digitalRead(TBOX_RIGHT) == LOW) rightCount++;

    if (now - accumStart >= ACCUM_WINDOW_MS) {
        if (now - lastDirMs >= DIR_DEBOUNCE_MS) {
            char winner = resolveWinner();
            if (winner) {
                pendingDir = winner;
                lastDirMs = now;
            }
        }
        resetAccum(now);
    }
}

char Trackball_Utils::getDirection() {
    char d = pendingDir;
    pendingDir = 0;
    return d;
}

bool Trackball_Utils::clickPressed() {
    if (!clickEventPending) return false;
    clickEventPending = false;
    return true;
}
