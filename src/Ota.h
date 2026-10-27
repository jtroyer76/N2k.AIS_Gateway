// OTA updates and rollback protection (SPEC 5.10, 5.11).
#pragma once

#include <stdint.h>

void otaBegin();
void otaLoop(uint32_t now, bool healthy);  // healthy: see n2kHealthy()
bool otaActive();
