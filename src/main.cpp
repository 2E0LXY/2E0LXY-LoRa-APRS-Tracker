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

    // Keyboard input
    if (Keyboard_Utils::available()) {
        char k = Keyboard_Utils::getKey();
        if (k) handleKeyInput(k);
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
            Display_Utils::setView(VIEW_STATIONS);
            return;
        }
        Setup_View::handleKey(key);
        return;
    }

    // Delete/backspace outside compose mode always returns to the home
    // screen (Stations — callsigns heard). Inside Messages it backspaces
    // the typed text as before — except on an already-empty buffer, where
    // there's nothing left to delete, so it also returns home instead of
    // doing nothing.
    if (isDelete && !inMessages) {
        Display_Utils::setView(VIEW_STATIONS);
        return;
    }
    if (isDelete && inMessages && Keyboard_Utils::getBuffer().length() == 0) {
        Display_Utils::setView(VIEW_STATIONS);
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
                Display_Utils::setView(VIEW_STATIONS);
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
    // 's'/'t'/'m' (view switches). Trackball isn't wired to firmware yet
    // (GPIOs are defined in board_pins.h but unused — see TBOX_* pins),
    // so this is keyboard-only for now: I/O zoom, comma/period pan
    // left/right, semicolon/quote pan up/down, G recentre on GPS.
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
