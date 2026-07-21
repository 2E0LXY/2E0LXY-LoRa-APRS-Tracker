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
#include "board_pins.h"
#include "configuration.h"
#include "gps_utils.h"
#include "lora_utils.h"
#include "aprs_utils.h"
#include "beacon_utils.h"
#include "mqtt_utils.h"
#include "display_utils.h"
#include "keyboard_utils.h"
#include "messaging.h"
#include "ota_utils.h"
#include "webconfig.h"

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

    // GPS
    GPS_Utils::setup();

    // Keyboard
    Keyboard_Utils::setup();

    // LoRa
    if (!LoRa_Utils::setup()) {
        Display_Utils::showMessage("LoRa Error", "SX1262 init failed — check hardware", TFT_RED);
        delay(3000);
    }

    // WiFi (non-blocking)
    if (Config.wifi.enabled && Config.wifi.ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(Config.wifi.ssid.c_str(), Config.wifi.password.c_str());
        Serial.printf("WiFi: connecting to %s\n", Config.wifi.ssid.c_str());
    }

    Serial.printf("Callsign: %s  Freq: %.4f MHz  SF%d\n",
        fullCallsign().c_str(), Config.lora.freq, Config.lora.sf);
}

// ── Main loop ─────────────────────────────────────────────────────────────
void loop() {
    // GPS NMEA
    GPS_Utils::loop();

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

    // Web config portal (if running)
    WebConfig::loop();

    // OTA check (every 6 hours)
    OTA_Utils::loop();
}

// ── LoRa receive handler ──────────────────────────────────────────────────
void handleLoRaRx(const String& packet, float rssi, float snr) {
    Serial.printf("LoRa RX [%.0fdBm SNR%.1f]: %s\n", rssi, snr, packet.c_str());

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

    // ── Compose mode: printable characters go to the buffer FIRST so
    //    digits and 'b' are typable inside a message ─────────────────────
    if (inMessages) {
        if (key == '\r' || key == '\n') {
            String buf = Keyboard_Utils::getBuffer();
            String dest = Messaging::getReplyTarget();
            if (buf.length() == 0) {
                // Enter on empty buffer exits messages view
                Display_Utils::setView(VIEW_STATUS);
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
    if (key == '1') { Display_Utils::setView(VIEW_STATUS);   return; }
    if (key == '2') { Display_Utils::setView(VIEW_STATIONS); return; }
    if (key == '3') { Display_Utils::setView(VIEW_MESSAGES); return; }
    if (key == '4') {
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
}
