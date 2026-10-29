// Bench reset (SPEC 7.4): hold the Atom's button for 10 s to forget WiFi and the device password
// and restart into the setup hotspot. The button can't be reached once the unit is enclosed.
#pragma once

#include <stdint.h>

void buttonBegin();
void buttonLoop(uint32_t now);
