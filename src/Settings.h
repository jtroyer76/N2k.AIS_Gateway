// Settings saved on the device (SPEC 7.1, 7.2): WiFi network, device password, AIS baud.
// Nothing boat-specific is compiled in; include/secrets.h is optional, for development boards.
#pragma once

#include <stdint.h>
#include "SettingsPolicy.h"

// Loads the settings, saving secrets.h values that were never saved, and makes up a device
// password on first boot. Call before anything that uses them.
void settingsBegin();

const GatewaySettings &settings();
bool settingsHaveNetwork();

// Saves all settings (already checked with applySettingsForm). Takes effect after a restart.
void settingsSave(const GatewaySettings &s);

// Bench reset (7.4): forgets WiFi and the device password. The AIS baud and N2k address stay.
void settingsClear();
