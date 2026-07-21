# 2E0LXY LoRa APRS Tracker

Standalone firmware for the **LilyGO T-Deck Plus** ESP32-S3 field terminal.

[![Build](https://github.com/2E0LXY/2E0LXY-LoRa-APRS-Tracker/actions/workflows/build.yml/badge.svg)](https://github.com/2E0LXY/2E0LXY-LoRa-APRS-Tracker/actions)

## Hardware

| Component | Detail |
|---|---|
| Board | LilyGO T-Deck Plus (ESP32-S3, 16 MB Flash, 8 MB PSRAM) |
| Radio | SX1262 LoRa |
| Display | ST7789 2.8" TFT 320×240 |
| Input | BlackBerry-style keyboard + trackball |
| GPS | L76K (UART) |
| Storage | MicroSD (map tiles, future) |
| Battery | Internal LiPo, USB-C charging |

## Features

- **UK LoRa APRS** — 439.9125 MHz, SF12, 125 kHz, CR 4/5
- **SmartBeaconing** — speed/heading/distance adaptive, corner-pegging
- **Two-way messaging** — compose and read APRS messages via the keyboard
- **Station list** — live list of heard LoRa and APRS-IS stations with distance/bearing
- **APRS-IS gateway** — connects to `www.aprsnet.uk:14580` via WiFi
- **aprsnet.uk MQTT** — telemetry to the iGate management dashboard (port 1883)
- **OTA updates** — checks GitHub releases every 6 hours; auto-installs over WiFi
- **TFT display** — status view, station list, messages; 1-second refresh

## Quick Start

### Flash (pre-built)
Download `2E0LXY-TDeck-Plus-firmware.bin` from [Releases](../../releases/latest) and flash:
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x0 2E0LXY-TDeck-Plus-firmware.bin
```

### Build from source
```bash
git clone https://github.com/2E0LXY/2E0LXY-LoRa-APRS-Tracker
cd 2E0LXY-LoRa-APRS-Tracker
pio run -e t-deck-plus
pio run -e t-deck-plus -t upload
```

## Configuration

Edit `/config.json` on the LittleFS partition **or** modify `include/configuration.h` defaults before building:

```json
{
  "aprs":   { "callsign": "2E0LXY", "ssid": 9, "comment": "T-Deck Plus" },
  "lora":   { "freq": 439.9125, "sf": 12, "bw": 125, "cr": 5, "power": 17 },
  "wifi":   { "ssid": "YourSSID", "password": "YourPass" },
  "mqtt":   { "active": true, "server": "80.64.216.113", "port": 1883,
              "topic": "aprsnet", "user": "2E0LXY", "pass": "member_password" },
  "beacon": { "smart": true, "slow": 300, "fast": 30, "speed_th": 5, "turn": 30 }
}
```

## Keyboard Shortcuts

| Key | Action |
|---|---|
| `1` | Status view |
| `2` | Station list |
| `3` | Messages view |
| `B` | Force beacon |
| `↵` (in messages) | Send composed message |
| `⌫` | Backspace in compose |

## aprsnet.uk Integration

When `mqtt.active = true`, the tracker appears in **Member Settings → 📡 My LoRa APRS iGate Devices** on [aprsnet.uk](https://www.aprsnet.uk) with:
- Live telemetry (GPS fix, speed, sats, RX/TX counts, uptime, heap)
- Remote **Restart** and **Force Beacon** commands

**Note:** Use the direct VPS IP `80.64.216.113` for MQTT — `www.aprsnet.uk` passes through Cloudflare which does not proxy TCP port 1883.

## Roadmap

- [ ] Offline tile map from MicroSD (`.mbtiles`)
- [ ] Live station plotting on map with trackball pan/zoom
- [ ] WebUI config panel (WiFi AP mode on boot)
- [ ] Weather APRS object (external BME280 via I2C)
- [ ] GPX track recording to SD card
- [ ] Winlink gateway

## Related

- [2E0LXY LoRa APRS iGate](https://github.com/2E0LXY/2E0LXY-LoRa-APRS-iGate) — fixed station iGate firmware
- [APRS Net UK](https://www.aprsnet.uk) — live map, messaging, iGate management

## Licence
GNU GPL v3 — see [LICENSE](LICENSE)
