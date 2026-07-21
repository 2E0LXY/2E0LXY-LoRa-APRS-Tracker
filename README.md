# 2E0LXY LoRa APRS Tracker

Standalone firmware for the **LilyGO T-Deck Plus** — a fully independent LoRa APRS field terminal with two-way messaging, live station tracking and [aprsnet.uk](https://www.aprsnet.uk/) integration.

![LilyGO T-Deck Plus](https://www.lilygo.cc/cdn/shop/files/T-DECK-PLUS.jpg)

## Features

| Feature | Status |
|---|---|
| SX1262 LoRa APRS TX/RX (UK 439.9125 MHz) | ✅ |
| SmartBeaconing with corner-pegging | ✅ |
| Two-way APRS messaging (keyboard input) | ✅ |
| Live station list (LoRa RF + APRS-IS) | ✅ |
| APRS-IS via WiFi (www.aprsnet.uk:14580) | ✅ |
| aprsnet.uk MQTT telemetry + remote control | ✅ |
| ST7789 TFT — status / stations / messages | ✅ |
| L76K GPS — SmartBeacon position | ✅ |
| MicroSD ready (offline map — Phase 2) | 🔲 |
| OTA update from GitHub releases | ✅ |

## Hardware

**LilyGO T-Deck Plus** (ESP32-S3, 16MB Flash, 8MB PSRAM)
- 2.8" ST7789 TFT 320×240
- Blackberry-style keyboard (I2C 0x55)
- Trackball (GPIO 1,2,3,15)
- SX1262 LoRa (SPI)
- L76K GPS (UART1: RX=44 TX=43)
- MicroSD (SPI shared)

## Quick start

### Flash pre-built binary
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x0 2E0LXY-TDeck-Plus-firmware.bin
```

Or use the [web installer](https://2e0lxy.github.io/2E0LXY-LoRa-APRS-Tracker/).

### Build from source
```bash
git clone https://github.com/2E0LXY/2E0LXY-LoRa-APRS-Tracker.git
cd 2E0LXY-LoRa-APRS-Tracker
pio run -e t-deck-plus
pio run -t uploadfs    # upload LittleFS (config.json)
```

## First-time setup

On first boot the device loads default config from LittleFS.  
Edit `data/config.json` before flashing, or connect to the WebUI after boot:

1. Device creates WiFi AP `2E0LXY-Tracker` / `aprsnet123`
2. Connect and browse to `192.168.4.1`
3. Set callsign, WiFi credentials, MQTT credentials
4. Save — device reboots with new settings

## aprsnet.uk MQTT integration

Once a member account exists at [aprsnet.uk](https://www.aprsnet.uk/), add credentials to `config.json`:

```json
"mqtt": {
  "active": true,
  "server": "80.64.216.113",
  "port": 1883,
  "topic": "aprsnet",
  "user": "YOUR_CALLSIGN",
  "pass": "YOUR_MEMBER_PASSWORD"
}
```

The device then appears in **Member Settings → 📡 My LoRa APRS iGate Devices** with live telemetry and remote control (restart, force beacon, status).

## Keyboard shortcuts

| Key | Action |
|---|---|
| `1` | Status view |
| `2` | Stations view |
| `3` | Messages view |
| `B` | Force beacon now |
| `Enter` (in messages) | Send composed message |
| `Backspace` | Delete last character |

## Frequency (UK LoRa APRS)

| Parameter | Value |
|---|---|
| Frequency | 439.9125 MHz |
| Spreading Factor | SF12 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |
| Preamble | 8 |
| TX Power | 17 dBm (default) |

## Licence

Copyright © 2026 2E0LXY  
GNU General Public License v3.0 — see [LICENSE](LICENSE)
