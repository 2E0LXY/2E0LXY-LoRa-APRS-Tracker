#include "satellites_utils.h"
#include "gps_utils.h"

namespace {
    // Each constellation's GSV sentence carries up to 4 satellites per
    // message, several messages per full sweep. We track up to 4 "slots"
    // per talker ID and re-read them every time a message completes;
    // between messages the previous sweep's data stays visible so the
    // sky plot doesn't flicker empty every second.
    struct ConstellationWatch {
        const char* talkerGSV;   // e.g. "GPGSV"
        GnssConstellation kind;
        TinyGPSCustom msgTotal, msgNum, satsInView;
        TinyGPSCustom prn[4], elev[4], azim[4], snr[4];
    };

    // GP=GPS, GL=GLONASS, GB=BeiDou (also seen as BD on some firmwares),
    // GQ=QZSS. The L76K uses these four.
    ConstellationWatch watches[4] = {
        { "GPGSV", GnssConstellation::GPS },
        { "GLGSV", GnssConstellation::GLONASS },
        { "GBGSV", GnssConstellation::BEIDOU },
        { "GQGSV", GnssConstellation::QZSS },
    };

    std::vector<SatelliteInfo> satList;
    uint32_t lastSweepMs = 0;

    void initWatch(ConstellationWatch& w) {
        w.msgTotal.begin(gps, w.talkerGSV, 1);
        w.msgNum.begin(gps, w.talkerGSV, 2);
        w.satsInView.begin(gps, w.talkerGSV, 3);
        for (int i = 0; i < 4; i++) {
            w.prn[i].begin(gps, w.talkerGSV, 4 + 4 * i);
            w.elev[i].begin(gps, w.talkerGSV, 5 + 4 * i);
            w.azim[i].begin(gps, w.talkerGSV, 6 + 4 * i);
            w.snr[i].begin(gps, w.talkerGSV, 7 + 4 * i);
        }
    }

    // Called whenever a constellation's GSV message completes — folds its
    // 4 satellite slots into satList, replacing any stale entry for the
    // same PRN+constellation (a satellite can appear in more than one GSV
    // sweep as it moves in/out of view across message numbers).
    void ingest(ConstellationWatch& w) {
        if (!w.msgNum.isUpdated()) return;
        for (int i = 0; i < 4; i++) {
            if (!w.prn[i].isUpdated() || strlen(w.prn[i].value()) == 0) continue;
            SatelliteInfo info;
            info.prn = atoi(w.prn[i].value());
            info.elevation = atoi(w.elev[i].value());
            info.azimuth = atoi(w.azim[i].value());
            info.snr = strlen(w.snr[i].value()) ? atoi(w.snr[i].value()) : 0;
            info.constellation = w.kind;

            bool replaced = false;
            for (auto& existing : satList) {
                if (existing.prn == info.prn && existing.constellation == info.constellation) {
                    existing = info;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) satList.push_back(info);
        }
        lastSweepMs = millis();
    }
}

void Satellites_Utils::setup() {
    for (auto& w : watches) initWatch(w);
}

void Satellites_Utils::loop() {
    for (auto& w : watches) ingest(w);

    // Drop satellites we haven't heard about in a full sweep cycle+margin
    // (~15s) — they've gone out of view or the module stopped reporting
    // them, so don't leave stale dots on the sky plot forever.
    if (millis() - lastSweepMs > 15000 && !satList.empty()) {
        satList.clear();
    }
}

const std::vector<SatelliteInfo>& Satellites_Utils::list() { return satList; }

int Satellites_Utils::countByConstellation(GnssConstellation c) {
    int n = 0;
    for (auto& s : satList) if (s.constellation == c) n++;
    return n;
}

const char* Satellites_Utils::constellationName(GnssConstellation c) {
    switch (c) {
        case GnssConstellation::GPS:     return "GPS";
        case GnssConstellation::GLONASS: return "GLONASS";
        case GnssConstellation::BEIDOU:  return "BeiDou";
        case GnssConstellation::QZSS:    return "QZSS";
        default:                         return "?";
    }
}

uint16_t Satellites_Utils::constellationColour(GnssConstellation c) {
    switch (c) {
        case GnssConstellation::GPS:     return 0x07E0;  // green
        case GnssConstellation::GLONASS: return 0xFBE0;  // amber
        case GnssConstellation::BEIDOU:  return 0xF800;  // red
        case GnssConstellation::QZSS:    return 0x04FF;  // cyan
        default:                         return 0x5AEB;  // grey
    }
}
