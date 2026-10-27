// Status LED (SPEC 5.5). Non-blocking: ledUpdate() picks the colour from LedPattern and only
// touches the LED when it changes. At LED_BRIGHTNESS 0 the LED is never driven.
#pragma once

#include <stdint.h>
#include "LedPattern.h"

void ledBegin();
void ledUpdate(const LedInputs &in);  // in.booting is filled in here

// Immediate colour, for code that blocks the loop afterwards (OTA).
void ledShowNow(LedColor color);
