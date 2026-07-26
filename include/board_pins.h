#pragma once
// LilyGO T-Deck Plus — hardware pin assignments

// ── Firmware version (single source of truth) ───────────────────────────
#define FW_VERSION "1.6.0"
// Source: Xinyuan-LilyGO/T-Deck utilities.h

// ── Power ────────────────────────────────────────────────────────────────
#define BOARD_POWERON       10   // HIGH to enable peripheral rail

// ── SPI bus (shared: TFT + SD + LoRa) ───────────────────────────────────
#define BOARD_SPI_MOSI      41
#define BOARD_SPI_MISO      38
#define BOARD_SPI_SCK       40

// ── TFT ST7789 320×240 ───────────────────────────────────────────────────
#define BOARD_TFT_CS        12
#define BOARD_TFT_DC        11
#define BOARD_TFT_BL        42   // Backlight PWM

// ── MicroSD ──────────────────────────────────────────────────────────────
#define BOARD_SD_CS         39

// ── LoRa SX1262 ──────────────────────────────────────────────────────────
#define LORA_CS             9
#define LORA_IRQ            45
#define LORA_RST            17
#define LORA_BUSY           13

// ── GPS L76K ─────────────────────────────────────────────────────────────
#define GPS_TX              43   // ESP32 TX → GPS RX
#define GPS_RX              44   // GPS TX → ESP32 RX
#define GPS_BAUD            9600

// ── I2C (keyboard, trackball, touch) ─────────────────────────────────────
#define I2C_SDA             18
#define I2C_SCL             8
#define KB_ADDR             0x55
#define KB_INT              46
#define TOUCH_INT           16

// ── Trackball (GPIO) ─────────────────────────────────────────────────────
#define TBOX_UP             2    // BOARD_TBOX_G02
#define TBOX_DOWN           15   // BOARD_TBOX_G03
#define TBOX_LEFT           3    // BOARD_TBOX_G01
#define TBOX_RIGHT          1    // BOARD_TBOX_G04
// Click is detected via keyboard I2C

// ── Battery ADC ──────────────────────────────────────────────────────────
#define BOARD_BAT_ADC       4
#define BAT_FULL_MV         4200
#define BAT_EMPTY_MV        3300

// ── Speaker / Audio ──────────────────────────────────────────────────────
#define I2S_WS              5
#define I2S_BCK             7
#define I2S_DOUT            6

// ── LoRa RF parameters (UK LoRa APRS) ───────────────────────────────────
#define LORA_FREQ           439.9125
#define LORA_SF             12
#define LORA_BW             125.0
#define LORA_CR             5      // 4/5
#define LORA_PREAMBLE       8
#define LORA_TX_POWER       17     // dBm default

// ── APRS-IS (aprsnet.uk) ─────────────────────────────────────────────────
#define APRSIS_HOST         "www.aprsnet.uk"
#define APRSIS_PORT         14580

// ── aprsnet.uk MQTT ──────────────────────────────────────────────────────
#define MQTT_HOST           "80.64.216.113"   // direct IP — Cloudflare bypassed
#define MQTT_PORT           1883
#define MQTT_TOPIC_BASE     "aprsnet"

// ── Display ──────────────────────────────────────────────────────────────
// NOTE: named SCREEN_* (not TFT_WIDTH/TFT_HEIGHT) — those macro names are
// owned by TFT_eSPI itself (set via platformio.ini build_flags as the
// panel's native portrait size, 240x320) and get resolved/rotated
// internally by the library. Reusing the same names here for the app's
// post-rotation landscape size collided with TFT_eSPI's own definition
// depending on header include order, leaving tft.width()/height() stuck
// at the un-rotated portrait values (240x320 instead of 320x240) and
// causing the app to draw UI past the addressable screen area.
#define SCREEN_WIDTH         320
#define SCREEN_HEIGHT        240
