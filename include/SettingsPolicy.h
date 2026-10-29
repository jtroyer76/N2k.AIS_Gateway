// Settings rules (SPEC 7.1, 7.2): where each value comes from at boot, what's valid, and the
// made-up device password. Platform-free so it runs in the host tests; src/Settings.cpp keeps
// the values in NVS.
#pragma once

#include <stdint.h>
#include <stdio.h>
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

struct GatewaySettings {
  char ssid[33];        // blank: no network saved
  char wifiPass[64];
  char devicePass[64];  // hotspot (WPA2), OTA, saving settings
  uint32_t aisBaud;
};

// Where a setting comes from at boot (7.1). A saved value wins, even when blank. Otherwise a
// secrets.h value is used and saved, so a later release build keeps it. Otherwise the default.
enum class SettingSource { Saved, Secrets, Default };

inline SettingSource settingSource(bool saved, bool inSecrets) {
  if (saved) return SettingSource::Saved;
  if (inSecrets) return SettingSource::Secrets;
  return SettingSource::Default;
}

// What the setup page sends (7.5). Blank wifiPass keeps the saved one for the same network;
// blank newPass keeps the device password.
struct SettingsForm {
  const char *ssid;
  const char *wifiPass;
  uint32_t aisBaud;
  const char *newPass;
  const char *password;  // current device password; not needed through a hotspot
};

// Checks a save from the setup page. Returns nullptr and fills out, or the reason it's refused.
// Through a hotspot no password is needed: joining the fallback hotspot already needed it, and
// the setup hotspot only runs while nothing is set up (7.2).
inline const char *applySettingsForm(const GatewaySettings &cur, const SettingsForm &f, bool viaHotspot,
                                     GatewaySettings &out) {
  if (!viaHotspot && strcmp(f.password, cur.devicePass) != 0) return "wrong device password";
  if (!validSsid(f.ssid)) return "network name must be 1-32 characters";
  const bool sameNetwork = strcmp(f.ssid, cur.ssid) == 0;
  const char *wifiPass = (f.wifiPass[0] == '\0' && sameNetwork) ? cur.wifiPass : f.wifiPass;
  if (!validWifiPass(wifiPass)) return "WiFi password must be blank (open network) or 8-63 characters";
  if (!validBaud(f.aisBaud)) return "AIS baud must be 38400 or 4800";
  const char *devicePass = f.newPass[0] ? f.newPass : cur.devicePass;
  if (!validDevicePass(devicePass)) return "device password must be 8-63 characters";

  snprintf(out.ssid, sizeof(out.ssid), "%s", f.ssid);
  snprintf(out.wifiPass, sizeof(out.wifiPass), "%s", wifiPass);
  snprintf(out.devicePass, sizeof(out.devicePass), "%s", devicePass);
  out.aisBaud = f.aisBaud;
  return nullptr;
}
