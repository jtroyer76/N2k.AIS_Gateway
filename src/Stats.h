// Counters and the last raw AIS sentences (SPEC 5.4, 5.8, 6.1). Updated from the main loop only.
#pragma once

#include <stdint.h>
#include "RawRing.h"

struct Stats {
  uint32_t sentences = 0;     // NMEA 0183 sentences received (any type)
  uint32_t aisSentences = 0;  // !AIVDM (and !AIVDO when forwarded)
  uint32_t aisDecoded = 0;    // AIS messages decoded
  uint32_t decodeErrors = 0;  // AIS decode/parse errors (incl. checksum)
  uint32_t n2kSent = 0;
  uint32_t n2kFailed = 0;
  uint32_t n2kReceived = 0;   // N2k messages received from other nodes
  uint32_t lastAisMs = 0;     // boot time until the first sentence
  bool everAis = false;
  RawRing<20, 90> raw;        // last 20 raw sentences (phase 2 shows them)
};

extern Stats stats;

// Logs the periodic stats line every STATS_LOG_MS.
void statsLoop(uint32_t now);
