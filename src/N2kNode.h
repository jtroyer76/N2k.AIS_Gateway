// NMEA 2000 node: CAN driver, identity, address in NVS, time messages, counting send
// (SPEC 5.3, 5.7).
#pragma once

#include <NMEA2000_esp32_idf_twai.h>

extern tNMEA2000_esp32_idf_twai NMEA2000;

void n2kBegin();
void n2kLoop();

// Sends through the node and counts success/failure in stats.
bool n2kSend(const tN2kMsg &msg);

// Bus healthy enough to validate a new image: library open, an address held, CAN running and
// our frames acknowledged (TX error counter below the CAN error-warning limit of 96).
bool n2kHealthy();
