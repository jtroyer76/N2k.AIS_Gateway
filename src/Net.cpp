#include "Net.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>

#include "Config.h"
#include "Log.h"
#include "NetPolicy.h"
#include "Settings.h"

static const uint32_t kPollMs = 500;
static const uint32_t kStationRetryMs = 60000;  // re-issue WiFi.begin() if still not connected
static const uint32_t kScanTimeoutMs = 15000;   // give up waiting for a scan, start the hotspot anyway
static const int kMaxFound = 20;

static NetPolicy policy;
static DNSServer dns;  // captive portal: every name answers the hotspot's address
static char hotspotSsid[24];
static uint32_t lastPoll = 0;
static uint32_t lastBegin = 0;
static bool stationEnabled = true;
static bool wasConnected = false;

static NetFound found[kMaxFound];
static int foundCount = 0;
static bool scanning = false;
static uint32_t scanStarted = 0;
static bool hotspotPending = false;  // waiting for the scan, then start the hotspot

static void joinNetwork() {
  if (settingsHaveNetwork()) WiFi.begin(settings().ssid, settings().wifiPass);  // non-blocking
}

// Keeps each name once, at its strongest, sorted strongest first. Hidden networks are skipped.
static void collectScan(int n) {
  foundCount = 0;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0 || ssid.length() > 32) continue;
    int8_t rssi = (int8_t)WiFi.RSSI(i);
    bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    int k = 0;
    while (k < foundCount && strcmp(found[k].ssid, ssid.c_str()) != 0) k++;
    if (k < foundCount) {
      if (rssi > found[k].rssi) found[k].rssi = rssi;
      continue;
    }
    if (foundCount == kMaxFound) continue;
    snprintf(found[foundCount].ssid, sizeof(found[foundCount].ssid), "%s", ssid.c_str());
    found[foundCount].rssi = rssi;
    found[foundCount].open = open;
    foundCount++;
  }
  for (int i = 1; i < foundCount; i++) {
    NetFound f = found[i];
    int j = i - 1;
    while (j >= 0 && found[j].rssi < f.rssi) {
      found[j + 1] = found[j];
      j--;
    }
    found[j + 1] = f;
  }
}

static bool startScan(uint32_t now) {
  if (scanning) return false;
  if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {  // async: poll with scanComplete()
    logf(LogLevel::Warn, "WiFi: scan failed to start");
    return false;
  }
  scanning = true;
  scanStarted = now;
  return true;
}

static void startHotspot() {
  WiFi.mode(WIFI_AP_STA);
  if (netSetupMode()) {
    WiFi.softAP(hotspotSsid);  // open: nothing is set up yet (7.3)
    logf(LogLevel::Warn, "WiFi: no network saved, open setup hotspot \"%s\" on %s", hotspotSsid,
         WiFi.softAPIP().toString().c_str());
  } else {
    WiFi.softAP(hotspotSsid, settings().devicePass);  // WPA2 with the device password
    logf(LogLevel::Warn, "WiFi: no boat network for %lu min, hotspot \"%s\" on %s", HOTSPOT_AFTER_MS / 60000,
         hotspotSsid, WiFi.softAPIP().toString().c_str());
  }
  dns.start(53, "*", WiFi.softAPIP());
}

static void pollScan(uint32_t now) {
  if (!scanning) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING && now - scanStarted < kScanTimeoutMs) return;
  scanning = false;
  if (n >= 0) {
    collectScan(n);
    logf(LogLevel::Info, "WiFi: scan found %d network(s)", foundCount);
  } else {
    logf(LogLevel::Warn, "WiFi: scan failed (%d)", n);
  }
  WiFi.scanDelete();
  if (hotspotPending) {
    hotspotPending = false;
    startHotspot();
  }
}

void netBegin() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(hotspotSsid, sizeof(hotspotSsid), "AIS-Gateway-%02X%02X", mac[4], mac[5]);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  stationEnabled = settingsHaveNetwork();
  WiFi.setAutoReconnect(stationEnabled);
  joinNetwork();
  lastBegin = millis();
  if (settingsHaveNetwork()) logf(LogLevel::Info, "WiFi: joining \"%s\" as %s", settings().ssid, HOSTNAME);
  else logf(LogLevel::Warn, "WiFi: no network saved");
}

void netLoop(uint32_t now) {
  if (policy.hotspotOn() && !hotspotPending) dns.processNextRequest();  // non-blocking
  if (now - lastPoll < kPollMs) return;
  lastPoll = now;

  pollScan(now);

  bool connected = WiFi.status() == WL_CONNECTED;
  int clients = policy.hotspotOn() && !hotspotPending ? WiFi.softAPgetStationNum() : 0;
  NetActions a = policy.step(now, connected, clients, settingsHaveNetwork());

  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) logf(LogLevel::Info, "WiFi: connected, IP %s, RSSI %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else logf(LogLevel::Warn, "WiFi: boat network lost");
  }

  // Scan first, for the setup page's list: once a phone has joined, a scan would drop it.
  if (a.startHotspot) {
    if (startScan(now) || scanning) hotspotPending = true;
    else startHotspot();
  }
  if (a.stopHotspot) {
    hotspotPending = false;
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    logf(LogLevel::Info, "WiFi: boat network back, hotspot off");
  }

  // Searching for the boat network changes channel and would drop a hotspot client (e.g. mid-OTA).
  if (a.stationEnabled != stationEnabled) {
    stationEnabled = a.stationEnabled;
    if (stationEnabled) {
      WiFi.setAutoReconnect(true);
      joinNetwork();
      lastBegin = now;
      logf(LogLevel::Info, "WiFi: hotspot clients gone, searching for the boat network again");
    } else {
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
      logf(LogLevel::Info, "WiFi: hotspot client connected, not searching for the boat network");
    }
  } else if (stationEnabled && !connected && now - lastBegin >= kStationRetryMs) {
    joinNetwork();
    lastBegin = now;
  }
}

bool netHotspotOn() { return policy.hotspotOn() && !hotspotPending; }
bool netSetupMode() { return !settingsHaveNetwork(); }

int netFoundCount() { return foundCount; }
const NetFound &netFound(int i) { return found[i]; }
bool netScanning() { return scanning; }

bool netStartScan() {
  if (policy.hotspotOn()) return false;
  return startScan(millis());
}
