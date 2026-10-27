# N2k.AIS_Gateway — Spec for Phase 1 & 2

Status: agreed draft, 2026-09-28. Conversion correctness (§5.4.1) added 2026-10-27 after a code review.

## 1. Purpose

A standalone, unattended AIS → NMEA 2000 gateway. It reads AIS sentences (`!AIVDM`) from a Vesper WatchMate over RS485/NMEA 0183 and publishes them as N2k AIS PGNs.

It replaces the current path (WatchMate USB → SignalK on a Pi → N2k). The gateway must keep AIS on the bus whether or not the Pi, SignalK or WiFi are up.

### Goals
- **Phase 1:** a robust bridge that can be installed and left alone. It recovers from CAN faults by itself and never blocks. It already includes **everything needed to update it once installed**: WiFi with a fallback hotspot, OTA updates from PlatformIO, and rollback protection. It is trialled on the boat, and the phase 1 acceptance tests are run there during the trial (§5.13).
- **Phase 2:** troubleshooting over the network: one status/console page. It is delivered to the installed unit by OTA.

### Non-goals
- A web settings UI or WiFiManager. Settings are fixed at compile time.
- SignalK integration (SignalK reads AIS from the N2k bus).
- A physical button feature. The unit is enclosed and the button can't be reached.
- N2k → 0183 conversion, a TCP data stream, and other inputs (see §8, Backlog).

## 2. Key decisions

| Topic | Decision | Reason |
|---|---|---|
| CAN driver | ~~Sergei Podshivalov's `NMEA2000_esp32_twai` (MIT), vendored into `lib/`~~ **Changed 2026-10-27:** own TWAI driver in a separate repo, `NMEA2000_esp32_idf_twai` (MIT, local for now), pinned by tag. See §5.3 review. | Uses Espressif's official TWAI driver, which includes ESP32 CAN errata fixes. Bus-off recovery is built in. The current `NMEA2000_esp32` ignores all CAN errors. Vendoring protects the build from the upstream repo changing or disappearing (it has been inactive since 2023). |
| Licence | Whole project stays **LGPL-2.1+**. The vendored library keeps its MIT licence file. | Simplest, since existing code from AK-Homberger is LGPL. GPL code (for example wellenvogel's `esp32-nmea2000`, GPL-2.0) is used as reference only, never copied. |
| Platform | `espressif32 @ 6.8.1` (Arduino core 2.0.17) | Newer bug-fix baseline. The ESP-IDF features used (TWAI, task watchdog, app rollback) are all available in it. |
| Settings | Compile-time: `include/Config.h`, and `include/secrets.h` (gitignored) | Single boat; the network rarely changes. |
| Network | Joins the boat network. **Fallback hotspot only when the network is missing for 5 minutes.** | USB can't be reached once installed, so OTA over WiFi (boat network or hotspot) is the only update path. |
| Framework | Arduino with a plain `loop()`; ESP-IDF APIs called directly where needed | No ReactESP or SensESP; the app is small. |

## 3. Hardware

| Part | Notes |
|---|---|
| M5Stack Atom Lite (ESP32-PICO-D4) | RGB LED on GPIO27. Button (GPIO39) not used. |
| M5 CAN Unit | TX GPIO22, RX GPIO19. **No 120 Ω termination fitted.** |
| M5 Tail485 | RX GPIO26, TX GPIO32. SP485EEN transceiver, AOZ1282CI buck regulator, 9–24 V input. |

**The USB-C port can't be reached once the Tail485 is fitted and the unit is installed.** All updates after installation are OTA.

### Power and wiring (powered from the bus)

| N2k drop wire | Connects to |
|---|---|
| NET-S (red) | Tail485 9–24 V, through a ~1 A inline fuse |
| NET-C (black) | Tail485 GND |
| CAN-H (white) | CAN Unit H |
| CAN-L (blue) | CAN Unit L |
| Shield (bare) | Not connected at the device end |

The WatchMate's NMEA OUT+ connects to Tail485 **A** and OUT− to **B**. If the output is single-ended, connect the signal to A and B to GND.

Install in a sealed enclosure with cable glands and a Micro-C drop cable, and conformal-coat the boards if the location is damp.

## 4. Design rules (apply to all code)

1. **The bridge never waits on anything.** No `delay()`, blocking I/O or blocking network calls in the main loop.
2. **Every interval is measured wraparound-safe:** `millis() - start >= interval`, never `start + interval < millis()`.
3. **Count and log everything; guess nothing.** Every fault path increments a counter and writes a log line.
4. **Callbacks from other tasks only set counters and flags.** For example, the CAN library's alert listener runs in its own task. The main loop does the logging and any other work.
5. **Builds are reproducible:** pin the platform and all libraries. Vendored code records its source commit.

---

## 5. Phase 1 — Robust bridge, updatable over WiFi

### 5.1 Build configuration
- `platform = espressif32 @ 6.8.1`.
- Pin library versions: NMEA2000 (tag or commit), NMEA0183 `1.10.1`, FastLED `3.6.0`.
  - *Done 2026-10-27:* NMEA2000 pinned to registry release `4.24.1` (4.25.1, published 2026-10-19, was too new to adopt).
- Remove the `NMEA2000_esp32` dependency.
  - *Deferred to §5.3:* the current `main.cpp` still gets its CAN object through `NMEA2000_CAN.h`, so `NMEA2000_esp32` stays, pinned to commit `15cc308`, until the driver switch.
  - *Open before §5.3:* the NMEA2000 maintainer declined to support this library ("does not use properly my library", [NMEA2000_esp32 #25](https://github.com/ttlappalainen/NMEA2000_esp32/issues/25), 2024-05-22) without saying why. Review how it subclasses `tNMEA2000` before switching.
- Vendor `NMEA2000_esp32_twai` into `lib/NMEA2000_esp32_twai/`, from upstream commit `75fd20b` (2023-04-25). It is used **unmodified**. Include its licence text and a `SOURCE.md` recording the URL and the commit, and stating that there are no local changes.
- Keep the default partition table (two OTA app slots), and state it explicitly with `board_build.partitions = default.csv`.

### 5.2 Compile-time configuration (`include/Config.h`)
```cpp
#define AIS_BAUD            38400   // WatchMate AIS output; 4800 for standard 0183
#define FORWARD_OWN_SHIP    false   // Also convert AIVDO
#define LED_BRIGHTNESS      20      // 0 = LED off
#define SERIAL_RX_BUFFER    2048
#define CAN_TX_QUEUE        20      // TWAI driver queues (library default is 5)
#define CAN_RX_QUEUE        32
#define N2K_SEND_FRAME_BUF  64      // NMEA2000 library frame buffer for fast-packet bursts
#define STATS_LOG_MS        60000   // Periodic stats log line
```

### 5.3 CAN driver

**Review 2026-10-27 — replaces the vendored driver.** `NMEA2000_esp32_twai` at `75fd20b` has two paths that leave CAN stuck off, which the "unmodified" rule prevents fixing: a `CANOpen()` retry after a failed `twai_start()` fails forever at `twai_driver_install()`, and a failed `twai_start()` after bus-off recovery leaves the driver stopped with nothing watching. It also wakes an alert task on every frame, carries an unused UDP side interface and listeners, and ignores the library's buffer-size settings; Timo declined to support it (NMEA2000_esp32 #25). Timo's `NMEA2000_esp32` has an empty error handler and no errata workarounds. Decision: write a small driver following Timo's pattern (no tasks; recovery driven from polled TWAI state inside the library's calls), in its own repo so HALMET can use it too. Spec: `NMEA2000_esp32_idf_twai/docs/SPEC.md`; v0.1.0 host-tested 2026-10-27. Question to Timo: [ttlappalainen/NMEA2000_esp32#39](https://github.com/ttlappalainen/NMEA2000_esp32/issues/39). The bullets below describe the superseded design and will be rewritten against that driver; the vendored `lib/NMEA2000_esp32_twai/` is removed when the new driver lands.

- Create the global object directly instead of through `NMEA2000_CAN.h`:
  `NMEA2000_esp32_twai NMEA2000(GPIO_NUM_22, GPIO_NUM_19, TWAI_MODE_NORMAL, CAN_TX_QUEUE, CAN_RX_QUEUE);`
- Call `SetN2kCANSendFrameBufSize(N2K_SEND_FRAME_BUF)` before `Open()`.
- **Bus-off recovery is provided by the library.** Its control task watches the TWAI alerts. On `BUS_OFF` it calls `twai_initiate_recovery()`; on `BUS_RECOVERED` it calls `twai_start()`.
- Register a `TwaiBusAlertListener`. Following rule 4, it only increments counters and sets flags for: error warning, error passive, bus-off, and recovered.
- The main loop logs CAN state changes from those flags. For the status page, it reads `twai_get_status_info()`: state, TX/RX error counters, TX failed, RX missed/overrun. It also keeps bus-off and recovery counts.

### 5.4 Serial input
- Call `Serial1.setRxBufferSize(SERIAL_RX_BUFFER)`, then `Serial1.begin(AIS_BAUD, SERIAL_8N1, 26, 32)`.
- Attach it to NMEA0183 with `SetMessageStream()` + `Open()`, **not** `Begin()`. `Begin()` calls `begin()` a second time.
- Accept `VDM`, plus `VDO` when `FORWARD_OWN_SHIP` is true.
- Counters: sentences received, AIS sentences decoded, decode/parse errors.
- Keep each sentence in the "last 20 raw sentences" buffer (§6.1). Phase 1 only fills it; phase 2 displays it.
- Copy the sentence into a buffer with room for the appended `"\n"`; the current `char buf[150]` + `strcat` can write one byte past the end on a 149-character message.

### 5.4.1 AIS → N2k conversion correctness

Code review (2026-10-27) of `include/NMEA0183AIStoNMEA2000.h` found AIS "not available" values transmitted as real data. Rules:

- **Every AIS "not available" value becomes the library's NA** (`N2kDoubleNA`, or the field's NA code), checked **before** any scaling. The NMEA2000 library writes values outside the field's range as "out of range" (`N2kInt16OR` etc.), not as NA, so scaling an NA first is a bug.

| Field | AIS "not available" | Messages | Current behaviour |
|---|---|---|---|
| SOG | 1023 | 1–3, 18, 19 | Sent as 102.3 kn |
| Longitude / latitude | 181° / 91° (raw 108600000 / 54600000) | 1–3, 18, 19 | Sent as coordinates (both fit the 4-byte field) |
| Rate of turn | −128 | 1–3 | NA multiplied by `degToRad / 60` → written as out-of-range |
| COG | 3600 | 1–3, 18, 19 | Already NA (`decodeCog`) |
| Heading | 511 | 1–3, 18, 19 | Already NA (`decodeHeading`) |
| ETA month / day | 0 | 5 | Garbage date |
| ETA hour / minute | 24 / 60 | 5 | Time > 24 h |
| Draught | 0 | 5 | Sent as 0.0 m |

- **Type 5 transceiver information:** pass the real value (channel A/B reception from the sentence's channel field, default `N2kaischannel_A_VDL_reception`), not `_ais_version` cast to `tN2kAISTransceiverInformation`. Today version 1/2/3 transponders are reported as channel B reception / channel A transmission / channel B transmission.
- **Type 5 ETA date:**
  - The year is the **next occurrence** of the ETA month/day relative to today (an ETA of 3 January seen on 28 December is next year).
  - "Today" comes from PGN 126992/129029, but only dates **not earlier than the firmware build date** (`__DATE__`) are accepted. This rejects the boat's old SeaTalk GPS, which reports 2007 (GPS week rollover) through the SeaTalk → N2k converter, and the 1970 default before any time message has arrived.
  - With no accepted date yet, or month/day not available, the ETA date is NA. Hour/minute not available → ETA time NA.
- **Type 24B mothership ID:** NA (or 0) unless the MMSI is an auxiliary craft (`98xxxxxxx`), in which case the dimension bits carry the mothership MMSI. Today the vessel's own MMSI is sent.
- **Type 19 (PGN 129040):** the hand-built message adds one byte past canboat's layout (`AddByte(0xff)` "Sequence ID"); drop it. The GNSS-type nibble (`_fixtype << 4`) and DTE/AIS-mode bits were checked against canboat and are correct.
- Prefer the library's setters where one exists for the PGN (e.g. 129802 `SetN2kAISSafetyRelatedBroadcastMsg`) over hand-built messages; keep hand-built 129038 only if the library setter can't carry a needed field, and note why in a comment.

### 5.5 LED (non-blocking)
The LED is driven by a state that the loop updates; nothing may call `delay()`. Only call `FastLED.show()` when the colour changes. At `LED_BRIGHTNESS 0`, the LED is never driven.

| State | LED | Phase |
|---|---|---|
| Boot | Blue, 500 ms (non-blocking) | 1 |
| AIS sentence received | Green flash, about 50 ms | 1 |
| CAN not running (bus-off, stopped, recovering) | Red blink, 1 Hz | 1 |
| No AIS for more than 5 minutes | Short amber blip every 5 s | 1 |
| Healthy and idle | Off | 1 |
| Fallback hotspot on | Purple blip every 5 s | 1 |
| OTA in progress | White | 1 |

### 5.6 Watchdog
- Call `enableLoopWDT()` at the end of `setup()` (5 s timeout).
- Anything that must block on purpose (OTA, §5.10) disables it first and re-enables it afterwards.
- At boot, log the reset reason (`esp_reset_reason()`).

### 5.7 N2k identity
- Keep the existing node address stored in NVS (namespace `nvs`, key `LastNodeAddress`).
- Product information: model ID `"AIS Gateway"`, software version from `FW_VERSION` (`include/Version.h`, starting at `1.1.0`), model version `"M5 Atom + CAN + Tail485"`.
- Device information: function 135 (NMEA 0183 gateway), class 25, manufacturer code 2046. Keep the unique number derived from the MAC, masked to 21 bits.
- **Announce the transmitted PGNs** with `ExtendTransmitMessages()`: 129038, 129039, 129040, 129794, 129802, 129809, 129810. This lets device lists and diagnostic tools show the gateway as an AIS source.

### 5.8 Logging
- Add a `logf(level, fmt, ...)` function in `src/Log.cpp` with levels INFO, WARN and ERROR, and replace all `Serial.printf` calls with it.
- Phase 1 writes to `Serial` only. Phase 2 adds the in-memory buffer without changing any call sites.
- Every `STATS_LOG_MS`, log one stats line: sentences, AIS decoded, decode errors, N2k sent/failed, CAN state, bus-off count, free/minimum heap.

### 5.9 WiFi and fallback hotspot
- **Normal operation:** station mode using `WIFI_SSID` / `WIFI_PASS` from `secrets.h`, with hostname `ais-gateway`. `WiFi.begin()` is non-blocking, and reconnection is automatic. Advertise over mDNS as `ais-gateway.local` (`_http._tcp`, `_arduino._tcp`).
- **Fallback:** if the boat network has not been connected for **5 minutes** (after boot or after losing it), start a WPA2 hotspot:
  - SSID `AIS-Gateway-XXXX` (last 2 MAC bytes), password `AP_PASS` from `secrets.h`, address `192.168.4.1`.
  - It serves OTA, and from phase 2 the status page, exactly as normal operation does.
- **While the hotspot is up:**
  - **While any client is connected to the hotspot, don't try to reach the boat network.** Searching for it changes channels and would drop the client, for example in the middle of an OTA upload.
  - With no hotspot clients connected, keep trying to reach the boat network.
  - As soon as the boat network is connected, **shut the hotspot down.**
- There is no timeout; the hotspot stays up until the boat network returns.
- Setup note (README): create a DHCP reservation on the boat router for a fixed IP.
- **WiFi state has no effect on bridging** (acceptance test).
- Recovering from a changed router: wait for the fallback hotspot, join it from a laptop, then OTA a build with the new `secrets.h`.

### 5.10 OTA (ArduinoOTA)
- Password is `OTA_PASS` from `secrets.h`.
- `onStart`: log it, disable the loop watchdog, and turn the LED white.
- `onEnd` / `onError`: log it and re-enable the watchdog.
- Add a PlatformIO environment:
  ```ini
  [env:m5stack-atom-ota]
  extends = env:m5stack-atom
  upload_protocol = espota
  upload_port = ais-gateway.local
  upload_flags = --auth=${sysenv.AIS_OTA_PASS}
  ```
  Through the hotspot: `pio run -e m5stack-atom-ota -t upload --upload-port 192.168.4.1`.

### 5.11 Rollback protection
USB can't be reached, so a broken image must never leave the device unreachable. The Arduino core has app rollback enabled (`CONFIG_APP_ROLLBACK_ENABLE=y`, and it provides the weak hook `verifyRollbackLater()`).

- Override `verifyRollbackLater()` to return `true`, so the core doesn't automatically mark a new image as valid.
- At boot, if the running partition is `ESP_OTA_IMG_PENDING_VERIFY`, the image counts as healthy once **CAN is running, an N2k address has been claimed, and uptime is at least 60 s**. At that point call `esp_ota_mark_app_valid_cancel_rollback()` and log it.
- AIS input is **not** required for validation. The WatchMate may be off when you update.
- If the image isn't healthy within 5 minutes, log it and `ESP.restart()`. The bootloader then rolls back to the previous image.

### 5.12 Code layout
```
include/Config.h                  compile-time settings
include/Version.h                 FW_VERSION
include/NMEA0183AIStoNMEA2000.h   existing converter (LGPL, AK-Homberger); sends go through a counting helper
lib/AIS_Decoder/                  existing AIS decoder (MIT, aduvenhage)
lib/NMEA2000_esp32_twai/          vendored CAN driver (MIT)
src/main.cpp                      setup/loop wiring only
src/CanStatus.{h,cpp}             alert listener, counters, state logging
src/AisInput.{h,cpp}              serial read + decode
src/Led.{h,cpp}                   LED state machine
src/Log.{h,cpp}                   logging
src/Stats.{h,cpp}                 counters + raw-sentence buffer
src/Net.{h,cpp}                   WiFi station, fallback hotspot, mDNS
src/Ota.{h,cpp}                   ArduinoOTA + rollback validation
include/secrets.h                 WiFi/hotspot/OTA passwords (gitignored)
include/secrets.example.h         committed template
```

Phase 2 adds `src/WebUI.{h,cpp}` (web server, page, API) and the log ring buffer in `src/Log.cpp`.

### 5.13 Phase 1 acceptance
Testing happens on the boat during a trial period; there is no separate bench setup.

**Trial setup:**
- The WatchMate's USB → SignalK connection stays in place as a **standby fallback**, with SignalK's N2k AIS output **disabled**. If the gateway misbehaves, re-enable it in SignalK.
- Confirm there are no duplicate or ghost targets on the plotter.

**Tests:**
- [ ] `pio run` builds with the pinned versions and no warnings in new code.
- [ ] The WatchMate feeds the gateway, and AIS targets appear on the plotter **with names and vessel details** within 6 minutes.
- [ ] **Conversion test vectors (§5.4.1):** *(host side done 2026-10-27: `pio test -e native`, 24 tests, real sentences from gpsd and pyais samples; the on-bus `candump`/`analyzer` check below remains for the trial)* a set of real `!AIVDM` sentences covering types 1–3, 5, 14, 18, 19, 24A/B — including targets with SOG, position, rate of turn and ETA not available — taken from the WatchMate's raw-sentence buffer or a public AIS sample set (not hand-written, so checksums and multi-part framing are real). Each is sent into the gateway, the output captured from the bus (`candump` on the Pi, decoded with canboat's `analyzer`), and every field compared with the decoded AIS value. Not-available inputs must decode as "Unknown"/NA, never as a number.
- [ ] A device list or diagnostic tool shows the gateway transmitting the AIS PGNs.
- [ ] CAN recovery is **not** forced; there is no fault injection on the live bus. During the trial, the bus-off and recovery counters are monitored. If a bus-off happens naturally, the log shows whether the gateway recovered.
- [ ] Unplugging the RS485 input: the LED shows "no AIS" after 5 minutes. Reconnecting resumes bridging.
- [ ] `pio run -e m5stack-atom-ota -t upload` updates the device over the boat network.
- [ ] Router off for 10 minutes: AIS bridging continues without interruption, and the hotspot appears after 5 minutes.
- [ ] Through the hotspot: an OTA upload completes without the connection dropping.
- [ ] Router back on, laptop disconnected: the device rejoins the boat network and the hotspot shuts down.
- [ ] Broken image test: an image where CAN never starts rolls back to the previous version.
- [ ] A 24-hour soak test: no resets, and minimum free heap is stable.

---

## 6. Phase 2 — Status and console page

### 6.1 Log buffers
- `logf` also writes to an in-memory ring buffer of about 150 lines × 120 characters (about 18 KB). Each line gets a sequence number and an uptime timestamp.
- The last 20 raw AIS sentences are kept in their own ring buffer (filled from phase 1), so raw traffic never pushes log lines out.

### 6.2 Web page and API
Uses the core's `WebServer` (synchronous; every handler must be fast, and there's no upload endpoint). There's no authentication; the page is only reachable on the trusted boat network or through the password-protected hotspot.

| Endpoint | Returns |
|---|---|
| `GET /` | Single embedded HTML page |
| `GET /api/status` | JSON: version, uptime, reset reason, free/min heap, WiFi (mode, SSID, RSSI, IP), AIS (baud, sentences, decoded, errors, seconds since last), N2k (address, sent, failed), CAN (state, error counters, bus-off/recovery counts), last 20 raw sentences |
| `GET /api/log?since=N` | JSON: log lines with sequence number > N, plus the latest sequence number |
| `POST /api/restart` | Restarts after sending the response |

**The page:**
- **Status tiles** that refresh every 2 s. "Last AIS" is green under 60 s, amber under 5 min, red otherwise, or "never".
- **Console:** appends new log lines every 1 s, with pause/resume and auto-scroll.
- **"Last 20 AIS sentences" panel,** collapsible.
- **Restart button,** with a confirmation step.
- Works on a phone and supports dark mode. Everything is inline, with no external resources, since the boat may have no internet.

### 6.3 Phase 2 acceptance
- [ ] Phase 2 firmware is delivered to the installed unit by OTA, and it validates (no rollback).
- [ ] The page shows live status, the console, and the last 20 sentences, over the boat network and through the hotspot.
- [ ] A 7-day soak test with the page left open part of the time: no resets and stable minimum heap.

---

## 7. Open questions
- **Q1.** Confirm the WatchMate model and its 0183 output: baud rate, and whether it is differential or single-ended.
- **Q2.** Confirm the CAN Unit has no termination fitted, and whether its transceiver is isolated.

### Resolved
- **CAN fault testing:** no forced bus-off test. Unplugging the drop cable also removes the gateway's power, so it proves nothing, and shorting CAN-H/L on the boat's live bus isn't acceptable. The bus-off and recovery counters are monitored during the trial instead.
- **Retry after a failed restart following recovery:** rely on the library's recovery code; no extra check.
- **Library wakes on every frame:** leave it; the library stays unmodified. No frames-per-second counter.
- **11-bit (non-N2k) frames:** leave them; they don't occur on this bus.

## 8. Backlog (Phase 3, not committed)
- A 0183 TCP stream on port 10110 for SignalK, OpenCPN or the 7" display (non-blocking; drop data when a client's buffer is full).
- Automatic baud detection (alternate 38400/4800 until sentences with valid checksums arrive).
- A plain TCP log port (`nc ais-gateway.local 23`).
- A browser-based installer (ESP Web Tools and GitHub Actions) if the project is open-sourced.
- WiFiManager provisioning, if units are ever shared or sold.
