#pragma once
#include <Arduino.h>
#include <vector>

// On-device setup screen — edit core settings (callsign, WiFi, TX enable,
// audio) directly on the T-Deck's keyboard, no phone/browser needed. A
// subset of what the web setup portal (webconfig.cpp) offers; the web
// portal remains the place for the full field set (colours, MQTT, weather
// calibration etc.) since a 320x240 screen and 4-row keyboard aren't a
// great fit for everything.
//
// This module owns the field list, selection, and edit-mode state.
// Display_Utils::drawSetupView() (in display_utils.cpp, alongside the
// other draw*View functions) reads that state to render it — this keeps
// the TFT_eSPI/sprite object private to display_utils.cpp, matching how
// every other view is structured.
enum class FieldType { TEXT, PASSWORD, TOGGLE, NUMBER };

struct SetupField {
    String     label;
    FieldType  type;
    String     value;   // current value, as displayed/edited (text form even for numbers/toggles)
};

namespace Setup_View {
    void setup();                // build the field list from Config — call once at boot
    void enter();                 // re-sync field list from Config and reset selection (called when the view opens)
    void handleKey(char key);     // called from main.cpp's handleKeyInput() when in VIEW_SETUP
    const std::vector<SetupField>& fields();
    int  selectedIndex();
    bool isEditing();
    String editBuffer();          // live text being typed, when isEditing()
}

