#include "Log.h"

#include <Arduino.h>
#include <stdarg.h>

void logf(LogLevel level, const char *fmt, ...) {
  static const char *const names[] = {"INFO", "WARN", "ERROR"};
  char line[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);
  Serial.printf("[%10.3f] %-5s %s\n", millis() / 1000.0, names[(int)level], line);
}
