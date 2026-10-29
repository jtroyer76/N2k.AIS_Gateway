// WiFi station + fallback hotspot (SPEC 5.9), and the setup hotspot when no network is saved
// (7.3). The decisions are in NetPolicy; this applies them. WiFi state never affects bridging:
// nothing here blocks.
#pragma once

#include <stdint.h>

void netBegin();
void netLoop(uint32_t now);
bool netHotspotOn();
bool netSetupMode();  // no network saved: the open setup hotspot

// Networks in range, strongest first, from the scan made just before a hotspot comes up, or
// one asked for with netStartScan().
struct NetFound {
  char ssid[33];
  int8_t rssi;
  bool open;
};
int netFoundCount();
const NetFound &netFound(int i);
bool netScanning();

// Starts a scan, unless one is running or a hotspot is on (a scan changes channel and would
// drop a joined phone). Returns false if it didn't start.
bool netStartScan();
