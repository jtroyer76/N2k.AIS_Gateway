// Ring buffer of the last N log lines for the web console (SPEC 6.1). Lines are numbered from 1
// so a client can ask for "everything after N". No heap; platform-free so it runs in the host
// tests.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

template <size_t N, size_t Len>
class LogRing {
public:
  struct Line {
    uint32_t seq;
    uint32_t ms;  // uptime when logged
    char level;   // 'I', 'W', 'E'
    char text[Len];
  };

  void push(uint32_t ms, char level, const char *text) {
    Line &l = lines[(last) % N];
    last++;
    l.seq = last;
    l.ms = ms;
    l.level = level;
    strncpy(l.text, text, Len - 1);
    l.text[Len - 1] = 0;
  }

  // Sequence number of the newest line; 0 if nothing has been logged.
  uint32_t latest() const { return last; }

  // Sequence number of the oldest line still kept (latest() + 1 when empty).
  uint32_t oldest() const { return last >= N ? last - N + 1 : 1; }

  // The line with this sequence number, or nullptr if it was never logged or has been dropped.
  const Line *find(uint32_t seq) const {
    if (seq == 0 || seq > last || seq < oldest()) return nullptr;
    return &lines[(seq - 1) % N];
  }

private:
  Line lines[N] = {};
  uint32_t last = 0;
};
