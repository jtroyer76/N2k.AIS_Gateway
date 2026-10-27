// Is the N2k bus working for us? Used to validate a new OTA image (SPEC 5.11).
// Platform-free so it runs in the host tests; src/N2kNode.cpp fills in the inputs.
//
// The library doesn't expose address-claim completion, so this asks for evidence instead:
// open, holding an address, CAN running, controller error-active (both error counters below
// the CAN warning limit of 96), and at least one N2k message received from another node.
// Error counters alone are not enough: a CAN Unit with nothing attached sits at TEC 0 /
// REC 135 (seen on the desk 2026-10-27), and a TEC-only check passed it.
#pragma once

#include <stdint.h>

static const uint32_t kCanErrorWarningLimit = 96;
static const uint8_t kN2kNullAddress = 254;

struct BusHealthInputs {
  bool libraryOpen;
  uint8_t source;             // our N2k source address
  bool canRunning;
  uint32_t txErrorCounter;
  uint32_t rxErrorCounter;
  uint32_t framesFromOthers;  // N2k messages received from other nodes since boot
};

inline bool busHealthy(const BusHealthInputs &b) {
  return b.libraryOpen && b.source < kN2kNullAddress && b.canRunning &&
         b.txErrorCounter < kCanErrorWarningLimit && b.rxErrorCounter < kCanErrorWarningLimit &&
         b.framesFromOthers > 0;
}
