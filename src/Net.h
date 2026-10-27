// WiFi station + fallback hotspot (SPEC 5.9). The decisions are in NetPolicy; this applies them.
// WiFi state never affects bridging: nothing here blocks.
#pragma once

#include <stdint.h>

void netBegin();
void netLoop(uint32_t now);
bool netHotspotOn();
