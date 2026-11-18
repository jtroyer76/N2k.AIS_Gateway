// USB serial console commands (SPEC 7.4). Typing needs the device in hand on USB, the same bar as
// the button and as the device password shown on serial at boot (SPEC 7.2).
//   help            list the commands
//   reset settings  forget WiFi and the device password, restart into the setup hotspot
#pragma once

#include <stdint.h>

void consoleLoop();  // reads what is waiting on Serial, a bounded amount per call

// Clear WiFi and the device password and restart into setup. Shared by the button and the console.
// `source` names the trigger in the log, e.g. "Button: held 10 s".
[[noreturn]] void resetSettingsAndRestart(const char *source);
