#include "lora_utils.h"
#include "board_pins.h"
#include "configuration.h"
#include <RadioLib.h>
#include <TFT_eSPI.h>

// Shares the physical SPI bus with the display. TFT_eSPI (built with
// USE_HSPI_PORT for this ESP32-S3 core) owns and initialises its own
// SPIClass instance for the bus (SCK/MOSI/MISO are wired to TFT+SD+LoRa
// together). Handing RadioLib that same instance — instead of letting it
// default to the global SPI object, which is a second, separate
// peripheral on this chip — avoids the two drivers fighting over the
// shared pins' IO-MUX, which was leaving the display frozen after the
// boot splash once LoRa init ran.
static SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, LORA_BUSY, TFT_eSPI::getSPIinstance());
static volatile bool rxFlag = false;
static String lastRxPacket;
static float  lastRxRSSI = 0, lastRxSNR = 0;
static uint32_t txCount = 0, rxCount = 0;

void IRAM_ATTR rxISR() { rxFlag = true; }

bool LoRa_Utils::setup() {
    int state = radio.begin(
        Config.lora.freq,
        Config.lora.bw,
        Config.lora.sf,
        Config.lora.cr,
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        Config.lora.txPower,
        Config.lora.preamble
    );
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("LoRa init failed: %d\n", state);
        return false;
    }
    radio.setCRC(2);
    radio.setDio1Action(rxISR);
    radio.startReceive();
    Serial.printf("LoRa OK — %.4f MHz SF%d BW%.0f %ddBm\n",
        Config.lora.freq, Config.lora.sf, Config.lora.bw, Config.lora.txPower);
    return true;
}

void LoRa_Utils::loop() {
    if (!rxFlag) return;
    rxFlag = false;
    String pkt;
    int state = radio.readData(pkt);
    if (state == RADIOLIB_ERR_NONE && pkt.length() > 3) {
        // Strip 3-byte LoRa APRS header (0x3C 0xFF 0x01)
        if ((uint8_t)pkt[0] == 0x3C && (uint8_t)pkt[1] == 0xFF && (uint8_t)pkt[2] == 0x01) {
            pkt = pkt.substring(3);
        }
        lastRxPacket = pkt;
        lastRxRSSI   = radio.getRSSI();
        lastRxSNR    = radio.getSNR();
        rxCount++;
    }
    radio.startReceive();
}

bool LoRa_Utils::sendPacket(const String& payload) {
    uint8_t buf[256];
    // Standard LoRa APRS header
    buf[0] = 0x3C; buf[1] = 0xFF; buf[2] = 0x01;
    size_t len = min((size_t)253, payload.length());
    memcpy(buf + 3, payload.c_str(), len);
    radio.standby();
    int state = radio.transmit(buf, len + 3);
    radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        txCount++;
        Serial.println("LoRa TX: " + payload);
        return true;
    }
    Serial.printf("LoRa TX failed: %d\n", state);
    return false;
}

bool LoRa_Utils::hasPacket()           { return lastRxPacket.length() > 0; }
String LoRa_Utils::getPacket()         { String p = lastRxPacket; lastRxPacket = ""; return p; }
float  LoRa_Utils::lastRSSI()          { return lastRxRSSI; }
float  LoRa_Utils::lastSNR()           { return lastRxSNR; }
uint32_t LoRa_Utils::getTxCount()      { return txCount; }
uint32_t LoRa_Utils::getRxCount()      { return rxCount; }
