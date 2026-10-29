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
// AIS (NMEA 0183 !AIVDM) -> NMEA 2000 gateway for M5Stack Atom Lite + CAN Unit + Tail485.
// Based on AK-Homberger's NMEA0183 AIS to N2k gateway (v0.7, 2022). See docs/SPEC.md.
// Supported AIS message types: 1-3, 5, 14, 18, 19, 24A, 24B.
//
// setup()/loop() wiring only. Nothing in the loop may block (SPEC 4).

#include <Arduino.h>

#include "AisInput.h"
#include "Button.h"
#include "CanStatus.h"
#include "Led.h"
#include "Log.h"
#include "N2kNode.h"
#include "Net.h"
#include "Ota.h"
#include "Settings.h"
#include "Stats.h"
#include "SysInfo.h"
#include "Version.h"
#include "Web.h"

void setup() {
  Serial.begin(115200);
  ledBegin();
  logf(LogLevel::Info, "AIS Gateway %s starting, reset reason: %s", FW_VERSION, resetReason());

  settingsBegin();  // WiFi, device password and AIS baud, used by the modules below

  n2kBegin();
  aisBegin();
  netBegin();
  otaBegin();
  webBegin();
  buttonBegin();

  enableLoopWDT();  // 5 s; OTA disables it while an upload blocks the loop
}

void loop() {
  const uint32_t now = millis();

  n2kLoop();
  aisPoll(now);
  canPoll(now);
  netLoop(now);
  otaLoop(now, n2kHealthy());
  webLoop(now);
  buttonLoop(now);

  LedInputs led = {};
  led.now = now;
  led.lastAisMs = stats.lastAisMs;
  led.canRunning = canRunning();
  led.hotspotOn = netHotspotOn();
  led.setupMode = netSetupMode();
  led.otaActive = otaActive();
  ledUpdate(led);

  statsLoop(now);
}
