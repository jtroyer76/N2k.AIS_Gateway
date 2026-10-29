// Optional, for development boards only (SPEC 7.1): copy to include/secrets.h (gitignored).
// At boot, any value here whose setting was never saved on the device is saved, so the board
// skips setup. Once a setting is saved, the saved value wins. Release builds never contain it.
#pragma once
#define WIFI_SSID    "boat-network"
#define WIFI_PASS    "boat-network-password"
#define DEVICE_PASS  "change-me-device"  // hotspot (WPA2), OTA and settings: 8-63 characters.
                                         // Export it as AIS_OTA_PASS for `pio run -e m5stack-atom-ota`.
