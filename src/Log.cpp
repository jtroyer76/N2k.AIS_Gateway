#include "Log.h"

#include <Arduino.h>
#include <stdarg.h>

static ConsoleLog ring;

void logf(LogLevel level, const char *fmt, ...) {
  static const char *const names[] = {"INFO", "WARN", "ERROR"};
  static const char letters[] = {'I', 'W', 'E'};
  char line[LOG_LINE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  uint32_t ms = millis();
  Serial.printf("[%10.3f] %-5s %s\n", ms / 1000.0, names[(int)level], line);
  ring.push(ms, letters[(int)level], line);
}

const ConsoleLog &consoleLog() { return ring; }
