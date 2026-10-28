// Writes JSON into a fixed buffer (SPEC 6.2). A write that doesn't fit is dropped whole and
// flags the output; mark()/rewind() undo a partly written item, and setReserve() keeps room for
// closing brackets. No heap; platform-free so it runs in the host tests.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class JsonOut {
public:
  JsonOut(char *buf, size_t cap) : buf(buf), cap(cap) {
    if (cap) buf[0] = 0;
  }

  void raw(const char *s) { put(s, strlen(s)); }

  // Quoted and escaped string.
  void str(const char *s) {
    put("\"", 1);
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;
      char esc[8];
      if (c == '"' || c == '\\') {
        esc[0] = '\\';
        esc[1] = (char)c;
        put(esc, 2);
      } else if (c == '\n') {
        put("\\n", 2);
      } else if (c == '\r') {
        put("\\r", 2);
      } else if (c < 0x20) {
        snprintf(esc, sizeof(esc), "\\u%04x", c);
        put(esc, 6);
      } else {
        put((const char *)&c, 1);
      }
    }
    put("\"", 1);
  }

  void num(uint32_t v) {
    char t[12];
    snprintf(t, sizeof(t), "%lu", (unsigned long)v);
    raw(t);
  }

  void num(int32_t v) {
    char t[12];
    snprintf(t, sizeof(t), "%ld", (long)v);
    raw(t);
  }

  void boolean(bool v) { raw(v ? "true" : "false"); }

  // Writes fail if they would leave fewer than n bytes free.
  void setReserve(size_t n) { reserve = n; }

  size_t mark() const { return len; }

  // Back to a mark(): drops what was written since and clears the overflow flag.
  void rewind(size_t m) {
    len = m;
    if (cap) buf[len] = 0;
    overflow = false;
  }

  bool ok() const { return !overflow; }
  size_t length() const { return len; }

private:
  void put(const char *s, size_t n) {
    if (overflow || len + n + reserve + 1 > cap) {
      overflow = true;
      return;
    }
    memcpy(buf + len, s, n);
    len += n;
    buf[len] = 0;
  }

  char *buf;
  size_t cap;
  size_t len = 0;
  size_t reserve = 0;
  bool overflow = false;
};
