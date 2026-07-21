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

enum DisplayView { VIEW_STATUS, VIEW_STATIONS, VIEW_MESSAGES, VIEW_MAP };

namespace Display_Utils {
    void        setup();
    void        loop();
    void        setView(DisplayView v);
    DisplayView getView();
    void        setBrightness(int v);
    void        showMessage(const String& title, const String& body, uint16_t colour = 0x07FF);
}
