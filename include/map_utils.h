#pragma once
#include <Arduino.h>

// On-device slippy map: renders cached OSM/similar tiles from the SD card
// (written by the aprsnet.uk website's "Map Downloader" tool via USB Mass
// Storage — key U/6) plus APRS symbol icons for heard stations, matching
// the same hessu symbol set used by the website/Android/desktop clients.
namespace Map_Utils {
    void setup();
    // Draws the current map view (centre station or GPS fix, current
    // stations overlay) into the screen area below the status bar.
    void draw();
    // Trackball/keyboard pan and zoom.
    void panBy(int dxTiles, int dyTiles);
    void zoomIn();
    void zoomOut();
    void centreOnGPS();
    void centreOnStation(const String& callsign);
}
