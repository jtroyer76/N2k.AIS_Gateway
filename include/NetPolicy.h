// WiFi fallback-hotspot policy (SPEC 5.9). Platform-free so it runs in the host tests;
// src/Net.cpp applies the actions to the WiFi stack.
//
// - Start the hotspot once the boat network has been missing for HOTSPOT_AFTER_MS (after boot
//   or after losing it).
// - While a client is connected to the hotspot, don't search for the boat network: scanning
//   changes channel and would drop the client (e.g. mid-OTA).
// - Stop the hotspot as soon as the boat network is connected. No other timeout.
#pragma once

#include <stdint.h>
#include "Config.h"

struct NetActions {
  bool stationEnabled = false;  // try to (re)connect to the boat network
  bool startHotspot = false;    // edge: start it now
  bool stopHotspot = false;     // edge: stop it now
};

class NetPolicy {
public:
  // haveNetwork false (no network saved, SPEC 7.3): the setup hotspot comes on at once and the
  // station stays off.
  NetActions step(uint32_t now, bool stationConnected, int hotspotClients, bool haveNetwork = true) {
    NetActions a;
    if (!haveNetwork) {
      if (!hotspot) {
        hotspot = true;
        a.startHotspot = true;
      }
      return a;
    }
    if (!started) {
      started = true;
      lastConnected = now;
    }

    if (stationConnected) {
      lastConnected = now;
      if (hotspot) {
        hotspot = false;
        a.stopHotspot = true;
      }
    } else if (!hotspot && now - lastConnected >= HOTSPOT_AFTER_MS) {
      hotspot = true;
      a.startHotspot = true;
    }

    a.stationEnabled = !(hotspot && hotspotClients > 0);
    return a;
  }

  bool hotspotOn() const { return hotspot; }

private:
  bool started = false;
  bool hotspot = false;
  uint32_t lastConnected = 0;
};
