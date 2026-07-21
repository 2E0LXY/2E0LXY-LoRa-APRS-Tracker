#include "ota_utils.h"
#include "configuration.h"
#include "display_utils.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#define GITHUB_REPO  "2E0LXY/2E0LXY-LoRa-APRS-Tracker"
#define ASSET_NAME   "2E0LXY-TDeck-Plus-firmware.bin"
#define CHECK_INTERVAL_MS  (6UL * 3600UL * 1000UL)  // every 6 hours

static uint32_t lastCheckMs = 0;
static String   lastCheckedTag;

static String getLatestTag() {
    if (!WiFi.isConnected()) return "";
    WiFiClientSecure client;
    client.setInsecure(); // accept any cert for GitHub API
    HTTPClient http;
    String url = "https://api.github.com/repos/" GITHUB_REPO "/releases/latest";
    http.begin(client, url);
    http.addHeader("User-Agent", "2E0LXY-Tracker");
    http.addHeader("Accept", "application/vnd.github+json");
    int code = http.GET();
    if (code != 200) { http.end(); return ""; }
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    http.end();
    return doc["tag_name"] | "";
}

static String getAssetURL(const String& tag) {
    return "https://github.com/" GITHUB_REPO "/releases/download/"
           + tag + "/" ASSET_NAME;
}

static bool performUpdate(const String& url) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    int size = http.getSize();
    if (size <= 0) { http.end(); return false; }

    Display_Utils::showMessage("OTA Update", "Downloading firmware...", 0xFBE0);

    WiFiClient* stream = http.getStreamPtr();
    if (!Update.begin(size, U_FLASH)) {
        http.end();
        return false;
    }

    uint8_t buf[1024];
    int written = 0;
    while (http.connected() && written < size) {
        int available = stream->available();
        if (available) {
            int r = stream->readBytes(buf, min((int)sizeof(buf), available));
            Update.write(buf, r);
            written += r;
        }
        delay(1);
    }
    http.end();

    if (!Update.end()) return false;
    if (!Update.isFinished()) return false;
    return true;
}

void OTA_Utils::checkAndUpdate(bool force) {
    if (!WiFi.isConnected()) return;
    String latest = getLatestTag();
    if (latest.isEmpty()) return;
    if (!force && latest == lastCheckedTag) return;
    lastCheckedTag = latest;

    // Compare version: running vs latest
    String running = "v" + String(Config.fwVersion);
    if (!force && running == latest) {
        Serial.println("OTA: firmware is current (" + latest + ")");
        return;
    }

    Serial.println("OTA: update available — " + running + " → " + latest);
    Display_Utils::showMessage("OTA Update", latest + " available", 0x07FF);
    delay(2000);

    String assetURL = getAssetURL(latest);
    if (performUpdate(assetURL)) {
        Display_Utils::showMessage("OTA Update", "Done! Rebooting...", 0x07E0);
        delay(2000);
        ESP.restart();
    } else {
        Display_Utils::showMessage("OTA Update", "Failed — will retry later", 0xF800);
    }
}

void OTA_Utils::loop() {
    if (millis() - lastCheckMs < CHECK_INTERVAL_MS) return;
    lastCheckMs = millis();
    checkAndUpdate(false);
}
