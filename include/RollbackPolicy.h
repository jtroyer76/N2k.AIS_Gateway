// Rollback validation (SPEC 5.11). Platform-free so it runs in the host tests; src/Ota.cpp
// reads the partition state and carries out the action.
//
// A new OTA image (pending verify) is marked valid once it has been healthy (CAN running,
// N2k address held) with at least ROLLBACK_MIN_UPTIME_MS uptime. If that hasn't happened by
// ROLLBACK_TIMEOUT_MS, restart: the bootloader then rolls back to the previous image.
// AIS input is not required; the WatchMate may be off during an update.
#pragma once

#include <stdint.h>
#include "Config.h"

enum class RollbackAction { None, MarkValid, Restart };

class RollbackPolicy {
public:
  explicit RollbackPolicy(bool pendingVerify) : pending(pendingVerify) {}

  RollbackAction step(uint32_t uptimeMs, bool healthy) {
    if (!pending) return RollbackAction::None;
    if (healthy && uptimeMs >= ROLLBACK_MIN_UPTIME_MS) {
      pending = false;
      return RollbackAction::MarkValid;
    }
    if (uptimeMs >= ROLLBACK_TIMEOUT_MS) {
      pending = false;
      return RollbackAction::Restart;
    }
    return RollbackAction::None;
  }

  bool isPending() const { return pending; }

private:
  bool pending;
};
