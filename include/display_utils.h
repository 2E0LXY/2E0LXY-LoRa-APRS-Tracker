#pragma once
#include <Arduino.h>

enum DisplayView { VIEW_STATUS, VIEW_STATIONS, VIEW_MESSAGES, VIEW_MAP };

namespace Display_Utils {
    void        setup();
    void        loop();
    void        setView(DisplayView v);
    DisplayView getView();
    void        setBrightness(int v);
    void        showMessage(const String& title, const String& body, uint16_t colour = 0x07FF);
}
