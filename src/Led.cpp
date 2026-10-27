#include "Led.h"

#include <Arduino.h>
#include <FastLED.h>

#include "Config.h"

static CRGB leds[1];
static LedColor shown = LedColor::Off;
static bool booting = true;
static uint32_t bootMs = 0;

static CRGB rgb(LedColor c) {
  switch (c) {
    case LedColor::Off: return CRGB::Black;
    case LedColor::Blue: return CRGB::Blue;
    case LedColor::Green: return CRGB::Green;
    case LedColor::Red: return CRGB::Red;
    case LedColor::Amber: return CRGB(255, 110, 0);
    case LedColor::Purple: return CRGB::Purple;
    case LedColor::White: return CRGB::White;
  }
  return CRGB::Black;
}

void ledShowNow(LedColor color) {
  if (LED_BRIGHTNESS == 0 || color == shown) return;
  shown = color;
  leds[0] = rgb(color);
  FastLED.show();
}

void ledBegin() {
  bootMs = millis();
  if (LED_BRIGHTNESS == 0) return;
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, 1);
  FastLED.setBrightness(LED_BRIGHTNESS);
  shown = LedColor::White;  // force the first show
  ledShowNow(LedColor::Blue);
}

void ledUpdate(const LedInputs &in) {
  if (booting && in.now - bootMs >= LED_BOOT_MS) booting = false;  // latched: no repeat on wraparound
  LedInputs i = in;
  i.booting = booting;
  ledShowNow(ledPattern(i));
}
