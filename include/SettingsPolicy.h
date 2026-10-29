// Settings rules (SPEC 7.1, 7.2): where each value comes from at boot, what's valid, and the
// made-up device password. Platform-free so it runs in the host tests; src/Settings.cpp keeps
// the values in NVS.
#pragma once

#include <stdint.h>
#include <string.h>

#define DEVICE_PASS_LEN     12  // made-up device password
#define AIS_BAUD_DEFAULT    38400

// a-z and 2-9 without the look-alikes 0 O 1 l i (as Telltale).
static const char kPasswordChars[] = "abcdefghjkmnpqrstuvwxyz23456789";

// Writes DEVICE_PASS_LEN characters and a NUL to out. rnd() returns a random 32-bit number.
template <typename Rng>
inline void makePassword(char *out, Rng rnd) {
  const uint32_t n = sizeof(kPasswordChars) - 1;
  for (int i = 0; i < DEVICE_PASS_LEN; i++) out[i] = kPasswordChars[rnd() % n];
  out[DEVICE_PASS_LEN] = '\0';
}

inline bool validBaud(uint32_t baud) { return baud == 38400 || baud == 4800; }

// 802.11 network names are 1-32 bytes.
inline bool validSsid(const char *s) {
  size_t n = strlen(s);
  return n >= 1 && n <= 32;
}

// WPA2 passphrase is 8-63 characters; blank for an open network.
inline bool validWifiPass(const char *s) {
  size_t n = strlen(s);
  return n == 0 || (n >= 8 && n <= 63);
}

// The device password also protects the fallback hotspot (WPA2), so the same limits apply.
inline bool validDevicePass(const char *s) {
  size_t n = strlen(s);
  return n >= 8 && n <= 63;
}

// Where a setting comes from at boot (7.1). A saved value wins, even when blank. Otherwise a
// secrets.h value is used and saved, so a later release build keeps it. Otherwise the default.
enum class SettingSource { Saved, Secrets, Default };

inline SettingSource settingSource(bool saved, bool inSecrets) {
  if (saved) return SettingSource::Saved;
  if (inSecrets) return SettingSource::Secrets;
  return SettingSource::Default;
}
