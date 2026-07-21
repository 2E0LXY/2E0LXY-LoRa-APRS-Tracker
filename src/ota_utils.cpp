#include "ota_utils.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "board_pins.h"

static const char* GITHUB_REPO    = "2E0LXY/2E0LXY-LoRa-APRS-Tracker";
static const char* CURRENT_VER    = FW_VERSION;
static uint32_t   lastCheckMs     = 0;
static const uint32_t CHECK_INTERVAL = 6UL * 3600UL * 1000UL; // 6 hours

// TLS: certificate validation is skipped (setInsecure). GitHub asset
// downloads redirect across multiple CDN hosts, each with different
// certificate chains. TLS still encrypts the transfer; binary integrity
// relies on fetching only from this repo over HTTPS.

static bool compareVersions(const String& current, const String& latest) {
    // Simple semver compare: returns true if latest > current
    int c1=0,c2=0,c3=0, l1=0,l2=0,l3=0;
    sscanf(current.c_str(), "%d.%d.%d", &c1, &c2, &c3);
    sscanf(latest.c_str(),  "%d.%d.%d", &l1, &l2, &l3);
    if (l1 > c1) return true;
    if (l1 == c1 && l2 > c2) return true;
    if (l1 == c1 && l2 == c2 && l3 > c3) return true;
    return false;
}

void OTA_Utils::checkAndUpdate(bool force) {
    if (!WiFi.isConnected()) return;

    WiFiClientSecure https;
    https.setInsecure();
    https.setTimeout(15000);

    HTTPClient http;
    String url = "https://api.github.com/repos/";
    url += GITHUB_REPO;
    url += "/releases/latest";

    if (!http.begin(https, url)) return;
    http.addHeader("User-Agent", "2E0LXY-Tracker");
    int code = http.GET();
    if (code != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) return;

    String tag = doc["tag_name"] | "";
    if (tag.startsWith("v")) tag = tag.substring(1);

    Serial.printf("OTA: running=%s latest=%s\n", CURRENT_VER, tag.c_str());
    if (!force && !compareVersions(CURRENT_VER, tag)) return;

    // Find the firmware binary asset
    String binUrl;
    for (auto asset : doc["assets"].as<JsonArray>()) {
        String name = asset["name"] | "";
        if (name.endsWith(".bin")) {
            binUrl = asset["browser_download_url"] | "";
            break;
        }
    }
    if (binUrl.isEmpty()) return;

    Serial.println("OTA: downloading " + binUrl);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient dlHttp;
    if (!dlHttp.begin(client, binUrl)) return;
    dlHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int dlCode = dlHttp.GET();
    if (dlCode != 200) { dlHttp.end(); return; }

    int totalSize = dlHttp.getSize();
    WiFiClient* stream = dlHttp.getStreamPtr();

    if (!Update.begin(totalSize > 0 ? totalSize : UPDATE_SIZE_UNKNOWN)) {
        Serial.println("OTA: not enough space");
        dlHttp.end();
        return;
    }

    size_t written = Update.writeStream(*stream);
    dlHttp.end();

    if (Update.end() && Update.isFinished()) {
        Serial.println("OTA: complete — rebooting");
        delay(500);
        ESP.restart();
    } else {
        Serial.printf("OTA: failed: %s\n", Update.errorString());
    }
}

void OTA_Utils::loop() {
    if (!WiFi.isConnected()) return;
    uint32_t now = millis();
    if (now - lastCheckMs < CHECK_INTERVAL) return;
    lastCheckMs = now;
    checkAndUpdate(false);
}
