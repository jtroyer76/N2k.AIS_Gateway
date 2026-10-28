// Status and console page (SPEC 6.2). Synchronous WebServer served from the main loop; every
// handler is short and writes into a fixed buffer.
#pragma once

#include <stdint.h>

void webBegin();
void webLoop(uint32_t now);
