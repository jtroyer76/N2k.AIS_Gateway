// Settings saved on the device (SPEC 7.1, 7.2): WiFi network, device password, AIS baud.
// Nothing boat-specific is compiled in; include/secrets.h is optional, for development boards.
#pragma once

#include <stdint.h>

struct GatewaySettings {
  char ssid[33];        // blank: no network saved
  char wifiPass[64];
  char devicePass[64];  // hotspot (WPA2), OTA, saving settings
  uint32_t aisBaud;
};

// Loads the settings, saving secrets.h values that were never saved, and makes up a device
// password on first boot. Call before anything that uses them.
void settingsBegin();

const GatewaySettings &settings();
bool settingsHaveNetwork();
