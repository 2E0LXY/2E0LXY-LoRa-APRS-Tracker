#include "ota_utils.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

static const char* GITHUB_REPO    = "2E0LXY/2E0LXY-LoRa-APRS-Tracker";
static const char* CURRENT_VER    = "1.0.0";
static uint32_t   lastCheckMs     = 0;
static const uint32_t CHECK_INTERVAL = 6UL * 3600UL * 1000UL; // 6 hours

// GitHub API root CA (DigiCert Global Root CA)
static const char* GITHUB_CA = \
"-----BEGIN CERTIFICATE-----\n"
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMB4XDTA2MTExMDAw\n"
"MDAwMFoXDTMxMTExMDAwMDAwMFowYTELMAkGA1UEBhMCVVMxFTATBgNVBAoTDERp\n"
"Z2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3LmRpZ2ljZXJ0LmNvbTEgMB4GA1UEAxMX\n"
"RGlnaUNlcnQgR2xvYmFsIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw\n"
"ggEKAoIBAQDiO+ERct6opP9GsLuLaQEx2sU6yXlz6NJ2FKFJFiW6hHA3AoNqoB5B\n"
"QEQ9MEzAFRDAAF+N6NhBM55kv4ZmFGqyVIDGLDt9RwgJG9lZ29XNYGQsB6RUeTOK\n"
"RYDLC2K5xq6RHLE8XALCNAQZ4T6oAFZnCaA6fMbNHSfHR9/iNLNNFZASJNmZFRkK\n"
"MkqoW80HQID66HJXRDiomjUqiAc7RRrEkFMdAtlQcnQIhBQ0jTiADi6a1C0yblGu\n"
"JgBrIKQb8YUnzgqoFZ0pgXHCnVSi9gTNH5U+e/7jVbQFe95u8B9dxpG5HB5OJEZZ\n"
"FiJoC/OoCM/8oOnhLDiRbgIpAgMBAAGjYzBhMA4GA1UdDwEB/wQEAwIBhjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBQD3lA1VtFMu2bwo+IbG8OXsj3RVTAfBgNV\n"
"HSMEGDAWgBQD3lA1VtFMu2bwo+IbG8OXsj3RVTANBgkqhkiG9w0BAQUFAAOCAQEAp\n"
"kgqHx/aSSj5YUFHqaKePMlKmpvFKIHLX3xPDtHNEfmIgq4gZqixPV3g1K6SzFcaM\n"
"LGfBTBjLCDo8rirCWDDhMH8mDd2sn61FJ7M3oSGCkfVJMkJFIcFv6dqsDoMfkBuF\n"
"b7PXAUVZ0VSmfIUYaGEhOoRhBGVWe9rFMSnNRiwRJKdO7JlPzDSnK3qhEIb+dC84\n"
"-----END CERTIFICATE-----\n";

static bool compareVersions(const String& current, const String& latest) {
    // Simple semver compare: returns true if latest > current
    int c1,c2,c3, l1,l2,l3;
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
    https.setCACert(GITHUB_CA);
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
    client.setCACert(GITHUB_CA);
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
