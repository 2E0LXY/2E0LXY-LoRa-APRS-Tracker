#include "display_utils.h"
#include "board_pins.h"
#include "configuration.h"
#include "gps_utils.h"
#include "aprs_utils.h"
#include "beacon_utils.h"
#include "lora_utils.h"
#include "mqtt_utils.h"
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "boot_splash.h"
#include <WiFi.h>
#include "keyboard_utils.h"
#include "messaging.h"
#include "map_utils.h"
#include "satellites_utils.h"
#include "setup_view.h"
#include <algorithm>
#include <vector>

static TFT_eSPI tft = TFT_eSPI();
// Full-screen framebuffer. Every 1s redraw previously cleared and redrew
// straight to the physical ST7789 with no double buffering, which showed
// as a visible flicker. Drawing the whole frame into this PSRAM sprite
// first, then pushing it in one go, means the panel only ever sees a
// complete finished frame. Created once in Display_Utils::setup().
static TFT_eSprite spr = TFT_eSprite(&tft);
static bool spriteReady = false;
static DisplayView currentView = VIEW_STATIONS;   // home screen: callsigns heard
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
    spr.fillRect(0, 0, SCREEN_WIDTH, 20, C_STATUS);
    spr.setTextColor(C_WHITE, C_STATUS);
    spr.setTextSize(1);

    // Callsign
    spr.setCursor(4, 6);
    spr.print(fullCallsign());

    // GPS status
    spr.setCursor(90, 6);
    if (GPS_Utils::hasFix()) {
        spr.setTextColor(C_GREEN, C_STATUS);
        spr.printf("GPS %d", GPS_Utils::sats());
    } else {
        spr.setTextColor(C_AMBER, C_STATUS);
        spr.print("NO GPS");
    }

    // WiFi
    spr.setTextColor(WiFi.isConnected() ? C_GREEN : C_GREY, C_STATUS);
    spr.setCursor(140, 6); spr.print("WiFi");

    // MQTT
    spr.setTextColor(MQTT_Utils::isConnected() ? C_ACCENT : C_GREY, C_STATUS);
    spr.setCursor(180, 6); spr.print("MQTT");

    // APRS-IS
    spr.setTextColor(APRS_Utils::isConnected() ? C_GREEN : C_GREY, C_STATUS);
    spr.setCursor(220, 6); spr.print("IS");

    // Stations heard count
    spr.setTextColor(C_WHITE, C_STATUS);
    spr.setCursor(240, 6);
    spr.printf("Rx:%d", (int)APRS_Utils::heardStations.size());

    // Battery
    int pct = Display_Utils::batteryPercent();
    spr.setTextColor(pct > 30 ? C_GREEN : (pct > 15 ? C_AMBER : C_RED), C_STATUS);
    spr.setCursor(290, 6);
    spr.printf("%d%%", pct);
}
static void drawStatusView() {
    spr.fillRect(0, 20, SCREEN_WIDTH, SCREEN_HEIGHT - 20, C_BG);
    spr.setTextColor(C_WHITE, C_BG);

    // Position
    spr.setTextSize(1);
    spr.setCursor(4, 28);
    if (GPS_Utils::hasFix()) {
        spr.setTextColor(C_ACCENT, C_BG);
        spr.printf("%.5f  %.5f", GPS_Utils::lat(), GPS_Utils::lon());
        spr.setTextColor(C_WHITE, C_BG);
        spr.setCursor(4, 40);
        spr.printf("Alt: %.0fm  Spd: %.1fkm/h  Cse: %.0f°",
            GPS_Utils::altM(), GPS_Utils::speedKph(), GPS_Utils::courseDeg());
    } else {
        spr.setTextColor(C_AMBER, C_BG);
        spr.print("Acquiring GPS fix...");
    }

    // Divider
    spr.drawFastHLine(0, 52, SCREEN_WIDTH, C_GREY);

    // Last beacon
    spr.setTextColor(C_GREEN, C_BG);
    spr.setCursor(4, 56);
    uint32_t beaconAgo = (millis() - Beacon_Utils::lastMs()) / 1000;
    spr.printf("Last TX: %lus ago   Beacons: %d", beaconAgo, (int)Beacon_Utils::getCount());

    // LoRa stats
    spr.setTextColor(C_PURPLE, C_BG);
    spr.setCursor(4, 68);
    spr.printf("LoRa RX: %d  TX: %d", (int)LoRa_Utils::getRxCount(), (int)LoRa_Utils::getTxCount());

    // Symbol + comment
    spr.setTextColor(C_WHITE, C_BG);
    spr.setCursor(4, 80);
    spr.printf("Symbol: %s  Path: %s", Config.aprs.symbol.c_str(), Config.aprs.path.c_str());
    spr.setCursor(4, 92);
    spr.print(Config.aprs.comment);

    // Region + TX status
    spr.setCursor(4, 108);
    spr.setTextColor(Config.region.txConfirmed ? C_GREEN : C_RED, C_BG);
    spr.printf("Region: %s  TX:%s", Config.region.profileId.c_str(),
        Config.region.txConfirmed ? "ON" : "OFF (disabled)");

    // Key hints
    spr.setTextColor(C_GREY, C_BG);
    spr.setCursor(4, SCREEN_HEIGHT - 12);
    spr.print("S:Sts T:Stn M:Msg W:WiFi P:Prof C:Cfg B:Bcn");
}
static void drawStationList() {
    spr.fillRect(0, 20, SCREEN_WIDTH, SCREEN_HEIGHT - 20, C_BG);
    spr.setTextSize(1);

    auto& stations = APRS_Utils::heardStations;
    if (stations.empty()) {
        spr.setTextColor(C_GREY, C_BG);
        spr.setCursor(10, 60);
        spr.print("No stations heard yet.");
        spr.setCursor(4, SCREEN_HEIGHT - 12);
        spr.print("Home  |  S:Sts M:Msg W:WiFi P:Prof V:Sats B:Bcn");
        return;
    }

    // Sort by last heard (most recent first)
    auto sorted = stations;
    std::sort(sorted.begin(), sorted.end(), [](const HeardStation& a, const HeardStation& b){
        return a.lastHeardMs > b.lastHeardMs;
    });

    int maxRows = (SCREEN_HEIGHT - 22 - 12) / 18;   // reserve 12px for the footer hint
    for (int i = 0; i < min((int)sorted.size(), maxRows); i++) {
        auto& s = sorted[i];
        int y = 22 + i * 18;

        // Row background alternating
        spr.fillRect(0, y, SCREEN_WIDTH, 17, (i % 2 == 0) ? 0x2104 : C_BG);

        // Callsign
        spr.setTextColor(C_ACCENT, (i % 2 == 0) ? 0x2104 : C_BG);
        spr.setCursor(2, y + 5);
        spr.print(s.callsign.length() > 10 ? s.callsign.substring(0,10) : s.callsign);
        // Distance / bearing (if we have GPS)
        if (GPS_Utils::hasFix() && s.lat != 0) {
            float dist = APRS_Utils::distanceKm(GPS_Utils::lat(), GPS_Utils::lon(), s.lat, s.lon);
            float bear = APRS_Utils::bearingDeg(GPS_Utils::lat(), GPS_Utils::lon(), s.lat, s.lon);
            spr.setTextColor(C_WHITE, (i % 2 == 0) ? 0x2104 : C_BG);
            spr.setCursor(100, y + 5);
            if (dist < 1.0f) spr.printf("%.0fm", dist * 1000);
            else             spr.printf("%.1fkm", dist);
            spr.setCursor(150, y + 5);
            spr.printf("%.0f°", bear);
        }

        // Source (RF = yellow, INET = green) + RSSI
        bool viaRF = (s.via == HeardVia::RF);
        spr.setTextColor(viaRF ? C_AMBER : C_GREEN, (i % 2 == 0) ? 0x2104 : C_BG);
        spr.setCursor(190, y + 5);
        spr.print(viaRF ? "RF" : "INT");
        spr.setTextColor(C_WHITE, (i % 2 == 0) ? 0x2104 : C_BG);
        spr.setCursor(213, y + 5);
        if (s.everHeardRF) spr.printf("%.0fdB", s.rssi);

        // Age
        spr.setTextColor(C_GREY, (i % 2 == 0) ? 0x2104 : C_BG);
        spr.setCursor(255, y + 5);
        uint32_t age = (millis() - s.lastHeardMs) / 1000;
        if (age < 60) spr.printf("%ds", age);
        else          spr.printf("%dm", age/60);

        // Symbol hint
        spr.setTextColor(C_PURPLE, (i % 2 == 0) ? 0x2104 : C_BG);
        spr.setCursor(295, y + 5);
        spr.print(s.symbol.length() > 0 ? s.symbol[1] : '?');
    }

    spr.setTextColor(C_GREY, C_BG);
    spr.setCursor(4, SCREEN_HEIGHT - 12);
    spr.print("Home  |  S:Sts M:Msg W:WiFi P:Prof V:Sats B:Bcn");
}
static void drawMessagesView() {
    spr.fillRect(0, 20, SCREEN_WIDTH, SCREEN_HEIGHT - 20, Config.msg.bgColour);
    spr.setTextSize(1);

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
        int x = m.outgoing ? (SCREEN_WIDTH - bubbleW - 4) : 4;

        // Bubble with rounded corners
        spr.fillRoundRect(x, y, bubbleW, bubbleH, 4, bub);
        // Sender label above bubble (tiny)
        spr.setTextColor(C_GREY, Config.msg.bgColour);
        spr.setCursor(x + 2, y - 9);
        String who = m.outgoing ? ("me -> " + m.to) : m.from;
        spr.print(who.substring(0, 20));
        // Bubble text
        spr.setTextColor(txt, bub);
        int ty = y + PAD;
        for (auto& l : wrapped) {
            spr.setCursor(x + PAD, ty);
            spr.print(l);
            ty += LINEH;
        }
        y += bubbleH + 12;
        if (y > SCREEN_HEIGHT - 40) break;   // don't overrun the compose bar
    }

    if (hist.empty()) {
        spr.setTextColor(C_GREY, Config.msg.bgColour);
        spr.setCursor(10, 60);
        spr.print("No messages yet.");
        spr.setCursor(10, 74);
        spr.print("Incoming messages set the reply target.");
    }
    // Compose bar at the bottom
    spr.fillRect(0, SCREEN_HEIGHT - 34, SCREEN_WIDTH, 34, 0x2104);
    spr.drawFastHLine(0, SCREEN_HEIGHT - 34, SCREEN_WIDTH, C_ACCENT);
    spr.setTextColor(C_AMBER, 0x2104);
    spr.setCursor(2, SCREEN_HEIGHT - 28);
    String target = Messaging::getReplyTarget();
    spr.print("To: " + (target.length() ? target : String("(none)")));
    spr.setTextColor(C_WHITE, 0x2104);
    spr.setCursor(2, SCREEN_HEIGHT - 14);
    String buf = Keyboard_Utils::getBuffer();
    // Show tail of buffer with cursor
    String view = buf.length() > 50 ? buf.substring(buf.length() - 50) : buf;
    spr.print(view + "_");
}

// Sky plot: a circle representing the horizon (edge) to zenith (centre),
// with each tracked satellite placed by elevation (radius) and azimuth
// (angle from north, clockwise), coloured by constellation. Elevation 90°
// (overhead) plots at the centre; 0° (horizon) plots at the rim.
static void drawSatsView() {
    spr.fillRect(0, 20, SCREEN_WIDTH, SCREEN_HEIGHT - 20, C_BG);
    spr.setTextSize(1);

    const int cx = 80, cy = 130, R = 68;   // plot centre + radius (left side of screen)

    // Horizon circle + elevation rings at 30°/60°
    spr.drawCircle(cx, cy, R, C_GREY);
    spr.drawCircle(cx, cy, R * 2 / 3, 0x2965);
    spr.drawCircle(cx, cy, R / 3, 0x2965);
    spr.drawFastHLine(cx - R, cy, R * 2, 0x2104);
    spr.drawFastVLine(cx, cy - R, R * 2, 0x2104);
    spr.setTextColor(C_GREY, C_BG);
    spr.setCursor(cx - 3, cy - R - 10); spr.print("N");
    spr.setCursor(cx - 3, cy + R + 2);  spr.print("S");
    spr.setCursor(cx - R - 10, cy - 3); spr.print("W");
    spr.setCursor(cx + R + 3, cy - 3);  spr.print("E");

    auto& sats = Satellites_Utils::list();
    for (auto& s : sats) {
        float elevFrac = 1.0f - (s.elevation / 90.0f);   // 0 at zenith, 1 at horizon
        float azRad = s.azimuth * DEG_TO_RAD;
        int px = cx + (int)(R * elevFrac * sinf(azRad));
        int py = cy - (int)(R * elevFrac * cosf(azRad));
        uint16_t col = Satellites_Utils::constellationColour(s.constellation);
        // Dim the dot if we're not actually receiving signal (SNR 0 = in
        // view but not tracked well enough to use)
        uint16_t dotCol = (s.snr > 0) ? col : C_GREY;
        spr.fillCircle(px, py, 3, dotCol);
        spr.setTextColor(col, C_BG);
        spr.setCursor(px + 4, py - 3);
        spr.print(s.prn);
    }

    // Constellation counts + total, right side of screen
    int rx = 165, ry = 26;
    spr.setTextColor(C_WHITE, C_BG);
    spr.setCursor(rx, ry);
    spr.printf("Satellites in view: %d", (int)sats.size());
    ry += 14;
    spr.setTextColor(C_ACCENT, C_BG);
    spr.setCursor(rx, ry);
    spr.printf("Used in fix: %d  HDOP: %.1f", GPS_Utils::sats(), GPS_Utils::hdop());
    ry += 16;

    const GnssConstellation kinds[] = {
        GnssConstellation::GPS, GnssConstellation::GLONASS,
        GnssConstellation::BEIDOU, GnssConstellation::QZSS
    };
    for (auto k : kinds) {
        int n = Satellites_Utils::countByConstellation(k);
        spr.setTextColor(Satellites_Utils::constellationColour(k), C_BG);
        spr.fillCircle(rx + 4, ry + 4, 3, Satellites_Utils::constellationColour(k));
        spr.setCursor(rx + 12, ry);
        spr.printf("%-8s %d", Satellites_Utils::constellationName(k), n);
        ry += 14;
    }

    ry += 6;
    spr.setTextColor(C_GREY, C_BG);
    spr.setCursor(rx, ry);
    spr.print("Grey dot = in view,");
    ry += 12;
    spr.setCursor(rx, ry);
    spr.print("no signal yet");

    spr.setTextColor(C_GREY, C_BG);
    spr.setCursor(4, SCREEN_HEIGHT - 12);
    spr.print("Home  |  S:Sts T:Stn M:Msg W:WiFi P:Prof B:Bcn");
}

// On-device setup: a scrollable field list, one row per setting. The
// selected row is highlighted; Enter opens it for text entry (or flips a
// toggle immediately) and Enter again confirms and saves. Logic/state
// lives in setup_view.cpp — this just renders whatever it reports.
static void drawSetupView() {
    spr.fillRect(0, 20, SCREEN_WIDTH, SCREEN_HEIGHT - 20, C_BG);
    spr.setTextSize(1);

    spr.setTextColor(C_ACCENT, C_BG);
    spr.setCursor(4, 26);
    spr.print("On-Device Setup");
    spr.drawFastHLine(0, 38, SCREEN_WIDTH, C_GREY);

    auto& fields = Setup_View::fields();
    int sel = Setup_View::selectedIndex();
    bool editing = Setup_View::isEditing();
    int y = 44;
    for (size_t i = 0; i < fields.size(); i++) {
        bool isSel = ((int)i == sel);
        uint16_t rowBg = isSel ? 0x2965 : C_BG;
        spr.fillRect(0, y, SCREEN_WIDTH, 20, rowBg);
        spr.setTextColor(isSel ? C_WHITE : C_GREY, rowBg);
        spr.setCursor(4, y + 6);
        spr.print(fields[i].label);

        String shown = fields[i].value;
        if (fields[i].type == FieldType::PASSWORD && shown.length() > 0) {
            shown = "";
            for (size_t c = 0; c < fields[i].value.length(); c++) shown += '*';
        }
        if (isSel && editing) {
            shown = Setup_View::editBuffer();
            if (fields[i].type == FieldType::PASSWORD) {
                String masked = "";
                for (size_t c = 0; c < shown.length(); c++) masked += '*';
                shown = masked;
            }
            shown += "_";   // cursor
        }
        spr.setTextColor(isSel ? C_ACCENT : C_WHITE, rowBg);
        spr.setCursor(170, y + 6);
        spr.print(shown);
        y += 20;
        if (y > SCREEN_HEIGHT - 26) break;
    }

    spr.setTextColor(C_GREY, C_BG);
    spr.setCursor(4, SCREEN_HEIGHT - 12);
    if (editing) spr.print("Enter:Save  Del:Backspace");
    else         spr.print("I/O:Up/Down  Enter:Edit  Home  |  S:Sts");
}

// ── Map view drawing primitives (used by map_utils.cpp) ─────────────────
// Kept narrow and TFT-object-private so map_utils.cpp doesn't need direct
// TFT_eSPI access — mirrors how the other views only ever touch `tft`
// from inside this file.
void Display_Utils::mapClearArea(int top, int height) {
    tft.fillRect(0, top, SCREEN_WIDTH, height, TFT_BLACK);
}

void Display_Utils::mapPushTileLine(int x, int y, int w, uint16_t* line) {
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    int drawW = w;
    if (x < 0) { line -= x; drawW += x; x = 0; }          // clip left
    if (x + drawW > SCREEN_WIDTH) drawW = SCREEN_WIDTH - x; // clip right
    if (drawW <= 0) return;
    tft.pushImage(x, y, drawW, 1, line);
}

void Display_Utils::mapPushIcon(int x, int y, const uint16_t* iconRGB565, int size, uint16_t transparentColour) {
    tft.pushImage(x, y, size, size, iconRGB565, transparentColour);
}

void Display_Utils::mapDrawCrosshair(int x, int y) {
    tft.drawFastHLine(x - 6, y, 13, C_ACCENT);
    tft.drawFastVLine(x, y - 6, 13, C_ACCENT);
    tft.drawCircle(x, y, 4, C_ACCENT);
}

void Display_Utils::mapDrawFooter(int y, int zoomLevel) {
    tft.fillRect(0, y, SCREEN_WIDTH, SCREEN_HEIGHT - y, C_STATUS);
    tft.setTextColor(C_WHITE, C_STATUS);
    tft.setCursor(4, y + 3);
    tft.printf("Zoom %d  |  Home  |  +/- zoom  Trackball pan", zoomLevel);
}

void Display_Utils::mapDrawNoCard() {
    tft.fillRect(0, 20, SCREEN_WIDTH, SCREEN_HEIGHT - 20, C_BG);
    tft.setTextColor(C_AMBER, C_BG);
    tft.setCursor(20, 80);
    tft.println("No SD card / no cached tiles.");
    tft.setCursor(20, 96);
    tft.println("Use the Map Downloader on");
    tft.setCursor(20, 110);
    tft.println("aprsnet.uk, then USB mode (U)");
    tft.setCursor(20, 124);
    tft.println("to copy tiles to the card.");
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

// TJpg_Decoder output callback — pushes each decoded MCU block to the TFT.
static bool tftJpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void Display_Utils::setup() {
    tft.init();
    tft.setRotation(1);  // landscape
    tft.fillScreen(TFT_BLACK);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(BOARD_TFT_BL, 0);
    ledcWrite(0, Config.display.brightness);

    // Framebuffer for flicker-free redraws — see the spr declaration above.
    // 320x240x16bpp = 150KB; TFT_eSPI auto-uses PSRAM for sprites on
    // boards that have it (this one has 8MB), so this doesn't compete
    // with internal RAM. If allocation somehow fails, spriteReady stays
    // false and Display_Utils::loop() falls back to drawing tft directly.
    spr.setColorDepth(16);
    spriteReady = (spr.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT) != nullptr);
    if (!spriteReady) Serial.println("Display: sprite alloc failed, falling back to direct draw");

    // ── Boot splash: decode the embedded APRS BOOT JPEG ──────────────────
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tftJpgOutput);
    uint16_t jw = 0, jh = 0;
    TJpgDec.getJpgSize(&jw, &jh, BOOT_SPLASH_JPG, BOOT_SPLASH_LEN);
    // Centre if not exactly full-screen
    int16_t ox = (jw && jw < 320) ? (320 - jw) / 2 : 0;
    int16_t oy = (jh && jh < 240) ? (240 - jh) / 2 : 0;
    if (TJpgDec.drawJpg(ox, oy, BOOT_SPLASH_JPG, BOOT_SPLASH_LEN) != 0) {
        // Fallback to text splash if decode fails
        tft.fillScreen(C_BG);
        tft.setCursor(70, 100);
        tft.setTextColor(C_ACCENT);
        tft.setTextSize(2);
        tft.println("APRS BOOT");
        tft.setTextSize(1);
        tft.setTextColor(C_WHITE);
        tft.setCursor(70, 130);
        tft.println("LoRa APRS Edition - aprsnet.uk");
    }
    delay(2500);   // hold the splash so it's readable
}
void Display_Utils::loop() {
    if (millis() < overlayUntilMs) return;         // hold overlay on screen
    if (millis() - lastRedrawMs < 1000) return;
    lastRedrawMs = millis();
    // Map view redraws itself on its own (slower) cadence — tile decode
    // from SD is expensive, so it isn't worth doing every second like the
    // text-only views. It still draws straight to tft (not the sprite);
    // its 3s cadence isn't the once-a-second flicker this fixes, and
    // adding a second full-screen PNG-decode-sized buffer isn't worth it
    // here — this can be revisited if the map ever needs it too.
    if (currentView == VIEW_MAP) {
        static uint32_t lastMapDrawMs = 0;
        drawStatusBar();   // drawn into the sprite; push just that 20px strip
        if (spriteReady) spr.pushSprite(0, 0, 0, 0, SCREEN_WIDTH, 20);
        if (millis() - lastMapDrawMs > 3000) {
            lastMapDrawMs = millis();
            Map_Utils::draw();
        }
        return;
    }

    if (!spriteReady) {
        // Sprite alloc failed at boot — fall back to the old direct-draw
        // behaviour rather than silently doing nothing. This means the
        // flicker returns, but the UI still works.
        drawStatusBar();
        switch (currentView) {
            case VIEW_STATUS:   drawStatusView();   break;
            case VIEW_STATIONS: drawStationList();  break;
            case VIEW_MESSAGES: drawMessagesView(); break;
            case VIEW_SATS:     drawSatsView();     break;
            case VIEW_SETUP:    drawSetupView();    break;
            default: break;
        }
        return;
    }

    // Draw the whole frame into the off-screen sprite, then push it to the
    // panel in one go — see the spr declaration/comment above for why.
    drawStatusBar();
    switch (currentView) {
        case VIEW_STATUS:   drawStatusView();   break;
        case VIEW_STATIONS: drawStationList();  break;
        case VIEW_MESSAGES: drawMessagesView(); break;
        case VIEW_SATS:     drawSatsView();     break;
        case VIEW_SETUP:    drawSetupView();    break;
        default: break;
    }
    spr.pushSprite(0, 0);
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
