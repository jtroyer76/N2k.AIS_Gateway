#include "Net.h"

#include <Arduino.h>
#include <WiFi.h>

#include "Config.h"
#include "Log.h"
#include "NetPolicy.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill it in"
#endif

// Test builds only: build with -DWIFI_SSID_OVERRIDE=\"name\" to aim at a network that isn't
// there, which brings up the fallback hotspot without touching secrets.h (TESTING.md).
#ifdef WIFI_SSID_OVERRIDE
#undef WIFI_SSID
#define WIFI_SSID WIFI_SSID_OVERRIDE
#endif

static const uint32_t kPollMs = 500;
static const uint32_t kStationRetryMs = 60000;  // re-issue WiFi.begin() if still not connected

static NetPolicy policy;
static char hotspotSsid[24];
static uint32_t lastPoll = 0;
static uint32_t lastBegin = 0;
static bool stationEnabled = true;
static bool wasConnected = false;

void netBegin() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(hotspotSsid, sizeof(hotspotSsid), "AIS-Gateway-%02X%02X", mac[4], mac[5]);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // non-blocking
  lastBegin = millis();
  logf(LogLevel::Info, "WiFi: joining \"%s\" as %s", WIFI_SSID, HOSTNAME);
}

void netLoop(uint32_t now) {
  if (now - lastPoll < kPollMs) return;
  lastPoll = now;

  bool connected = WiFi.status() == WL_CONNECTED;
  int clients = policy.hotspotOn() ? WiFi.softAPgetStationNum() : 0;
  NetActions a = policy.step(now, connected, clients);

  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) logf(LogLevel::Info, "WiFi: connected, IP %s, RSSI %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else logf(LogLevel::Warn, "WiFi: boat network lost");
  }

  if (a.startHotspot) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(hotspotSsid, AP_PASS);
    logf(LogLevel::Warn, "WiFi: no boat network for %lu min, hotspot \"%s\" on %s",
         HOTSPOT_AFTER_MS / 60000, hotspotSsid, WiFi.softAPIP().toString().c_str());
  }
  if (a.stopHotspot) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    logf(LogLevel::Info, "WiFi: boat network back, hotspot off");
  }

  // Searching for the boat network changes channel and would drop a hotspot client (e.g. mid-OTA).
  if (a.stationEnabled != stationEnabled) {
    stationEnabled = a.stationEnabled;
    if (stationEnabled) {
      WiFi.setAutoReconnect(true);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      lastBegin = now;
      logf(LogLevel::Info, "WiFi: hotspot clients gone, searching for the boat network again");
    } else {
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
      logf(LogLevel::Info, "WiFi: hotspot client connected, not searching for the boat network");
    }
  } else if (stationEnabled && !connected && now - lastBegin >= kStationRetryMs) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastBegin = now;
  }
}

bool netHotspotOn() { return policy.hotspotOn(); }
