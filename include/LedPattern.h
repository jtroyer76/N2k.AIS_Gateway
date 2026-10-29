// LED pattern (SPEC 5.5): which colour the LED shows now, from the gateway state.
// Platform-free so it runs in the host tests; src/Led.cpp drives the actual LED.
#pragma once

#include <stdint.h>
#include "Config.h"

#define LED_BOOT_MS        500   // Blue at boot
#define LED_AIS_FLASH_MS   50    // Green flash per AIS sentence
#define LED_BLIP_MS        100   // Length of the "no AIS" / "hotspot" blips
#define LED_BLIP_PERIOD_MS 5000  // Blips repeat every 5 s; hotspot blip is offset by half a period
#define LED_SETUP_GAP_MS   300   // Setup hotspot: a second purple blip this long after the first

enum class LedColor { Off, Blue, Green, Red, Amber, Purple, White };

struct LedInputs {
  uint32_t now;        // millis()
  uint32_t lastAisMs;  // last AIS sentence; boot time if none yet
  bool booting;        // latched by the LED module for the first LED_BOOT_MS
  bool canRunning;
  bool hotspotOn;
  bool setupMode;      // no network saved: the setup hotspot (SPEC 7.3)
  bool otaActive;
};

// Priority, highest first: OTA, boot, CAN fault, AIS flash, blips (no AIS, hotspot), off.
inline LedColor ledPattern(const LedInputs &in) {
  if (in.otaActive) return LedColor::White;
  if (in.booting) return LedColor::Blue;
  if (!in.canRunning) return (in.now % 1000) < 500 ? LedColor::Red : LedColor::Off;

  const uint32_t sinceAis = in.now - in.lastAisMs;
  if (sinceAis < LED_AIS_FLASH_MS) return LedColor::Green;

  const uint32_t phase = in.now % LED_BLIP_PERIOD_MS;
  if (sinceAis > NO_AIS_MS && phase < LED_BLIP_MS) return LedColor::Amber;
  const uint32_t half = LED_BLIP_PERIOD_MS / 2;
  if (in.hotspotOn && phase >= half && phase < half + LED_BLIP_MS) return LedColor::Purple;
  // The setup hotspot blips twice.
  const uint32_t second = half + LED_SETUP_GAP_MS;
  if (in.hotspotOn && in.setupMode && phase >= second && phase < second + LED_BLIP_MS) return LedColor::Purple;
  return LedColor::Off;
}
