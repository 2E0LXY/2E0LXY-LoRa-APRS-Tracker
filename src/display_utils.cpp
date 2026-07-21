#include "display_utils.h"
#include "board_pins.h"
#include "configuration.h"
#include "gps_utils.h"
#include "aprs_utils.h"
#include "beacon_utils.h"
#include "lora_utils.h"
#include "mqtt_utils.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();
static DisplayView currentView = VIEW_STATUS;
static uint32_t lastRedrawMs = 0;

// ── Colour palette ───────────────────────────────────────────────────────
#define C_BG        0x1082   // near-black
#define C_STATUS    0x1145   // dark header
#define C_ACCENT    0x04FF   // cyan
#define C_GREEN     0x07E0
#define C_AMBER     0xFBE0
#define C_RED       0xF800
#define C_GREY      0x5AEB
#define C_WHITE     0xFFFF
#define C_PURPLE    0x780F

static void drawStatusBar() {
    // Top 20px status bar
    tft.fillRect(0, 0, TFT_WIDTH, 20, C_STATUS);
    tft.setTextColor(C_WHITE, C_STATUS);
    tft.setTextSize(1);

    // Callsign
    tft.setCursor(4, 6);
    tft.print(fullCallsign());

    // GPS status
    tft.setCursor(90, 6);
    if (GPS_Utils::hasFix()) {
        tft.setTextColor(C_GREEN, C_STATUS);
        tft.printf("GPS %d", GPS_Utils::sats());
    } else {
        tft.setTextColor(C_AMBER, C_STATUS);
        tft.print("NO GPS");
    }

    // WiFi
    tft.setTextColor(WiFi.isConnected() ? C_GREEN : C_GREY, C_STATUS);
    tft.setCursor(140, 6); tft.print("WiFi");

    // MQTT
    tft.setTextColor(MQTT_Utils::isConnected() ? C_ACCENT : C_GREY, C_STATUS);
    tft.setCursor(180, 6); tft.print("MQTT");

    // APRS-IS
    tft.setTextColor(APRS_Utils::isConnected() ? C_GREEN : C_GREY, C_STATUS);
    tft.setCursor(220, 6); tft.print("IS");

    // Stations heard count
    tft.setTextColor(C_WHITE, C_STATUS);
    tft.setCursor(240, 6);
    tft.printf("Rx:%d", (int)APRS_Utils::heardStations.size());

    // Speed
    if (GPS_Utils::hasFix()) {
        tft.setCursor(290, 6);
        tft.printf("%.0f", GPS_Utils::speedKph());
    }
}

static void drawStatusView() {
    tft.fillRect(0, 20, TFT_WIDTH, TFT_HEIGHT - 20, C_BG);
    tft.setTextColor(C_WHITE, C_BG);

    // Position
    tft.setTextSize(1);
    tft.setCursor(4, 28);
    if (GPS_Utils::hasFix()) {
        tft.setTextColor(C_ACCENT, C_BG);
        tft.printf("%.5f  %.5f", GPS_Utils::lat(), GPS_Utils::lon());
        tft.setTextColor(C_WHITE, C_BG);
        tft.setCursor(4, 40);
        tft.printf("Alt: %.0fm  Spd: %.1fkm/h  Cse: %.0f°",
            GPS_Utils::altM(), GPS_Utils::speedKph(), GPS_Utils::courseDeg());
    } else {
        tft.setTextColor(C_AMBER, C_BG);
        tft.print("Acquiring GPS fix...");
    }

    // Divider
    tft.drawFastHLine(0, 52, TFT_WIDTH, C_GREY);

    // Last beacon
    tft.setTextColor(C_GREEN, C_BG);
    tft.setCursor(4, 56);
    uint32_t beaconAgo = (millis() - Beacon_Utils::lastMs()) / 1000;
    tft.printf("Last TX: %lus ago   Beacons: %d", beaconAgo, (int)Beacon_Utils::getCount());

    // LoRa stats
    tft.setTextColor(C_PURPLE, C_BG);
    tft.setCursor(4, 68);
    tft.printf("LoRa RX: %d  TX: %d", (int)LoRa_Utils::getRxCount(), (int)LoRa_Utils::getTxCount());

    // Symbol + comment
    tft.setTextColor(C_WHITE, C_BG);
    tft.setCursor(4, 80);
    tft.printf("Symbol: %s  Path: %s", Config.aprs.symbol.c_str(), Config.aprs.path.c_str());
    tft.setCursor(4, 92);
    tft.print(Config.aprs.comment);
}

static void drawStationList() {
    tft.fillRect(0, 20, TFT_WIDTH, TFT_HEIGHT - 20, C_BG);
    tft.setTextSize(1);

    auto& stations = APRS_Utils::heardStations;
    if (stations.empty()) {
        tft.setTextColor(C_GREY, C_BG);
        tft.setCursor(10, 60);
        tft.print("No stations heard yet.");
        return;
    }

    // Sort by last heard (most recent first)
    auto sorted = stations;
    std::sort(sorted.begin(), sorted.end(), [](const HeardStation& a, const HeardStation& b){
        return a.lastHeardMs > b.lastHeardMs;
    });

    int maxRows = (TFT_HEIGHT - 22) / 18;
    for (int i = 0; i < min((int)sorted.size(), maxRows); i++) {
        auto& s = sorted[i];
        int y = 22 + i * 18;

        // Row background alternating
        tft.fillRect(0, y, TFT_WIDTH, 17, (i % 2 == 0) ? 0x2104 : C_BG);

        // Callsign
        tft.setTextColor(C_ACCENT, (i % 2 == 0) ? 0x2104 : C_BG);
        tft.setCursor(2, y + 5);
        tft.print(s.callsign.length() > 10 ? s.callsign.substring(0,10) : s.callsign);

        // Distance / bearing (if we have GPS)
        if (GPS_Utils::hasFix() && s.lat != 0) {
            float dist = APRS_Utils::distanceKm(GPS_Utils::lat(), GPS_Utils::lon(), s.lat, s.lon);
            float bear = APRS_Utils::bearingDeg(GPS_Utils::lat(), GPS_Utils::lon(), s.lat, s.lon);
            tft.setTextColor(C_WHITE, (i % 2 == 0) ? 0x2104 : C_BG);
            tft.setCursor(100, y + 5);
            if (dist < 1.0f) tft.printf("%.0fm", dist * 1000);
            else             tft.printf("%.1fkm", dist);
            tft.setCursor(150, y + 5);
            tft.printf("%.0f°", bear);
        }

        // RSSI
        tft.setTextColor(s.rssi > -90 ? C_GREEN : C_AMBER, (i % 2 == 0) ? 0x2104 : C_BG);
        tft.setCursor(190, y + 5);
        if (s.rssi != 0) tft.printf("%.0fdB", s.rssi);

        // Age
        tft.setTextColor(C_GREY, (i % 2 == 0) ? 0x2104 : C_BG);
        tft.setCursor(240, y + 5);
        uint32_t age = (millis() - s.lastHeardMs) / 1000;
        if (age < 60) tft.printf("%ds", age);
        else          tft.printf("%dm", age/60);

        // Symbol hint
        tft.setTextColor(C_PURPLE, (i % 2 == 0) ? 0x2104 : C_BG);
        tft.setCursor(280, y + 5);
        tft.print(s.symbol.length() > 0 ? s.symbol[1] : '?');
    }
}

// ── Public API ────────────────────────────────────────────────────────────

void Display_Utils::setup() {
    tft.init();
    tft.setRotation(1);  // landscape
    tft.fillScreen(C_BG);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(BOARD_TFT_BL, 0);
    ledcWrite(0, Config.display.brightness);
    tft.setCursor(80, 110);
    tft.setTextColor(C_ACCENT);
    tft.setTextSize(2);
    tft.println("2E0LXY Tracker");
    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    tft.setCursor(100, 130);
    tft.println("LoRa APRS T-Deck Plus");
    delay(1500);
}

void Display_Utils::loop() {
    if (millis() - lastRedrawMs < 1000) return;
    lastRedrawMs = millis();
    drawStatusBar();
    switch (currentView) {
        case VIEW_STATUS:   drawStatusView();   break;
        case VIEW_STATIONS: drawStationList();  break;
        default: break;
    }
}

void Display_Utils::setView(DisplayView v)  { currentView = v; lastRedrawMs = 0; }
DisplayView Display_Utils::getView()         { return currentView; }
void Display_Utils::setBrightness(int v)     { ledcWrite(0, v); }

void Display_Utils::showMessage(const String& title, const String& body, uint16_t colour) {
    tft.fillRect(20, 80, 280, 80, 0x1104);
    tft.drawRect(20, 80, 280, 80, colour);
    tft.setTextColor(colour, 0x1104);
    tft.setTextSize(1);
    tft.setCursor(28, 90);
    tft.print(title);
    tft.setTextColor(C_WHITE, 0x1104);
    tft.setCursor(28, 105);
    // Word-wrap body
    String word, line;
    for (char c : body + ' ') {
        if (c == ' ') {
            if ((line + word).length() > 38) { tft.println(line); line = ""; tft.setCursor(28, tft.getCursorY()); }
            line += word + ' '; word = "";
        } else word += c;
    }
    if (line.length()) { tft.setCursor(28, tft.getCursorY()); tft.println(line); }
}
