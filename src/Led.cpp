#include "Led.h"

#include <Arduino.h>

#include "Config.h"

#ifndef LED_MONO
#include <FastLED.h>
#endif

static LedColor shown = LedColor::Off;
static bool booting = true;
static uint32_t bootMs = 0;

#ifdef LED_MONO
// Single-colour LED, active low: the same pattern, shown as on/off.
static void drive(LedColor c) { digitalWrite(LED_PIN, c == LedColor::Off ? HIGH : LOW); }

static void setupLed() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
}
#else
static CRGB leds[1];

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

static void drive(LedColor c) {
  leds[0] = rgb(c);
  FastLED.show();
}

static void setupLed() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, 1);
  FastLED.setBrightness(LED_BRIGHTNESS);
}
#endif

void ledShowNow(LedColor color) {
  if (LED_BRIGHTNESS == 0 || color == shown) return;
  shown = color;
  drive(color);
}

void ledBegin() {
  bootMs = millis();
  if (LED_BRIGHTNESS == 0) return;
  setupLed();
  shown = LedColor::White;  // force the first show
  ledShowNow(LedColor::Blue);
}

void ledUpdate(const LedInputs &in) {
  if (booting && in.now - bootMs >= LED_BOOT_MS) booting = false;  // latched: no repeat on wraparound
  LedInputs i = in;
  i.booting = booting;
  ledShowNow(ledPattern(i));
}
