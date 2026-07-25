#pragma once
#include <Arduino.h>
#include <vector>

// Per-satellite sky position tracking (elevation/azimuth/SNR/PRN) parsed
// from $GxGSV NMEA sentences, across every constellation the L76K module
// reports (GPS, GLONASS, BeiDou, QZSS). Not tracked by TinyGPS++ core —
// see gps_utils.cpp for the basic fix/sat-count data that IS tracked
// there; this module is purely for the sky-plot view.
enum class GnssConstellation { GPS, GLONASS, BEIDOU, QZSS, UNKNOWN };

struct SatelliteInfo {
    int  prn = 0;
    int  elevation = 0;   // 0-90 degrees above horizon
    int  azimuth   = 0;   // 0-359 degrees from true north
    int  snr       = 0;   // 0-99 dB-Hz, 0 = not tracking (in view but no signal)
    GnssConstellation constellation = GnssConstellation::UNKNOWN;
};

namespace Satellites_Utils {
    void setup();
    void loop();
    const std::vector<SatelliteInfo>& list();
    int  countByConstellation(GnssConstellation c);
    const char* constellationName(GnssConstellation c);
    uint16_t constellationColour(GnssConstellation c);   // RGB565, for the sky plot
}
