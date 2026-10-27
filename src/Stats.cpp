#include "Stats.h"

#include <Arduino.h>

#include "CanStatus.h"
#include "Config.h"
#include "Log.h"
#include "N2kNode.h"

Stats stats;

static uint32_t lastLog = 0;

void statsLoop(uint32_t now) {
  if (now - lastLog < STATS_LOG_MS) return;
  lastLog = now;
  tNMEA2000_esp32_idf_twai::Status can = NMEA2000.GetStatus();
  logf(LogLevel::Info,
       "stats: 0183 %u, AIS %u, decoded %u, errors %u | N2k sent %u, failed %u | CAN %s, bus-off %u | heap %u, min %u",
       stats.sentences, stats.aisSentences, stats.aisDecoded, stats.decodeErrors, stats.n2kSent, stats.n2kFailed,
       canStateName(), can.busOffCount, ESP.getFreeHeap(), ESP.getMinFreeHeap());
}
