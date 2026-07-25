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

enum DisplayView { VIEW_STATUS, VIEW_STATIONS, VIEW_MESSAGES, VIEW_MAP, VIEW_SATS, VIEW_SETUP };

namespace Display_Utils {
    void        setup();
    void        loop();
    void        setView(DisplayView v);
    DisplayView getView();
    void        setBrightness(int v);
    void        showMessage(const String& title, const String& body, uint16_t colour = 0x07FF);
    float       batteryVolts();
    int         batteryPercent();

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
