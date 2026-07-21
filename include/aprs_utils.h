#pragma once
#include <Arduino.h>
#include <vector>

#define MAX_STATIONS 64

struct ParsedPacket {
    String fromCall, toCall, path, text, msgID, symbol, comment, raw;
    float  lat = 0, lon = 0;
    float  rssi = 0, snr = 0;
    char   type = 0;
    bool   valid = false, hasPosition = false, isMessage = false;
};

struct HeardStation {
    String   callsign, symbol, comment;
    float    lat = 0, lon = 0;
    float    rssi = 0, snr = 0;
    uint32_t lastHeardMs = 0;
};

using StationList = std::vector<HeardStation>;

namespace APRS_Utils {
    extern StationList heardStations;

    // Packet builders
    String buildPositionPacket();
    String buildMessagePacket(const String& toCall, const String& text, int msgID = 0);
    String buildAckPacket(const String& toCall, const String& msgID);

    // Parser
    ParsedPacket parsePacket(const String& raw);

    // APRS-IS connection
    void   connect();
    void   disconnect();
    bool   isConnected();
    void   loop();
    bool   sendPacketIS(const String& packet);

    // Station management
    void   updateStation(const String& call, const ParsedPacket& p);
    float  distanceKm(float lat1, float lon1, float lat2, float lon2);
    float  bearingDeg(float lat1, float lon1, float lat2, float lon2);

    uint32_t getRxCount();
    uint32_t getTxCount();

    // Callbacks (override in main)
    void onMessageReceived(const ParsedPacket& p);
}
