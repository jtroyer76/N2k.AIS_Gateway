// CAN state logging (SPEC 5.3). Polls the driver's status from the main loop and logs state
// changes and new bus-off / recovery / error events. No callbacks from other tasks: the driver
// has none.
#pragma once

#include <stdint.h>

void canPoll(uint32_t now);
bool canRunning();
const char *canStateName();
