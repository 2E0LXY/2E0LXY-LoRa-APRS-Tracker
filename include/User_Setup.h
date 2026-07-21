// TFT_eSPI user setup — LilyGO T-Deck Plus ST7789 320×240
// This file must be copied to the TFT_eSPI library directory OR
// referenced via build_flags: -DUSER_SETUP_LOADED and placed in include/

#pragma once

#define ST7789_DRIVER
#define TFT_WIDTH   320
#define TFT_HEIGHT  240

#define TFT_MOSI    41
#define TFT_SCLK    40
#define TFT_CS      12
#define TFT_DC      11
#define TFT_RST     -1
#define TFT_BL      42

#define TFT_BACKLIGHT_ON  HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
