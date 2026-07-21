#include "ble_kiss.h"
#include "lora_utils.h"
#include "configuration.h"
#include <NimBLEDevice.h>

// Nordic UART Service — the de-facto "serial over BLE" APRSdroid speaks.
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // phone → device (write)
#define NUS_TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // device → phone (notify)

// KISS special bytes
static const uint8_t KISS_FEND  = 0xC0;
static const uint8_t KISS_FESC  = 0xDB;
static const uint8_t KISS_TFEND = 0xDC;
static const uint8_t KISS_TFESC = 0xDD;

static NimBLEServer*         server   = nullptr;
static NimBLECharacteristic* txChar   = nullptr;
static bool                  connected = false;
static std::vector<uint8_t>  rxBuf;    // accumulates a KISS frame from phone

// Decode a completed KISS frame (between FENDs) → raw AX.25/APRS payload,
// then transmit over LoRa. We only handle command 0x00 (data frame).
static void handleKissFrame(const std::vector<uint8_t>& frame) {
    if (frame.empty()) return;
    // First byte = KISS command/port. 0x00 = data on port 0.
    if ((frame[0] & 0x0F) != 0x00) return;
    // Un-escape the payload
    std::string payload;
    for (size_t i = 1; i < frame.size(); i++) {
        uint8_t b = frame[i];
        if (b == KISS_FESC && i + 1 < frame.size()) {
            uint8_t n = frame[++i];
            payload += (n == KISS_TFEND) ? (char)KISS_FEND :
                       (n == KISS_TFESC) ? (char)KISS_FESC : (char)n;
        } else {
            payload += (char)b;
        }
    }
    // Send over LoRa (payload is the APRS frame body)
    LoRa_Utils::sendPacket(String(payload.c_str()));
}

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string v = c->getValue();
        for (uint8_t b : v) {
            if (b == KISS_FEND) {
                if (!rxBuf.empty()) { handleKissFrame(rxBuf); rxBuf.clear(); }
            } else {
                rxBuf.push_back(b);
                if (rxBuf.size() > 512) rxBuf.clear();  // guard
            }
        }
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*) override    { connected = true;  Serial.println("BLE KISS: phone connected"); }
    void onDisconnect(NimBLEServer* s) override {
        connected = false; Serial.println("BLE KISS: phone disconnected");
        NimBLEDevice::startAdvertising();
    }
};

void BLE_KISS::begin() {
    String name = "2E0LXY-TNC " + Config.aprs.callsign;
    NimBLEDevice::init(name.c_str());
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService* svc = server->createService(NUS_SERVICE_UUID);
    txChar = svc->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic* rxChar =
        svc->createCharacteristic(NUS_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rxChar->setCallbacks(new RxCallbacks());

    svc->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE_UUID);
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    Serial.println("BLE KISS TNC advertising as: " + name);
}

// Wrap a raw APRS frame in KISS and notify the phone.
void BLE_KISS::sendToPhone(const uint8_t* data, size_t len) {
    if (!connected || !txChar) return;
    std::string out;
    out += (char)KISS_FEND;
    out += (char)0x00;  // data frame, port 0
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        if (b == KISS_FEND)      { out += (char)KISS_FESC; out += (char)KISS_TFEND; }
        else if (b == KISS_FESC) { out += (char)KISS_FESC; out += (char)KISS_TFESC; }
        else                     { out += (char)b; }
    }
    out += (char)KISS_FEND;
    txChar->setValue((uint8_t*)out.data(), out.size());
    txChar->notify();
}

void BLE_KISS::sendToPhone(const String& frame) {
    sendToPhone((const uint8_t*)frame.c_str(), frame.length());
}

bool BLE_KISS::isConnected() { return connected; }
void BLE_KISS::loop() { /* NimBLE runs in its own task; nothing needed here */ }
