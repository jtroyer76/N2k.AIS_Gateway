/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
// Calendar helpers for the AIS type 5 ETA (days since 1970-01-01, UTC).
// Pure functions with no platform dependencies, so they run in the native tests.
#pragma once

#include <stdint.h>

namespace AisDate {

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's days_from_civil).
inline int32_t daysFromCivil(int32_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

// Inverse of daysFromCivil (Howard Hinnant's civil_from_days).
inline void civilFromDays(int32_t z, int32_t &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int32_t)yoe + era * 400 + (m <= 2);
}

inline int32_t yearOf(int32_t days) {
  int32_t y; unsigned m, d;
  civilFromDays(days, y, m, d);
  return y;
}

const uint16_t NA = 0xffff;  // same as N2kUInt16NA

inline bool isLeapYear(int32_t y) {
  return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

inline unsigned daysInMonth(int32_t y, unsigned m) {
  static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeapYear(y)) ? 29 : days[m - 1];
}

// Days since 1970 for a compiler __DATE__ string ("Mmm dd yyyy", day space-padded).
inline int32_t buildDateDays(const char *date) {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  unsigned m = 1;
  while (m <= 12 && !(months[(m - 1) * 3] == date[0] && months[(m - 1) * 3 + 1] == date[1] &&
                      months[(m - 1) * 3 + 2] == date[2]))
    m++;
  unsigned d = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
  int32_t y = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');
  return daysFromCivil(y, m, d);
}

// The first date on or after `today` that falls on month/day (an ETA of 3 January seen on
// 28 December is next year). NA if month/day is not a real date.
inline uint16_t nextOccurrence(uint16_t today, unsigned month, unsigned day) {
  if (month < 1 || month > 12 || day < 1 || day > 31) return NA;
  const int32_t year = yearOf(today);
  for (int32_t y = year; y <= year + 8; y++) {  // 29 February can be up to 8 years away
    if (day > daysInMonth(y, month)) continue;
    const int32_t days = daysFromCivil(y, month, day);
    if (days >= today) return days < NA ? (uint16_t)days : NA;
  }
  return NA;
}

}  // namespace AisDate
