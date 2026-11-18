// Serial console commands (SPEC 7.4): typed on the USB serial port, so they need the device in hand,
// like the button. Platform-free so it runs in the host tests; src/Console.cpp reads Serial.
#pragma once

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define CONSOLE_LINE_MAX 32  // longer lines are dropped whole

enum class ConsoleCommand { None, Help, ResetSettings, Unknown };

// Collects characters into lines. A line ends at CR or LF; CRLF gives one line, not two.
class ConsoleLineReader {
public:
  // True when c completes a line; line() then holds it (without the line end).
  bool feed(char c) {
    if (c == '\r' || c == '\n') {
      if (len == 0 && !overflow) return false;  // empty line, or the LF of a CRLF
      buf[len] = '\0';
      complete = !overflow;
      len = 0;
      overflow = false;
      if (!complete) buf[0] = '\0';
      return true;
    }
    if (len < CONSOLE_LINE_MAX) buf[len++] = c;
    else overflow = true;
    return false;
  }

  // The last line, or "" if it was too long.
  const char *line() const { return buf; }
  bool tooLong() const { return !complete; }

private:
  char buf[CONSOLE_LINE_MAX + 1] = {};
  size_t len = 0;
  bool overflow = false;
  bool complete = true;
};

// Spaces around and between words don't matter, nor does case: "Reset  Settings " is a reset.
inline ConsoleCommand parseConsoleCommand(const char *line) {
  char words[CONSOLE_LINE_MAX + 1];
  size_t n = 0;
  bool space = false;
  for (const char *p = line; *p && n < CONSOLE_LINE_MAX; p++) {
    if (isspace((unsigned char)*p)) {
      space = n > 0;
      continue;
    }
    if (space) {
      words[n++] = ' ';
      space = false;
      if (n >= CONSOLE_LINE_MAX) break;
    }
    words[n++] = (char)tolower((unsigned char)*p);
  }
  words[n] = '\0';

  if (n == 0) return ConsoleCommand::None;
  if (strcmp(words, "help") == 0) return ConsoleCommand::Help;
  if (strcmp(words, "reset settings") == 0) return ConsoleCommand::ResetSettings;
  return ConsoleCommand::Unknown;
}
