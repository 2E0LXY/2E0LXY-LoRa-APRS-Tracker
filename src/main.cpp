/**
 * 2E0LXY LoRa APRS Tracker
 * Standalone firmware for the LilyGO T-Deck Plus
 *
 * Features:
 *   - SX1262 LoRa APRS transceiver (439.9125 MHz UK)
 *   - SmartBeaconing with corner-pegging
 *   - Two-way APRS messaging via T-Deck keyboard
 *   - Live station list (heard via LoRa RF or APRS-IS)
 *   - APRS-IS gateway via WiFi (www.aprsnet.uk:14580)
 *   - aprsnet.uk MQTT telemetry + remote control
 *   - ST7789 320×240 TFT — status / station list / message views
 *   - MicroSD ready (future offline map)
 *   - OTA firmware update from GitHub releases
 *
 * Hardware: LilyGO T-Deck Plus (ESP32-S3, 16MB Flash, 8MB PSRAM)
 * Repository: https://github.com/2E0LXY/2E0LXY-LoRa-APRS-Tracker
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include "board_pins.h"
#include "configuration.h"
#include "gps_utils.h"
#include "satellites_utils.h"
#include "lora_utils.h"
#include "aprs_utils.h"
#include "beacon_utils.h"
#include "mqtt_utils.h"
#include "display_utils.h"
#include "keyboard_utils.h"
#include "messaging.h"
#include "ota_utils.h"
#include "webconfig.h"
#include "weather_utils.h"
#include "ble_kiss.h"
#include "audio_utils.h"
#include "map_utils.h"
#include "setup_view.h"
#include "trackball_utils.h"
#include "touch_utils.h"

// ── Forward declarations ──────────────────────────────────────────────────
void handleKeyInput(char key);
void handleLoRaRx(const String& packet, float rssi, float snr);
void handleAPRSMessage(const String& from, const String& text, const String& msgID);

// ── APRS message callback (override weak symbol in aprs_utils) ────────────
namespace APRS_Utils {
void onMessageReceived(const ParsedPacket& p) {
    handleAPRSMessage(p.fromCall, p.text, p.msgID);
}
}

// ── Setup ─────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n[BOOT] Free heap: %u  PSRAM: %u\n", ESP.getFreeHeap(), ESP.getPsramSize());
    Serial.println("\n\n=== 2E0LXY LoRa APRS Tracker ===");

    // Power on peripheral rail
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100);

    // Load config (or use defaults)
    bool cfgOk = loadConfig();
    Serial.printf("Config: %s\n", cfgOk ? "loaded" : "using defaults");

    // Display
    Display_Utils::setup();

    // Map (SD/tiles brought up on first draw, not at boot)
    Map_Utils::setup();
    Setup_View::setup();

    // GPS
    GPS_Utils::setup();
    Satellites_Utils::setup();   // per-satellite sky-plot data (attaches to GPS_Utils::gps)

    // Keyboard
    Keyboard_Utils::setup();
    Trackball_Utils::setup();
    Touch_Utils::setup();

    // Weather sensor (auto-detect BME280 on I2C)
    Weather_Utils::setup();
    Serial.printf("Heap after weather: %u\n", ESP.getFreeHeap());

    // Audio (message notification tone) — lazy I2S init on first play
    Audio_Utils::setup();

    // BLE KISS TNC — only if explicitly enabled (OFF by default; the BLE
    // stack + WiFi + SPI at boot is memory-heavy and can prevent booting).
    if (Config.device.bleEnabled) {
        BLE_KISS::begin();
        Serial.printf("Heap after BLE: %u\n", ESP.getFreeHeap());
    }

    // LoRa (shares the display's SPI bus/instance — see lora_utils.cpp)
    if (!LoRa_Utils::setup()) {
        Display_Utils::showMessage("LoRa Error", "SX1262 init failed — check hardware", TFT_RED);
        delay(3000);
    }

    // WiFi (non-blocking)
    if (Config.wifi.enabled && Config.wifi.ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        // Identify as "T-Deck-<callsign>" in the router's connected-clients
        // list (DHCP hostname) instead of the generic default. Must be set
        // after WiFi.mode() and before WiFi.begin().
        String hostname = "T-Deck-" + fullCallsign();
        WiFi.setHostname(hostname.c_str());
        WiFi.begin(Config.wifi.ssid.c_str(), Config.wifi.password.c_str());
        Serial.printf("WiFi: connecting to %s as '%s'\n", Config.wifi.ssid.c_str(), hostname.c_str());
    }

    Serial.printf("Callsign: %s  Freq: %.4f MHz  SF%d\n",
        fullCallsign().c_str(), Config.lora.freq, Config.lora.sf);

    // Explicitly (re-)confirm the home screen as the starting view —
    // harmless belt-and-braces since VIEW_HOME is already the default,
    // but costs nothing to be explicit about it here.
    Display_Utils::setView(VIEW_HOME);
}

// ── Main loop ─────────────────────────────────────────────────────────────
void loop() {
    // GPS NMEA
    GPS_Utils::loop();
    Satellites_Utils::loop();

    // LoRa receive
    LoRa_Utils::loop();
    if (LoRa_Utils::hasPacket()) {
        String pkt = LoRa_Utils::getPacket();
        handleLoRaRx(pkt, LoRa_Utils::lastRSSI(), LoRa_Utils::lastSNR());
    }

    // SmartBeacon
    Beacon_Utils::loop();

    // APRS-IS
    if (WiFi.isConnected()) {
        APRS_Utils::loop();
        MQTT_Utils::loop();
    }

    // Temporary WiFi stability diagnostic
    static bool lastWifiUp = false;
    static uint32_t lastWifiEventMs = 0;
    bool wifiUp = WiFi.isConnected();
    if (wifiUp != lastWifiUp) {
        Serial.printf("WiFi: %s (RSSI %ddBm, after %lums)\n",
            wifiUp ? "UP" : "DOWN", WiFi.RSSI(), millis() - lastWifiEventMs);
        lastWifiUp = wifiUp;
        lastWifiEventMs = millis();
    }
    static uint32_t lastHeapMs = 0;
    if (millis() - lastHeapMs > 15000) {
        lastHeapMs = millis();
        Serial.printf("Heap: free=%u minFree=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }

    // Keyboard input
    if (Keyboard_Utils::available()) {
        char k = Keyboard_Utils::getKey();
        if (k) handleKeyInput(k);
    }
    // Trackball input — direction re-enabled after ruling it out as the
    // cause of a separate boot-time issue (home screen defaulting to
    // Stations on first boot) via a temporary disable-and-test; keeping
    // it disabled once ruled out would only remove working functionality
    // for no reason, so it's back.
    Trackball_Utils::loop();
    char tbDir = Trackball_Utils::getDirection();
    if (tbDir && Display_Utils::getView() == VIEW_HOME) {
        if (tbDir == 'U') Display_Utils::homeMove(0, -1);
        else if (tbDir == 'D') Display_Utils::homeMove(0, 1);
        else if (tbDir == 'L') Display_Utils::homeMove(-1, 0);
        else if (tbDir == 'R') Display_Utils::homeMove(1, 0);
    } else if (tbDir) {
        if (tbDir == 'U') handleKeyInput('i');
        else if (tbDir == 'D') handleKeyInput('o');
        else if (tbDir == 'L') handleKeyInput(',');
        else if (tbDir == 'R') handleKeyInput('.');
    }
    if (Trackball_Utils::clickPressed()) handleKeyInput('\r');

    // Touchscreen input (GT911, confirmed present on this hardware via
    // LilyGO's own factory firmware — see touch_utils.cpp). Only polled
    // while actually on the home screen (the only place it's wired up to
    // do anything) — it shares the I2C bus with the keyboard, and
    // polling it every loop iteration regardless of view was very likely
    // starving/corrupting keyboard I2C timing on other screens (reported:
    // Setup fields stopped accepting any typed characters after touch
    // support was added, while navigation — which doesn't hit the I2C
    // bus as often per keypress — still worked).
    if (Touch_Utils::isPresent() && Display_Utils::getView() == VIEW_HOME) {
        Touch_Utils::loop();
        int tx, ty;
        if (Touch_Utils::tapped(tx, ty)) {
            if (Display_Utils::homeSelectAt(tx, ty)) {
                handleKeyInput('\r');   // activate whatever homeSelectAt just selected
            }
        }
    }

    // Display refresh
    Display_Utils::loop();

    // Messaging
    Messaging::loop();

    // Weather WX beacons
    Weather_Utils::loop();

    // BLE KISS TNC
    if (Config.device.bleEnabled) BLE_KISS::loop();

    // Web config portal (if running)
    WebConfig::loop();

    // OTA check (every 6 hours)
    OTA_Utils::loop();
}

// ── LoRa receive handler ──────────────────────────────────────────────────
void handleLoRaRx(const String& packet, float rssi, float snr) {
    Serial.printf("LoRa RX [%.0fdBm SNR%.1f]: %s\n", rssi, snr, packet.c_str());

    // Forward the raw frame to any BLE-connected phone (KISS TNC mode)
    if (Config.device.bleEnabled && BLE_KISS::isConnected()) BLE_KISS::sendToPhone(packet);

    ParsedPacket p = APRS_Utils::parsePacket(packet);
    p.rssi = rssi;
    p.snr  = snr;

    if (p.valid && p.hasPosition) {
        APRS_Utils::updateStation(p.fromCall, p);
    }

    // Gate LoRa to APRS-IS with proper qAO construct (receive-only iGate)
    if (p.valid && Config.beacon.aprsIsEnabled && APRS_Utils::isConnected()) {
        int ci = packet.indexOf(':');
        if (ci > 0) {
            String gated = packet.substring(0, ci) + ",qAO," + fullCallsign()
                         + packet.substring(ci);
            APRS_Utils::sendPacketIS(gated);
        }
    }

    if (p.isMessage) {
        String _tc = p.toCall; _tc.trim();
        if (_tc == fullCallsign()) {
            handleAPRSMessage(p.fromCall, p.text, p.msgID);
        }
    }
}

// ── Message received ──────────────────────────────────────────────────────
void handleAPRSMessage(const String& from, const String& text, const String& msgID) {
    Serial.printf("MSG from %s: %s {%s}\n", from.c_str(), text.c_str(), msgID.c_str());
    Messaging::receive(from, text, msgID);
    Display_Utils::showMessage("MSG: " + from, text, TFT_CYAN);
    Audio_Utils::playMessageTone();

    // Send ACK only if the message carried an {ID} (per APRS spec)
    if (msgID.length() > 0) {
        String ack = APRS_Utils::buildAckPacket(from, msgID);
        if (Config.beacon.loraEnabled)   LoRa_Utils::sendPacket(ack);
        if (APRS_Utils::isConnected())   APRS_Utils::sendPacketIS(ack);
    }
}

// ── Keyboard navigation & input ───────────────────────────────────────────
void handleKeyInput(char key) {
    bool inMessages = (Display_Utils::getView() == VIEW_MESSAGES);
    bool inSetup    = (Display_Utils::getView() == VIEW_SETUP);
    bool isDelete = (key == '\b' || key == 0x08 || key == 0x7F);

    // Setup screen: hand every key to Setup_View while active, same
    // pattern as Messages compose mode below — it needs Delete to
    // backspace inside a field being edited, not jump home, and letters
    // typed into a field (like 's' in a WiFi SSID) shouldn't trigger
    // navigation shortcuts. Only escape home when Delete is pressed
    // outside of active editing.
    if (inSetup) {
        if (isDelete && !Setup_View::isEditing()) {
            Display_Utils::setView(VIEW_HOME);
            return;
        }
        Setup_View::handleKey(key);
        return;
    }

    // Delete/backspace outside compose mode always returns to the home
    // screen (icon grid). Inside Messages it backspaces the typed text as
    // before — except on an already-empty buffer, where there's nothing
    // left to delete, so it also returns home instead of doing nothing.
    if (isDelete && !inMessages) {
        Display_Utils::setView(VIEW_HOME);
        return;
    }
    if (isDelete && inMessages && Keyboard_Utils::getBuffer().length() == 0) {
        Display_Utils::setView(VIEW_HOME);
        return;
    }

    // ── Compose mode: printable characters go to the buffer FIRST so
    //    digits and 'b' are typable inside a message ─────────────────────
    if (inMessages) {
        if (key == '\r' || key == '\n') {
            String buf = Keyboard_Utils::getBuffer();
            String dest = Messaging::getReplyTarget();
            if (buf.length() == 0) {
                // Enter on empty buffer exits messages view (to home)
                Display_Utils::setView(VIEW_HOME);
                return;
            }
            if (dest.length() > 0) {
                bool sent = false;
                String route = Config.msg.defaultRoute;

                if (route == "server" && MQTT_Utils::isConnected()) {
                    // aprsnet.uk store-and-forward (no RF)
                    sent = MQTT_Utils::publishMessage(dest, buf);
                    if (sent) Display_Utils::showMessage("Sent (server)", "-> " + dest, TFT_GREEN);
                }
                if (!sent && (route == "aprsis" || route == "server") && APRS_Utils::isConnected()) {
                    // Fall back to APRS-IS over WiFi
                    String pkt = APRS_Utils::buildMessagePacket(dest, buf, Messaging::nextMsgID());
                    sent = APRS_Utils::sendPacketIS(pkt);
                    if (sent) Display_Utils::showMessage("Sent (APRS-IS)", "-> " + dest, TFT_GREEN);
                }
                if (!sent) {
                    // Default / fallback: LoRa RF
                    String pkt = APRS_Utils::buildMessagePacket(dest, buf, Messaging::nextMsgID());
                    if (Config.beacon.loraEnabled) sent = LoRa_Utils::sendPacket(pkt);
                    // Also gate to APRS-IS if connected
                    if (APRS_Utils::isConnected()) APRS_Utils::sendPacketIS(pkt);
                    if (sent) Display_Utils::showMessage("Sent (LoRa)", "-> " + dest, TFT_GREEN);
                }
                if (sent) {
                    Messaging::markSent(dest, buf);
                    Keyboard_Utils::clearBuffer();
                } else {
                    Display_Utils::showMessage("Send failed", "No route available for " + dest, TFT_RED);
                }
            } else {
                Display_Utils::showMessage("No target", "Reply target not set - wait for a message first", TFT_ORANGE);
            }
        } else {
            Keyboard_Utils::appendToBuffer(key);
        }
        return;
    }

    // ── Navigation (outside messages view) ──────────────────────────────
    // Home screen: direction keys move the highlight, Enter activates the
    // selected tile. i/o/,/. are what the trackball's four directions and
    // the keyboard's own pan/zoom keys both send (see Trackball_Utils in
    // the main loop and the Map pan bindings below) — reusing them here
    // means the same physical inputs navigate the grid without needing a
    // separate set of "arrow key" bindings.
    if (Display_Utils::getView() == VIEW_HOME) {
        if (key == 'i' || key == 'I') { Display_Utils::homeMove(0, -1); return; }
        if (key == 'o' || key == 'O') { Display_Utils::homeMove(0, 1);  return; }
        if (key == ',') { Display_Utils::homeMove(-1, 0); return; }
        if (key == '.') { Display_Utils::homeMove(1, 0);  return; }
        if (key == '\r' || key == '\n') {
            const char* label = Display_Utils::homeSelectedLabel();
            if (strcmp(label, "WiFi") == 0) {
                if (!WebConfig::isRunning()) {
                    WebConfig::begin();
                    Display_Utils::showMessage("Setup Portal",
                        "Join WiFi '2E0LXY-Tracker-Setup' then browse to 192.168.4.1", TFT_CYAN);
                } else {
                    WebConfig::stop();
                    Display_Utils::showMessage("Setup Portal", "Stopped", TFT_ORANGE);
                }
            } else if (strcmp(label, "Beacon") == 0) {
                Beacon_Utils::sendBeacon();
                Display_Utils::showMessage("Beacon", "Manual beacon sent", TFT_GREEN);
            } else if (strcmp(label, "Stations") == 0) {
                Display_Utils::setView(VIEW_STATIONS);
            } else if (strcmp(label, "Messages") == 0) {
                Display_Utils::setView(VIEW_MESSAGES);
            } else if (strcmp(label, "Map") == 0) {
                Display_Utils::setView(VIEW_MAP);
            } else if (strcmp(label, "Sats") == 0) {
                Display_Utils::setView(VIEW_SATS);
            } else if (strcmp(label, "Status") == 0) {
                Display_Utils::setView(VIEW_STATUS);
            } else if (strcmp(label, "Setup") == 0) {
                Setup_View::enter();
                Display_Utils::setView(VIEW_SETUP);
            }
            return;
        }
        // Fall through to the letter shortcuts below too, so 's'/'t'/'m'
        // etc. still work as a fast path even from the home screen.
    }

    // Letter aliases (s/t/m/w/p/u) alongside the printed digits — the
    // T-Deck keyboard co-processor resolves its own shift/symbol layer
    // before the byte reaches us, so Alt+<number-row key> doesn't reliably
    // produce '1'-'6' here. Plain letters always arrive unmodified.
    if (key == '1' || key == 's' || key == 'S') { Display_Utils::setView(VIEW_STATUS);   return; }
    if (key == '2' || key == 't' || key == 'T') { Display_Utils::setView(VIEW_STATIONS); return; }
    if (key == '3' || key == 'm' || key == 'M') { Display_Utils::setView(VIEW_MESSAGES); return; }
    if (key == 'x' || key == 'X') { Display_Utils::setView(VIEW_MAP); return; }
    if (key == 'v' || key == 'V') { Display_Utils::setView(VIEW_SATS); return; }
    if (key == 'c' || key == 'C') { Setup_View::enter(); Display_Utils::setView(VIEW_SETUP); return; }
    // Map pan/zoom — checked and consumed before any other letter binding
    // so it can't be shadowed by globals like 'w' (WiFi portal) or
    // 's'/'t'/'m' (view switches). The trackball also feeds into this
    // same key-handling path (see Trackball_Utils in the main loop): its
    // up/down maps to 'i'/'o' (zoom here, field navigation in Setup) and
    // left/right maps to ','/'.', so it works everywhere these letter
    // keys already do without any separate handling.
    // Keyboard: I/O zoom, comma/period pan left/right, semicolon/quote
    // pan up/down, G recentre on GPS.
    if (Display_Utils::getView() == VIEW_MAP) {
        if (key == 'i' || key == 'I') { Map_Utils::zoomIn();       return; }
        if (key == 'o' || key == 'O') { Map_Utils::zoomOut();      return; }
        if (key == 'g' || key == 'G') { Map_Utils::centreOnGPS();  return; }
        if (key == ',') { Map_Utils::panBy(-1, 0); return; }
        if (key == '.') { Map_Utils::panBy(1, 0);  return; }
        if (key == ';') { Map_Utils::panBy(0, -1); return; }
        if (key == '\'') { Map_Utils::panBy(0, 1); return; }
    }
    if (key == '4' || key == 'w') {
        // Launch WiFi settings portal
        if (!WebConfig::isRunning()) {
            WebConfig::begin();
            Display_Utils::showMessage("Setup Portal",
                "Join WiFi '2E0LXY-Tracker-Setup' then browse to 192.168.4.1", TFT_CYAN);
        } else {
            WebConfig::stop();
            Display_Utils::showMessage("Setup Portal", "Stopped", TFT_ORANGE);
        }
        return;
    }
    if (key == 'B' || key == 'b') {
        Beacon_Utils::sendBeacon();
        Display_Utils::showMessage("Beacon", "Manual beacon sent", TFT_GREEN);
        return;
    }
    if (key == '5' || key == 'p') {
        // Cycle operating profile: Walking -> Car -> Bicycle -> Stationary
        int next = (Config.activeProfile + 1) % 4;
        applyOpProfile(next);
        saveConfig();
        Display_Utils::showMessage("Profile", "Now: " + Config.profiles[next].name, TFT_CYAN);
        return;
    }
}
