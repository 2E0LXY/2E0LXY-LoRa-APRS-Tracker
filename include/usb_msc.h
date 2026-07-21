#pragma once
#include <Arduino.h>

// USB Mass Storage mode — presents the T-Deck's MicroSD card to a host
// computer as a removable USB drive. Used so the aprsnet.uk Map Downloader
// (and any file manager) can write map tiles straight onto the SD card.
//
// While MSC is active the card is owned by the USB host, so the tracker
// pauses SD access. Toggle it from the menu; the device reboots on exit to
// cleanly remount the card for its own use.
namespace USB_MSC {
    bool begin();      // mount SD, start USB MSC. Returns false if no card.
    bool isActive();
    void stop();       // stop MSC (host should eject first)
}
