// AIS input (SPEC 5.4): NMEA 0183 from the Tail485, decoded and converted to N2k.
#pragma once

#include <stdint.h>

void aisBegin();
void aisPoll(uint32_t now);

// Today's date (days since 1970) from the N2k bus, for the type 5 ETA year.
void aisSetToday(uint16_t daysSince1970);
