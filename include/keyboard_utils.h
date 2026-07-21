#pragma once
#include <Arduino.h>

namespace Keyboard_Utils {
    void   setup();
    char   getKey();
    bool   available();
    void   appendToBuffer(char k);
    String getBuffer();
    void   clearBuffer();
    char   lastKeyPressed();
}
