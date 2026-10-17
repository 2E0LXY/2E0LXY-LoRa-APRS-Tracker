#pragma once
#include <stdint.h>
#include <string.h>
#include "aprs_icons.h"

// Official APRS symbol composition for the 16x16 TFT icons. No Arduino dependencies beyond the
// icon tables, so it is unit-tested on a host compiler (test/host/test_symbol_compose.cpp).
//
// The table character of a symbol is:
//   '/'              primary table
//   '\\'             alternate table
//   'A'-'Z','0'-'9'  alternate table + that overlay glyph
//   'a'-'j'          alternate table + overlay '0'-'9' (compressed-position encoding)
// Anything invalid renders the official primary-table dot ("//") instead of nothing.
namespace AprsSymbolCompose {

    // Fills out[256] with the icon for (table, code). Returns true if the symbol was valid.
    inline bool compose(char table, char code, uint16_t* out) {
        int idx = (int)code - 33;
        bool valid = true;
        const uint16_t* base = nullptr;
        int overlay = -1;

        if (idx < 0 || idx >= APRS_ICON_COUNT) {
            valid = false;
        } else if (table == '/') {
            base = APRS_ICONS_PRIMARY[idx];
        } else if (table == '\\') {
            base = APRS_ICONS_ALT[idx];
        } else if (table >= '0' && table <= '9') {
            base = APRS_ICONS_ALT[idx]; overlay = table - '0';
        } else if (table >= 'A' && table <= 'Z') {
            base = APRS_ICONS_ALT[idx]; overlay = 10 + (table - 'A');
        } else if (table >= 'a' && table <= 'j') {
            base = APRS_ICONS_ALT[idx]; overlay = table - 'a';
        } else {
            valid = false;
        }

        if (!valid) {
            memcpy(out, APRS_ICONS_PRIMARY['/' - 33], 256 * sizeof(uint16_t));   // official dot
            return false;
        }
        memcpy(out, base, 256 * sizeof(uint16_t));
        if (overlay >= 0 && overlay < APRS_OVERLAY_COUNT) {
            const uint16_t* o = APRS_ICONS_OVERLAY[overlay];
            for (int i = 0; i < 256; i++) if (o[i] != APRS_ICON_TRANSPARENT) out[i] = o[i];
        }
        return true;
    }
}
