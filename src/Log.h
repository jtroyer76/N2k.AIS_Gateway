// Logging (SPEC 5.8). Phase 1 writes to Serial; phase 2 adds the in-memory ring buffer here
// without changing call sites.
#pragma once

enum class LogLevel { Info, Warn, Error };

void logf(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
