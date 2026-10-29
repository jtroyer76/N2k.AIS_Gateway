// Bench reset button (SPEC 7.4): held for RESET_HOLD_MS fires once; a short press does nothing.
// Platform-free so it runs in the host tests; src/Button.cpp reads the pin.
#pragma once

#include <stdint.h>

#define RESET_HOLD_MS 10000

class ButtonPolicy {
public:
  // True once, when the button has been held down for RESET_HOLD_MS without a break.
  bool step(uint32_t now, bool pressed) {
    if (!pressed) {
      held = false;
      fired = false;
      return false;
    }
    if (!held) {
      held = true;
      since = now;
    }
    if (!fired && now - since >= RESET_HOLD_MS) {
      fired = true;
      return true;
    }
    return false;
  }

private:
  bool held = false;
  bool fired = false;
  uint32_t since = 0;
};
