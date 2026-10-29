#include "Settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>

#include "Log.h"
#include "SettingsPolicy.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Older secrets.h files have OTA_PASS and no DEVICE_PASS: keep the OTA password working.
#if !defined(DEVICE_PASS) && defined(OTA_PASS)
#define DEVICE_PASS OTA_PASS
#endif

#ifdef WIFI_SSID
static const char *const kSecretSsid = WIFI_SSID;
#else
static const char *const kSecretSsid = nullptr;
#endif
#ifdef WIFI_PASS
static const char *const kSecretWifiPass = WIFI_PASS;
#else
static const char *const kSecretWifiPass = "";
#endif
#ifdef DEVICE_PASS
static const char *const kSecretDevicePass = DEVICE_PASS;
#else
static const char *const kSecretDevicePass = nullptr;
#endif

static GatewaySettings current;
static Preferences prefs;  // NVS namespace "gateway"

static void loadNetwork() {
  switch (settingSource(prefs.isKey("ssid"), kSecretSsid != nullptr)) {
    case SettingSource::Saved:
      prefs.getString("ssid", current.ssid, sizeof(current.ssid));
      prefs.getString("wifi_pass", current.wifiPass, sizeof(current.wifiPass));
      break;
    case SettingSource::Secrets:
      snprintf(current.ssid, sizeof(current.ssid), "%s", kSecretSsid);
      snprintf(current.wifiPass, sizeof(current.wifiPass), "%s", kSecretWifiPass);
      prefs.putString("ssid", current.ssid);
      prefs.putString("wifi_pass", current.wifiPass);
      logf(LogLevel::Info, "Settings: WiFi network saved from secrets.h");
      break;
    case SettingSource::Default:
      current.ssid[0] = current.wifiPass[0] = '\0';
      break;
  }

// Test builds only: build with -DWIFI_SSID_OVERRIDE=\"name\" to aim at a network that isn't
// there, which brings up the fallback hotspot without touching the saved one (TESTING.md).
#ifdef WIFI_SSID_OVERRIDE
  snprintf(current.ssid, sizeof(current.ssid), "%s", WIFI_SSID_OVERRIDE);
  logf(LogLevel::Warn, "Settings: test build, WiFi network overridden (not saved)");
#endif
}

static void loadDevicePass() {
  switch (settingSource(prefs.isKey("device_pass"), kSecretDevicePass != nullptr)) {
    case SettingSource::Saved:
      prefs.getString("device_pass", current.devicePass, sizeof(current.devicePass));
      if (validDevicePass(current.devicePass)) return;
      logf(LogLevel::Error, "Settings: saved device password not valid, making up a new one");
      break;
    case SettingSource::Secrets:
      snprintf(current.devicePass, sizeof(current.devicePass), "%s", kSecretDevicePass);
      if (validDevicePass(current.devicePass)) {
        prefs.putString("device_pass", current.devicePass);
        logf(LogLevel::Info, "Settings: device password saved from secrets.h");
        return;
      }
      logf(LogLevel::Warn, "Settings: secrets.h device password is not 8-63 characters, making up one");
      break;
    case SettingSource::Default:
      break;
  }
  makePassword(current.devicePass, []() { return esp_random(); });
  prefs.putString("device_pass", current.devicePass);
  logf(LogLevel::Info, "Settings: device password made up (shown on serial at boot)");
}

static void loadBaud() {
  current.aisBaud = prefs.getUInt("ais_baud", AIS_BAUD_DEFAULT);
  if (!validBaud(current.aisBaud)) {
    logf(LogLevel::Error, "Settings: saved AIS baud %lu not valid, using %d", (unsigned long)current.aisBaud,
         AIS_BAUD_DEFAULT);
    current.aisBaud = AIS_BAUD_DEFAULT;
  }
}

void settingsBegin() {
  prefs.begin("gateway", false);
  loadNetwork();
  loadDevicePass();
  loadBaud();
  prefs.end();

  // Serial only, never the web console's log: reading serial needs the device in hand (7.2).
  Serial.printf("Device password: %s\n", current.devicePass);
  if (settingsHaveNetwork()) logf(LogLevel::Info, "Settings: WiFi network \"%s\", AIS %lu baud", current.ssid,
                                  (unsigned long)current.aisBaud);
  else logf(LogLevel::Info, "Settings: no WiFi network saved, AIS %lu baud", (unsigned long)current.aisBaud);
}

const GatewaySettings &settings() { return current; }

bool settingsHaveNetwork() { return current.ssid[0] != '\0'; }

void settingsSave(const GatewaySettings &s) {
  prefs.begin("gateway", false);
  prefs.putString("ssid", s.ssid);
  prefs.putString("wifi_pass", s.wifiPass);
  prefs.putString("device_pass", s.devicePass);
  prefs.putUInt("ais_baud", s.aisBaud);
  prefs.end();
  logf(LogLevel::Info, "Settings: saved (WiFi network \"%s\", AIS %lu baud)", s.ssid, (unsigned long)s.aisBaud);
}

void settingsClear() {
  prefs.begin("gateway", false);
  prefs.remove("ssid");
  prefs.remove("wifi_pass");
  prefs.remove("device_pass");
  prefs.end();
  logf(LogLevel::Warn, "Settings: WiFi and device password cleared");
}
