// Logging (SPEC 5.8, 6.1). Each line goes to Serial and to an in-memory ring for the web
// console. Call from the main loop only (everything that logs runs there, including the
// ArduinoOTA callbacks), so the ring needs no lock.
#pragma once

#include "Config.h"
#include "LogRing.h"

enum class LogLevel { Info, Warn, Error };

typedef LogRing<LOG_LINES, LOG_LINE_LEN> ConsoleLog;

void logf(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

const ConsoleLog &consoleLog();
