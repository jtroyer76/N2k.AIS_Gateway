#include "Button.h"

#include <Arduino.h>

#include "ButtonPolicy.h"
#include "Config.h"
#include "Led.h"
#include "Log.h"
#include "Settings.h"

static const uint32_t kPollMs = 20;

static ButtonPolicy policy;
static uint32_t lastPoll = 0;

void buttonBegin() { pinMode(BUTTON_PIN, INPUT); }  // input-only pin, pulled up on the board

void buttonLoop(uint32_t now) {
  if (now - lastPoll < kPollMs) return;
  lastPoll = now;
  if (!policy.step(now, digitalRead(BUTTON_PIN) == LOW)) return;

  logf(LogLevel::Warn, "Button: held %lu s, clearing settings and restarting into setup",
       (unsigned long)(RESET_HOLD_MS / 1000));
  settingsClear();
  ledShowNow(LedColor::Purple);
  Serial.flush();
  ESP.restart();
}
