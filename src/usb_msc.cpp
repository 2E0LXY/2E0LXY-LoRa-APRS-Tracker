#include "usb_msc.h"
#include "board_pins.h"
#include <SD.h>
#include <SPI.h>

// USB MSC is only available on ESP32-S3 (native USB). Guard so the file
// compiles even if the target somehow lacks it.
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(ARDUINO_USB_MODE)
#include "USB.h"
#include "USBMSC.h"

static USBMSC msc;
static bool   active = false;
static uint32_t cardSectors = 0;
static const uint16_t SECTOR_SIZE = 512;

// Host reads `bufsize` bytes from the card starting at (lba*512 + offset).
static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    uint32_t count = bufsize / SECTOR_SIZE;
    if (!SD.readRAW((uint8_t*)buffer, lba)) return -1;   // one sector per call is simplest
    // If more than one sector requested, read the rest
    for (uint32_t i = 1; i < count; i++) {
        if (!SD.readRAW((uint8_t*)buffer + i * SECTOR_SIZE, lba + i)) return -1;
    }
    return count * SECTOR_SIZE;
}

// Host writes `bufsize` bytes to the card.
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    uint32_t count = bufsize / SECTOR_SIZE;
    for (uint32_t i = 0; i < count; i++) {
        if (!SD.writeRAW(buffer + i * SECTOR_SIZE, lba + i)) return -1;
    }
    return count * SECTOR_SIZE;
}

// Host started/stopped the unit (eject). Return true to allow.
static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    if (load_eject && !start) {
        // Host ejected — safe to tear down
        active = false;
    }
    return true;
}

bool USB_MSC::begin() {
    // Bring up SPI + SD (shared bus). LoRa/TFT already share these pins but
    // during MSC mode the radio is idle, so we can own the bus.
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI, BOARD_SD_CS);
    if (!SD.begin(BOARD_SD_CS, SPI, 20000000)) {
        Serial.println("USB MSC: no SD card");
        return false;
    }

    uint64_t sizeBytes = SD.cardSize();
    cardSectors = (uint32_t)(sizeBytes / SECTOR_SIZE);
    Serial.printf("USB MSC: card %llu MB (%u sectors)\n", sizeBytes / (1024ULL*1024ULL), cardSectors);

    msc.vendorID("2E0LXY");
    msc.productID("T-Deck-SD");
    msc.productRevision("1.0");
    msc.onRead(onRead);
    msc.onWrite(onWrite);
    msc.onStartStop(onStartStop);
    msc.mediaPresent(true);
    msc.begin(cardSectors, SECTOR_SIZE);

    USB.begin();
    active = true;
    Serial.println("USB MSC: active — SD card presented to host");
    return true;
}

bool USB_MSC::isActive() { return active; }

void USB_MSC::stop() {
    if (!active) return;
    msc.end();
    active = false;
    Serial.println("USB MSC: stopped");
}

#else  // Non-S3 / no native USB — stub so the build still links

bool USB_MSC::begin()    { Serial.println("USB MSC: unsupported on this target"); return false; }
bool USB_MSC::isActive() { return false; }
void USB_MSC::stop()     {}

#endif
