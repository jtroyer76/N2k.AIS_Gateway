// Fixed-size ring of the last N text lines, each truncated to Len-1 characters (SPEC 5.4/6.1:
// last 20 raw AIS sentences). No heap; platform-free so it runs in the host tests.
#pragma once

#include <stddef.h>
#include <string.h>

template <size_t N, size_t Len>
class RawRing {
public:
  void push(const char *line) {
    strncpy(lines[next], line, Len - 1);
    lines[next][Len - 1] = 0;
    next = (next + 1) % N;
    if (count < N) count++;
  }

  size_t size() const { return count; }

  // i = 0 is the oldest kept line.
  const char *at(size_t i) const { return lines[(next + N - count + i) % N]; }

private:
  char lines[N][Len] = {};
  size_t next = 0;
  size_t count = 0;
};
