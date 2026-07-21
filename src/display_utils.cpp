#include "display_utils.h"
#include "board_pins.h"
#include "configuration.h"
#include "gps_utils.h"
#include "aprs_utils.h"
#include "beacon_utils.h"
#include "lora_utils.h"
#include "mqtt_utils.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "keyboard_utils.h"
#include "messaging.h"
#include <algorithm>
#include <vector>

static TFT_eSPI tft = TFT_eSPI();
static DisplayView currentView = VIEW_STATUS;
static uint32_t lastRedrawMs = 0;
static uint32_t overlayUntilMs = 0;   // suppress redraw while an overlay message is showing

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

    // Battery
    int pct = Display_Utils::batteryPercent();
    tft.setTextColor(pct > 30 ? C_GREEN : (pct > 15 ? C_AMBER : C_RED), C_STATUS);
    tft.setCursor(290, 6);
    tft.printf("%d%%", pct);
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

    // Region + TX status
    tft.setCursor(4, 108);
    tft.setTextColor(Config.region.txConfirmed ? C_GREEN : C_RED, C_BG);
    tft.printf("Region: %s  TX:%s", Config.region.profileId.c_str(),
        Config.region.txConfirmed ? "ON" : "OFF (disabled)");

    // Key hints
    tft.setTextColor(C_GREY, C_BG);
    tft.setCursor(4, TFT_HEIGHT - 12);
    tft.print("1:Status 2:Stations 3:Msgs 4:Setup(WiFi) B:Beacon");
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

static void drawMessagesView() {
    tft.fillRect(0, 20, TFT_WIDTH, TFT_HEIGHT - 20, Config.msg.bgColour);
    tft.setTextSize(1);

    // ── Chat bubbles (mirrors the website / Android app) ────────────────
    // Outgoing = right-aligned, Config.msg.outBubble.
    // Incoming = left-aligned,  Config.msg.inBubble.
    auto& hist = Messaging::history;
    const int MAXW = 210;          // max bubble width in px
    const int PAD  = 4;
    const int CHARW = 6;           // approx width of size-1 font glyph
    const int LINEH = 10;
    int y = 24;
    int shown = min((int)hist.size(), 12);

    for (int i = (int)hist.size() - shown; i < (int)hist.size(); i++) {
        if (i < 0) continue;
        auto& m = hist[i];
        String text = m.text;
        // Word-wrap into lines that fit MAXW
        int maxChars = (MAXW - 2 * PAD) / CHARW;
        std::vector<String> wrapped;
        String line = "";
        for (int c = 0; c < (int)text.length(); c++) {
            line += text[c];
            if ((int)line.length() >= maxChars) { wrapped.push_back(line); line = ""; }
        }
        if (line.length()) wrapped.push_back(line);
        if (wrapped.empty()) wrapped.push_back("");

        int bubbleH = (int)wrapped.size() * LINEH + 2 * PAD;
        int bubbleW = 0;
        for (auto& l : wrapped) bubbleW = max(bubbleW, (int)l.length() * CHARW);
        bubbleW += 2 * PAD;
        if (bubbleW > MAXW) bubbleW = MAXW;

        uint16_t bub = m.outgoing ? Config.msg.outBubble : Config.msg.inBubble;
        uint16_t txt = m.outgoing ? Config.msg.outText   : Config.msg.inText;
        int x = m.outgoing ? (TFT_WIDTH - bubbleW - 4) : 4;

        // Bubble with rounded corners
        tft.fillRoundRect(x, y, bubbleW, bubbleH, 4, bub);
        // Sender label above bubble (tiny)
        tft.setTextColor(C_GREY, Config.msg.bgColour);
        tft.setCursor(x + 2, y - 9);
        String who = m.outgoing ? ("me -> " + m.to) : m.from;
        tft.print(who.substring(0, 20));
        // Bubble text
        tft.setTextColor(txt, bub);
        int ty = y + PAD;
        for (auto& l : wrapped) {
            tft.setCursor(x + PAD, ty);
            tft.print(l);
            ty += LINEH;
        }
        y += bubbleH + 12;
        if (y > TFT_HEIGHT - 40) break;   // don't overrun the compose bar
    }

    if (hist.empty()) {
        tft.setTextColor(C_GREY, Config.msg.bgColour);
        tft.setCursor(10, 60);
        tft.print("No messages yet.");
        tft.setCursor(10, 74);
        tft.print("Incoming messages set the reply target.");
    }

    // Compose bar at the bottom
    tft.fillRect(0, TFT_HEIGHT - 34, TFT_WIDTH, 34, 0x2104);
    tft.drawFastHLine(0, TFT_HEIGHT - 34, TFT_WIDTH, C_ACCENT);
    tft.setTextColor(C_AMBER, 0x2104);
    tft.setCursor(2, TFT_HEIGHT - 28);
    String target = Messaging::getReplyTarget();
    tft.print("To: " + (target.length() ? target : String("(none)")));
    tft.setTextColor(C_WHITE, 0x2104);
    tft.setCursor(2, TFT_HEIGHT - 14);
    String buf = Keyboard_Utils::getBuffer();
    // Show tail of buffer with cursor
    String view = buf.length() > 50 ? buf.substring(buf.length() - 50) : buf;
    tft.print(view + "_");
}

// ── Battery ───────────────────────────────────────────────────────────────
float Display_Utils::batteryVolts() {
    // Divider halves the LiPo voltage into the ADC
    uint32_t mv = analogReadMilliVolts(BOARD_BAT_ADC) * 2;
    return mv / 1000.0f;
}

int Display_Utils::batteryPercent() {
    float v = batteryVolts() * 1000;
    if (v <= BAT_EMPTY_MV) return 0;
    if (v >= BAT_FULL_MV)  return 100;
    return (int)((v - BAT_EMPTY_MV) * 100.0f / (BAT_FULL_MV - BAT_EMPTY_MV));
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
    if (millis() < overlayUntilMs) return;         // hold overlay on screen
    if (millis() - lastRedrawMs < 1000) return;
    lastRedrawMs = millis();
    drawStatusBar();
    switch (currentView) {
        case VIEW_STATUS:   drawStatusView();   break;
        case VIEW_STATIONS: drawStationList();  break;
        case VIEW_MESSAGES: drawMessagesView(); break;
        default: break;
    }
}

void Display_Utils::setView(DisplayView v)  { currentView = v; lastRedrawMs = 0; }
DisplayView Display_Utils::getView()         { return currentView; }
void Display_Utils::setBrightness(int v)     { ledcWrite(0, v); }

void Display_Utils::showMessage(const String& title, const String& body, uint16_t colour) {
    overlayUntilMs = millis() + 4000;   // keep visible for 4 s
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
