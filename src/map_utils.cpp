#include "map_utils.h"
#include "board_pins.h"
#include "configuration.h"
#include "gps_utils.h"
#include "aprs_utils.h"
#include "display_utils.h"
#include "aprs_icons.h"
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include <SD.h>
#include <SPI.h>
#include <math.h>

namespace {
    PNG png;
    File pngFile;

    // Map view state
    int   zoom = 15;
    double centreLat = 53.6833;   // Wakefield-ish default until GPS/first station fixes it
    double centreLon = -1.4977;
    bool   centred = false;

    const int TILE_PX = 256;
    // Drawing area: full width, below the 20px status bar, above a small
    // footer — matches the layout convention used by the other views.
    const int MAP_TOP = 20;
    const int MAP_H = 220 - 12;   // leave 12px for the footer hint

    // ── Web Mercator tile math (standard slippy-map) ─────────────────────
    void lonLatToTileF(double lon, double lat, int z, double& tx, double& ty) {
        double latRad = lat * PI / 180.0;
        double n = pow(2.0, z);
        tx = (lon + 180.0) / 360.0 * n;
        ty = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / PI) / 2.0 * n;
    }

    void lonLatToPixel(double lon, double lat, int& px, int& py) {
        double ctx, cty;
        lonLatToTileF(centreLon, centreLat, zoom, ctx, cty);
        double tx, ty;
        lonLatToTileF(lon, lat, zoom, tx, ty);
        // Screen centre
        int cx = SCREEN_WIDTH / 2;
        int cy = MAP_TOP + MAP_H / 2;
        px = cx + (int)round((tx - ctx) * TILE_PX);
        py = cy + (int)round((ty - cty) * TILE_PX);
    }

    // ── PNGdec <-> SD file callbacks ──────────────────────────────────────
    void* pngOpen(const char* filename, int32_t* size) {
        pngFile = SD.open(filename, FILE_READ);
        if (!pngFile) return nullptr;
        *size = pngFile.size();
        return &pngFile;
    }
    void pngClose(void* handle) {
        File* f = (File*)handle;
        if (f) f->close();
    }
    int32_t pngRead(PNGFILE* page, uint8_t* buf, int32_t len) {
        File* f = (File*)page->fHandle;
        if (!f) return 0;
        return f->read(buf, len);
    }
    int32_t pngSeek(PNGFILE* page, int32_t pos) {
        File* f = (File*)page->fHandle;
        if (!f) return 0;
        return f->seek(pos) ? pos : -1;
    }

    // Where the current tile should be blitted on screen (set before decode).
    int drawOriginX = 0, drawOriginY = 0;

    int pngDraw(PNGDRAW* draw) {
        uint16_t lineBuf[TILE_PX];
        png.getLineAsRGB565(draw, lineBuf, PNG_RGB565_BIG_ENDIAN, 0xFFFFFFFF);
        int y = drawOriginY + draw->y;
        if (y < MAP_TOP || y >= MAP_TOP + MAP_H) return 1;   // clip above/below the map area, keep decoding
        // TFT_eSPI's tft object isn't exposed outside display_utils.cpp;
        // drawing goes through the shared helper below.
        Display_Utils::mapPushTileLine(drawOriginX, y, TILE_PX, lineBuf);
        return 1;   // continue decoding remaining lines
    }

    // Draws one 256x256 tile at screen (x,y), clipping to the map area.
    void drawTile(int z, int tx, int ty, int x, int y) {
        char path[48];
        snprintf(path, sizeof(path), "/tiles/%d/%d/%d.png", z, tx, ty);
        if (!SD.exists(path)) return;   // not cached — leave background showing
        drawOriginX = x;
        drawOriginY = y;
        int rc = png.open(path, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
        if (rc == PNG_SUCCESS) {
            png.decode(nullptr, 0);
            png.close();
        }
    }

    // Look up the icon array + palette for a 2-char APRS symbol ("table"+"code").
    const uint16_t* iconFor(const String& sym) {
        if (sym.length() < 2) return nullptr;
        char table = sym[0];
        char code  = sym[1];
        int idx = (int)code - 33;
        if (idx < 0 || idx >= APRS_ICON_COUNT) return nullptr;
        return (table == '/') ? APRS_ICONS_PRIMARY[idx] : APRS_ICONS_ALT[idx];
    }
}

void Map_Utils::setup() {
    // SD is brought up on demand (draw()) since it shares the bus with LoRa/TFT.
}

void Map_Utils::draw() {
    if (!centred) {
        if (GPS_Utils::hasFix()) {
            centreLat = GPS_Utils::lat();
            centreLon = GPS_Utils::lon();
            centred = true;
        }
    }

    // Bring up SD (shared SPI bus) — cheap no-op if already begun.
    static bool sdReady = false;
    if (!sdReady) {
        sdReady = SD.begin(BOARD_SD_CS, SPI, 20000000);
        if (!sdReady) {
            Display_Utils::mapDrawNoCard();
            return;
        }
    }

    Display_Utils::mapClearArea(MAP_TOP, MAP_H);

    // Which tiles cover the visible area, given the current centre.
    double ctx, cty;
    lonLatToTileF(centreLon, centreLat, zoom, ctx, cty);
    int baseTx = (int)floor(ctx);
    int baseTy = (int)floor(cty);
    int cx = SCREEN_WIDTH / 2;
    int cy = MAP_TOP + MAP_H / 2;
    int originX = cx - (int)((ctx - baseTx) * TILE_PX);
    int originY = cy - (int)((cty - baseTy) * TILE_PX);

    // Enough tiles either side to cover the screen at 256px each.
    int tilesX = (SCREEN_WIDTH / TILE_PX) + 2;
    int tilesY = (MAP_H / TILE_PX) + 2;
    for (int j = -1; j < tilesY; j++) {
        for (int i = -1; i < tilesX; i++) {
            int tx = baseTx + i, ty = baseTy + j;
            int sx = originX + i * TILE_PX;
            int sy = originY + j * TILE_PX;
            if (sx > SCREEN_WIDTH || sy > MAP_TOP + MAP_H) continue;
            if (sx + TILE_PX < 0 || sy + TILE_PX < MAP_TOP) continue;
            drawTile(zoom, tx, ty, sx, sy);
        }
    }

    // Overlay heard stations as APRS symbol icons.
    for (auto& s : APRS_Utils::heardStations) {
        if (s.lat == 0 && s.lon == 0) continue;
        int px, py;
        lonLatToPixel(s.lon, s.lat, px, py);
        if (px < -APRS_ICON_SIZE || px > SCREEN_WIDTH || py < MAP_TOP - APRS_ICON_SIZE || py > MAP_TOP + MAP_H)
            continue;
        const uint16_t* icon = iconFor(s.symbol);
        if (!icon) continue;
        Display_Utils::mapPushIcon(px - APRS_ICON_SIZE / 2, py - APRS_ICON_SIZE, icon,
                                    APRS_ICON_SIZE, APRS_ICON_TRANSPARENT);
    }

    // Own position — small crosshair, distinct from station icons.
    if (GPS_Utils::hasFix()) {
        int px, py;
        lonLatToPixel(GPS_Utils::lon(), GPS_Utils::lat(), px, py);
        Display_Utils::mapDrawCrosshair(px, py);
    }

    Display_Utils::mapDrawFooter(MAP_TOP + MAP_H, zoom);
}

void Map_Utils::panBy(int dxTiles, int dyTiles) {
    double ctx, cty;
    lonLatToTileF(centreLon, centreLat, zoom, ctx, cty);
    ctx += dxTiles;
    cty += dyTiles;
    double n = pow(2.0, zoom);
    double lonDeg = ctx / n * 360.0 - 180.0;
    double latRad = atan(sinh(PI * (1.0 - 2.0 * cty / n)));
    centreLat = latRad * 180.0 / PI;
    centreLon = lonDeg;
    centred = true;   // manual pan overrides GPS auto-centre
}

void Map_Utils::zoomIn()  { if (zoom < 19) zoom++; }
void Map_Utils::zoomOut() { if (zoom > 2)  zoom--; }

void Map_Utils::centreOnGPS() {
    if (GPS_Utils::hasFix()) {
        centreLat = GPS_Utils::lat();
        centreLon = GPS_Utils::lon();
        centred = true;
    }
}

void Map_Utils::centreOnStation(const String& callsign) {
    for (auto& s : APRS_Utils::heardStations) {
        if (s.callsign == callsign && (s.lat != 0 || s.lon != 0)) {
            centreLat = s.lat;
            centreLon = s.lon;
            centred = true;
            return;
        }
    }
}
