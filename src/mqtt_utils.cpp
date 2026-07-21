#include "mqtt_utils.h"
#include "configuration.h"
#include "gps_utils.h"
#include "beacon_utils.h"
#include "lora_utils.h"
#include "aprs_utils.h"
#include "ota_utils.h"
#include "messaging.h"
#include "display_utils.h"
#include "board_pins.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

static WiFiClient  mqttWifi;
static PubSubClient pubSub(mqttWifi);
static uint32_t lastConnectMs    = 0;
static uint32_t lastTelemetryMs  = 0;

static String baseTopic() {
    return Config.mqtt.topic + "/" + Config.mqtt.username + "/" + fullCallsign();
}

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    String t(topic), p((char*)payload, length);

    // Incoming message via server route: aprsnet/{owner}/{device}/inbox
    if (t.endsWith("/inbox")) {
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, p) == DeserializationError::Ok) {
            String from = doc["from"] | "";
            String text = doc["text"] | "";
            if (from.length() && text.length()) {
                Serial.println("MQTT inbox: " + from + " -> " + text);
                Messaging::receive(from, text, "");   // no msgID for server-route msgs
                Display_Utils::showMessage("MSG (server): " + from, text, TFT_CYAN);
            }
        }
        return;
    }

    // Commands: aprsnet/{owner}/{device}/cmd
    Serial.println("MQTT CMD: " + t + " → " + p);
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, p) == DeserializationError::Ok) {
        String cmd = doc["cmd"] | "";
        if (cmd == "restart")  ESP.restart();
        if (cmd == "beacon")   Beacon_Utils::sendBeacon();
        if (cmd == "telemetry") MQTT_Utils::publishTelemetry();
        if (cmd == "update")   OTA_Utils::checkAndUpdate(true);  // remote-triggered OTA
    }
}

bool MQTT_Utils::connect() {
    if (!Config.mqtt.active || !WiFi.isConnected()) return false;
    if (pubSub.connected()) return true;
    uint32_t now = millis();
    if (now - lastConnectMs < 30000) return false;
    lastConnectMs = now;

    pubSub.setServer(Config.mqtt.server.c_str(), Config.mqtt.port);
    pubSub.setCallback(onMqttMessage);
    pubSub.setBufferSize(1024);

    bool ok = (!Config.mqtt.username.isEmpty())
        ? pubSub.connect(fullCallsign().c_str(),
                         Config.mqtt.username.c_str(),
                         Config.mqtt.password.c_str())
        : pubSub.connect(fullCallsign().c_str());

    if (ok) {
        String cmdTopic = baseTopic() + "/cmd";
        pubSub.subscribe(cmdTopic.c_str());
        String inboxTopic = baseTopic() + "/inbox";
        pubSub.subscribe(inboxTopic.c_str());
        publishTelemetry();
        Serial.println("MQTT: connected to " + Config.mqtt.server);
    }
    return ok;
}

void MQTT_Utils::publishTelemetry() {
    if (!pubSub.connected()) return;
    StaticJsonDocument<256> doc;
    doc["fw"]        = FW_VERSION;
    doc["uptime"]    = millis() / 1000;
    doc["heap"]      = ESP.getFreeHeap();
    doc["gps_fix"]   = GPS_Utils::hasFix();
    doc["sats"]      = GPS_Utils::sats();
    doc["speed_kph"] = GPS_Utils::speedKph();
    doc["rx"]        = LoRa_Utils::getRxCount();
    doc["tx"]        = LoRa_Utils::getTxCount();
    doc["beacons"]   = Beacon_Utils::getCount();
    doc["batt_v"]    = Display_Utils::batteryVolts();
    doc["batt_pct"]  = Display_Utils::batteryPercent();
    if (GPS_Utils::hasFix()) {
        doc["lat"] = GPS_Utils::lat();
        doc["lon"] = GPS_Utils::lon();
        doc["alt"] = GPS_Utils::altM();
    }
    char buf[256];
    serializeJson(doc, buf);
    pubSub.publish((baseTopic() + "/telemetry").c_str(), buf);
}

// Send a message to another station via the aprsnet.uk server (store-and-forward).
// Publishes to aprsnet/{owner}/{device}/message — the server delivers it exactly
// like the website's "direct" route, no RF needed. Returns false if not connected.
bool MQTT_Utils::publishMessage(const String& to, const String& text) {
    if (!pubSub.connected()) return false;
    StaticJsonDocument<256> doc;
    doc["to"]   = to;
    doc["text"] = text;
    char buf[256];
    size_t n = serializeJson(doc, buf);
    String topic = baseTopic() + "/message";
    return pubSub.publish(topic.c_str(), (const uint8_t*)buf, n, false);
}

void MQTT_Utils::loop() {
    if (!Config.mqtt.active) return;
    if (!pubSub.connected()) { connect(); return; }
    pubSub.loop();
    if (millis() - lastTelemetryMs > 60000) {
        lastTelemetryMs = millis();
        publishTelemetry();
    }
}

bool MQTT_Utils::isConnected() { return pubSub.connected(); }
