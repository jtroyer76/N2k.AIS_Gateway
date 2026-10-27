#include "CanStatus.h"

#include "Log.h"
#include "N2kNode.h"

static const uint32_t kPollMs = 100;

static tNMEA2000_esp32_idf_twai::Status last = {};
static bool havePolled = false;
static uint32_t lastPoll = 0;

static const char *stateName(twai_state_t s) {
  switch (s) {
    case TWAI_STATE_STOPPED: return "stopped";
    case TWAI_STATE_RUNNING: return "running";
    case TWAI_STATE_BUS_OFF: return "bus-off";
    case TWAI_STATE_RECOVERING: return "recovering";
  }
  return "?";
}

void canPoll(uint32_t now) {
  if (havePolled && now - lastPoll < kPollMs) return;
  lastPoll = now;

  tNMEA2000_esp32_idf_twai::Status s = NMEA2000.GetStatus();
  if (!havePolled) {
    havePolled = true;
    last = s;
    logf(LogLevel::Info, "CAN: %s", stateName(s.state));
    return;
  }

  if (s.state != last.state)
    logf(s.state == TWAI_STATE_RUNNING ? LogLevel::Info : LogLevel::Warn, "CAN: %s -> %s (TEC %u, REC %u)",
         stateName(last.state), stateName(s.state), s.txErrorCounter, s.rxErrorCounter);
  if (s.busOffCount != last.busOffCount)
    logf(LogLevel::Error, "CAN: bus-off #%u, recovery started", s.busOffCount);
  if (s.recoveryCount != last.recoveryCount)
    logf(LogLevel::Info, "CAN: recovered (#%u)", s.recoveryCount);
  if (s.restartFailures != last.restartFailures)
    logf(LogLevel::Error, "CAN: restart failed (%u total)", s.restartFailures);
  if (s.errorWarningCount != last.errorWarningCount)
    logf(LogLevel::Warn, "CAN: error counter above warning limit (TEC %u, REC %u)", s.txErrorCounter, s.rxErrorCounter);
  if (s.errorPassiveCount != last.errorPassiveCount)
    logf(LogLevel::Warn, "CAN: error passive (TEC %u, REC %u)", s.txErrorCounter, s.rxErrorCounter);
  last = s;
}

bool canRunning() { return havePolled && last.state == TWAI_STATE_RUNNING; }

const char *canStateName() { return stateName(last.state); }
