#pragma once
#include <Arduino.h>

// TFT colour constants (16-bit RGB565)
#define TFT_BLACK   0x0000
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_CYAN    0x07FF
#define TFT_MAGENTA 0xF81F
#define TFT_YELLOW  0xFFE0
#define TFT_WHITE   0xFFFF
#define TFT_ORANGE  0xFBE0
#define TFT_PURPLE  0x780F

enum DisplayView { VIEW_HOME, VIEW_STATUS, VIEW_STATIONS, VIEW_MESSAGES, VIEW_MAP, VIEW_SATS, VIEW_SETUP };

namespace Display_Utils {
    void        setup();
    void        loop();
    void        setView(DisplayView v);
    DisplayView getView();
    void        setBrightness(int v);
    void        showMessage(const String& title, const String& body, uint16_t colour = 0x07FF);
    float       batteryVolts();
    int         batteryPercent();

    // ── Home screen (icon grid) navigation ────────────────────────────
    // Called from main.cpp's key handler when VIEW_HOME is active — moves
    // the highlighted tile, or activates it (either switching to that
    // tile's view, or running its action for the two action-only tiles
    // handled directly in main.cpp: WiFi portal toggle, Beacon Now).
    void homeMove(int dCol, int dRow);
    // Returns the currently-selected tile's index and label, so
    // main.cpp can decide what "activate" means for tiles that aren't a
    // plain view switch (WiFi/Beacon).
    int         homeSelectedIndex();
    const char* homeSelectedLabel();
    // Converts a screen tap (x,y) directly to a tile index and selects
    // it, so a touch on the home grid picks the tile under the finger
    // immediately rather than needing separate move-then-confirm steps
    // like keyboard/trackball navigation. Returns false if the tap
    // landed outside the tile grid (e.g. on the status bar).
    bool        homeSelectAt(int x, int y);

    // ── Map view helpers (used by map_utils.cpp) ──────────────────────
    // Keep the TFT_eSPI object private to display_utils.cpp; the map
    // module only needs these narrow drawing primitives, not the full
    // TFT_eSPI API.
    void mapClearArea(int top, int height);
    void mapPushTileLine(int x, int y, int w, uint16_t* line);
    void mapPushIcon(int x, int y, const uint16_t* iconRGB565, int size, uint16_t transparentColour);
    void mapDrawCrosshair(int x, int y);
    void mapDrawFooter(int y, int zoomLevel);
    void mapDrawNoCard();
}
