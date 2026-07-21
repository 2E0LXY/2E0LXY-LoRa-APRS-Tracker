#include "weather_utils.h"
#include "configuration.h"
#include "gps_utils.h"
#include "lora_utils.h"
#include "aprs_utils.h"
#include <Wire.h>
#include <Adafruit_BME280.h>

static Adafruit_BME280 bme;
static bool     present = false;
static uint32_t lastWxMs = 0;

bool Weather_Utils::setup() {
    // Try both common BME280 addresses on the already-initialised I2C bus.
    if (bme.begin(0x76) || bme.begin(0x77)) {
        present = true;
        Config.weather.enabled = true;
        Serial.println("BME280: found");
    } else {
        present = false;
        Serial.println("BME280: not present");
    }
    return present;
}

bool  Weather_Utils::available()    { return present; }
float Weather_Utils::temperatureC() { return present ? bme.readTemperature() + Config.weather.tempOffset : 0; }
float Weather_Utils::humidity()     { return present ? bme.readHumidity() : 0; }
float Weather_Utils::pressureHpa()  { return present ? bme.readPressure() / 100.0f + Config.weather.pressOffset : 0; }

// APRS positionless weather report:
//   CALL>APLT00::...  no — WX uses position + _ symbol OR positionless _ form.
// We use the compressed positionless form: "_<CSE>/<SPD>g...t...r...P...h...b..."
// but simplest interoperable form is a full position WX report with the WX
// symbol overlay. Here we build a positionless WX report (starts with '_').
String Weather_Utils::buildWxPacket() {
    if (!present) return "";
    float tC = temperatureC();
    float tF = tC * 9.0f / 5.0f + 32.0f;
    float h  = humidity();
    float p  = pressureHpa();

    char wx[80];
    // _<timestamp is optional> ... use MDHM? Keep positionless: c...s...g...t...r...p...P...h...b...
    // Minimal WX: temperature (t, °F 3-digit), humidity (h, 2-digit), pressure (b, tenths of hPa 5-digit)
    int tFi = (int)round(tF);
    int hi  = (int)round(h); if (hi >= 100) hi = 0;  // APRS: h00 = 100%
    int bi  = (int)round(p * 10.0f);
    snprintf(wx, sizeof(wx), "_c...s...g...t%03dh%02db%05d", tFi, hi, bi);

    return fullCallsign() + ">APLT00," + Config.aprs.path + ":" + String(wx)
         + "2E0LXY BME280";
}

void Weather_Utils::loop() {
    if (!present || !Config.weather.txWx) return;
    uint32_t now = millis();
    if (now - lastWxMs < (uint32_t)Config.weather.wxInterval * 1000) return;
    lastWxMs = now;

    String pkt = buildWxPacket();
    if (pkt.length() == 0) return;
    if (Config.beacon.loraEnabled)  LoRa_Utils::sendPacket(pkt);
    if (APRS_Utils::isConnected())  APRS_Utils::sendPacketIS(pkt);
    Serial.println("WX TX: " + pkt);
}
