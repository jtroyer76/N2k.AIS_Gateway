#include "Button.h"

#include <Arduino.h>

#include "ButtonPolicy.h"
#include "Config.h"
#include "Console.h"

#ifdef BUTTON_PIN
static const uint32_t kPollMs = 20;

static ButtonPolicy policy;
static uint32_t lastPoll = 0;

void buttonBegin() { pinMode(BUTTON_PIN, INPUT); }  // input-only pin, pulled up on the board

void buttonLoop(uint32_t now) {
  if (now - lastPoll < kPollMs) return;
  lastPoll = now;
  if (!policy.step(now, digitalRead(BUTTON_PIN) == LOW)) return;

  char source[32];
  snprintf(source, sizeof(source), "Button: held %lu s", (unsigned long)(RESET_HOLD_MS / 1000));
  resetSettingsAndRestart(source);
}
#else
// Board without a usable button (Config.h): nothing to poll.
void buttonBegin() {}
void buttonLoop(uint32_t) {}
#endif
