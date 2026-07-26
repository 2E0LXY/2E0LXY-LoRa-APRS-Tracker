#include "trackball_utils.h"
#include "board_pins.h"

// Trackball reading. Earlier self-calibrating-baseline attempt had a
// feedback bug: the baseline only updated on non-triggering windows, so
// once it drifted wrong (or started from a bad seed) every subsequent
// window falsely "deviated" from it forever, latching onto one direction
// permanently regardless of actual input (confirmed in testing — see the
// "Trackball dir: D" spam that didn't correspond to the operator's actual
// R/L/U/D sequence). This version always updates the baseline, and
// additionally requires the winning count to be a genuine local spike —
// not just any deviation — by comparing against the count from several
// windows ago rather than a slow-moving average, so a sustained bad
// reading can't anchor the baseline against itself.
namespace {
    const uint32_t DIR_DEBOUNCE_MS  = 400;
    const uint32_t ACCUM_WINDOW_MS  = 60;
    const int      HISTORY_LEN      = 5;     // compare current window against this many windows back
    const int      SPIKE_MIN        = 80;    // current count must exceed the older reference by at least this
    const int      MIN_MARGIN       = 40;    // winner's spike must beat the runner-up's by at least this much
    const uint32_t CLICK_DEBOUNCE_MS = 250;

    uint32_t accumStart = 0;
    int upCount = 0, downCount = 0, leftCount = 0, rightCount = 0;
    char pendingDir = 0;

    // Ring buffers of recent per-pin counts, so "spike" is judged against
    // where each pin actually was a few windows ago, not a value that can
    // itself be corrupted by an ongoing false trigger.
    int histUp[HISTORY_LEN] = {0}, histDown[HISTORY_LEN] = {0};
    int histLeft[HISTORY_LEN] = {0}, histRight[HISTORY_LEN] = {0};
    int histIdx = 0;
    int windowsSeen = 0;

    uint32_t lastDirMs = 0;
    uint32_t lastClickMs = 0;
    bool lastClickState = false;
    bool clickEventPending = false;

    void resetAccum(uint32_t now) {
        accumStart = now;
        upCount = downCount = leftCount = rightCount = 0;
    }

    char resolveWinner() {
        int oldIdx = histIdx;   // oldest entry in the ring, about to be overwritten this window
        int spikes[4] = {
            upCount    - histUp[oldIdx],
            downCount  - histDown[oldIdx],
            leftCount  - histLeft[oldIdx],
            rightCount - histRight[oldIdx],
        };
        char labels[4] = { 'U', 'D', 'L', 'R' };
        int bestIdx = 0;
        for (int i = 1; i < 4; i++) if (spikes[i] > spikes[bestIdx]) bestIdx = i;
        if (spikes[bestIdx] < SPIKE_MIN) return 0;
        int runnerUp = -100000;
        for (int i = 0; i < 4; i++) if (i != bestIdx && spikes[i] > runnerUp) runnerUp = spikes[i];
        if (spikes[bestIdx] - runnerUp < MIN_MARGIN) return 0;
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
        char winner = 0;
        if (windowsSeen >= HISTORY_LEN && now - lastDirMs >= DIR_DEBOUNCE_MS) {
            winner = resolveWinner();
        }
        if (winner) {
            pendingDir = winner;
            lastDirMs = now;
        }
        // History always updates, win or not — this is the actual fix
        // for the runaway-lock bug: a bad reading ages out of the
        // comparison window after HISTORY_LEN cycles regardless of
        // whether it kept "winning" in the meantime.
        histUp[histIdx] = upCount;
        histDown[histIdx] = downCount;
        histLeft[histIdx] = leftCount;
        histRight[histIdx] = rightCount;
        histIdx = (histIdx + 1) % HISTORY_LEN;
        windowsSeen++;
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
