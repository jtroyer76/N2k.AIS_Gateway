// Copy to include/secrets.h (gitignored) and fill in.
#pragma once
#define WIFI_SSID  "boat-network"
#define WIFI_PASS  "boat-network-password"
#define AP_PASS    "change-me-hotspot"  // fallback hotspot (WPA2: at least 8 characters)
#define OTA_PASS   "change-me-ota"      // also export it as AIS_OTA_PASS for `pio run -e m5stack-atom-ota`
