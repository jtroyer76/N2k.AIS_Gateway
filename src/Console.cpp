#include "Console.h"

#include <Arduino.h>

#include "ConsoleCommand.h"
#include "Led.h"
#include "Log.h"
#include "Settings.h"

static const int kMaxBytesPerLoop = 64;  // bounded work per loop (SPEC 4)

static ConsoleLineReader reader;

void resetSettingsAndRestart(const char *source) {
  logf(LogLevel::Warn, "%s: clearing settings and restarting into setup", source);
  settingsClear();
  ledShowNow(LedColor::Purple);
  Serial.flush();
  ESP.restart();
  for (;;) {}  // ESP.restart() doesn't return
}

static void help() {
  logf(LogLevel::Info, "Console commands: \"help\"; \"reset settings\" (forget WiFi and the device password, "
                       "restart into the setup hotspot)");
}

void consoleLoop() {
  for (int n = 0; n < kMaxBytesPerLoop && Serial.available() > 0; n++) {
    if (!reader.feed((char)Serial.read())) continue;
    if (reader.tooLong()) {
      logf(LogLevel::Warn, "Console: line too long, ignored");
      continue;
    }
    switch (parseConsoleCommand(reader.line())) {
      case ConsoleCommand::None: break;
      case ConsoleCommand::Help: help(); break;
      case ConsoleCommand::ResetSettings: resetSettingsAndRestart("Console: \"reset settings\""); break;
      case ConsoleCommand::Unknown:
        logf(LogLevel::Warn, "Console: unknown command \"%s\"", reader.line());
        help();
        break;
    }
  }
}
