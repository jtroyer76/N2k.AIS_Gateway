# N2k.AIS_Gateway — Spec for Phases 1–3

Status: agreed draft, 2026-09-28. Conversion correctness (§5.4.1) added 2026-10-27 after a code review. Phase 3 (setup on the device, releases, web flasher; §7) added 2026-10-29: proposed, not yet agreed.

## 1. Purpose

A standalone, unattended AIS → NMEA 2000 gateway. It reads AIS sentences (`!AIVDM`) from an AIS receiver or transponder over RS485/RS422 NMEA 0183 and publishes them as N2k AIS PGNs. Tested with a Vesper WatchMate 850.

A common setup without it is AIS → USB → SignalK on a computer → N2k. The gateway keeps AIS on the bus whether or not that computer, SignalK or WiFi are up.

### Goals
- **Phase 1:** a robust bridge that can be installed and left alone. It recovers from CAN faults by itself and never blocks. It already includes **everything needed to update it once installed**: WiFi with a fallback hotspot, OTA updates from PlatformIO, and rollback protection. It is trialled on a boat, and the phase 1 acceptance tests are run there during the trial (§5.13).
- **Phase 2:** troubleshooting over the network: one status/console page. It is delivered to the installed unit by OTA.
- **Phase 3:** a general product. One release binary fits every boat: nothing boat-specific is compiled in. A new unit is installed from a web flasher and set up from a phone on its setup hotspot, with no PlatformIO needed (§7).

### Non-goals
- ~~A web settings UI or WiFiManager. Settings are fixed at compile time.~~ *Changed 2026-10-29: phase 3 adds settings on the device (§7).*
- SignalK integration (SignalK reads AIS from the N2k bus).
- A button feature for normal use. The unit is enclosed and the button can't be reached. *(Phase 3 uses it on the bench only, to reset settings; §7.4.)*
- N2k → 0183 conversion, a TCP data stream, and other inputs (see §9, Backlog).

## 2. Key decisions

| Topic | Decision | Reason |
|---|---|---|
| CAN driver | ~~Sergei Podshivalov's `NMEA2000_esp32_twai` (MIT), vendored into `lib/`~~ **Changed 2026-10-27:** own TWAI driver in a separate repo, [`NMEA2000_esp32_idf_twai`](https://github.com/jtroyer76/NMEA2000_esp32_idf_twai) (MIT), pinned by tag. See §5.3 review. | Uses Espressif's official TWAI driver, which includes ESP32 CAN errata fixes. Bus-off recovery is built in. The current `NMEA2000_esp32` ignores all CAN errors. Being our own repo, pinned by tag, protects the build from upstream changes. |
| Licence | Whole project stays **LGPL-2.1+**. The vendored library keeps its MIT licence file. | Simplest, since existing code from AK-Homberger is LGPL. GPL code (for example wellenvogel's `esp32-nmea2000`, GPL-2.0) is used as reference only, never copied. |
| Platform | `espressif32 @ 6.8.1` (Arduino core 2.0.17) | Newer bug-fix baseline. The ESP-IDF features used (TWAI, task watchdog, app rollback) are all available in it. |
| Settings | ~~Compile-time: `include/Config.h`, and `include/secrets.h` (gitignored)~~ **Changed 2026-10-29 (phase 3):** WiFi, device password and AIS baud are saved on the device (NVS). `secrets.h` is optional and for development boards only; release builds never contain it (§7.1). | One release binary for every boat. |
| Network | Joins the boat network. **Fallback hotspot only when the network is missing for 5 minutes.** Phase 3 adds a setup hotspot when no network is saved (§7.3). | USB can't be reached once installed, so OTA over WiFi (boat network or hotspot) is the only update path. |
| Framework | Arduino with a plain `loop()`; ESP-IDF APIs called directly where needed | No ReactESP or SensESP; the app is small. |

## 3. Hardware

| Part | Notes |
|---|---|
| M5Stack Atom Lite (ESP32-PICO-D4) | RGB LED on GPIO27. Button (GPIO39): bench settings reset only, from phase 3 (§7.4). |
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

The WatchMate 850's NMEA output is non-isolated RS422 (differential), 4800 or 38400 baud (Q1, resolved). **Gray** (out+) connects to Tail485 **A**, **Yellow** (out−) to **B**. Tie the Tail485 GND to the WatchMate's DC negative (black) for a common reference. Set the WatchMate's NMEA output to NMEA+AIS at 38400.

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
#define CAN_RX_QUEUE        32      // TWAI RX queue (SetN2kCANReceiveFrameBufSize)
#define N2K_SEND_FRAME_BUF  64      // Send frames in total: TWAI TX queue = this - 4, library keeps 4
#define STATS_LOG_MS        60000   // Periodic stats log line
```
*Implemented 2026-10-27.* `CAN_TX_QUEUE` is gone: with `NMEA2000_esp32_idf_twai` the TX queue size comes from `N2K_SEND_FRAME_BUF` (driver sizing, as `NMEA2000_esp32`). `Config.h` also holds the pins, the 5-minute timeouts (no AIS, hotspot, rollback) and the hostname.

### 5.3 CAN driver

**Review 2026-10-27 — replaces the vendored driver.** `NMEA2000_esp32_twai` at `75fd20b` has two paths that leave CAN stuck off, which the "unmodified" rule prevents fixing: a `CANOpen()` retry after a failed `twai_start()` fails forever at `twai_driver_install()`, and a failed `twai_start()` after bus-off recovery leaves the driver stopped with nothing watching. It also wakes an alert task on every frame, carries an unused UDP side interface and listeners, and ignores the library's buffer-size settings; Timo declined to support it (NMEA2000_esp32 #25). Timo's `NMEA2000_esp32` has an empty error handler and no errata workarounds. Decision: write a small driver following Timo's pattern (no tasks; recovery driven from polled TWAI state inside the library's calls), in its own repo so HALMET can use it too. Spec: `NMEA2000_esp32_idf_twai/docs/SPEC.md`; v0.1.0 host-tested 2026-10-27. Question to Timo: [ttlappalainen/NMEA2000_esp32#39](https://github.com/ttlappalainen/NMEA2000_esp32/issues/39). 

*Implemented 2026-10-27* (replaces the earlier bullets):
- The driver was vendored at `v0.1.0` in `lib/` while its repo was local-only. **2026-10-28:** published at [jtroyer76/NMEA2000_esp32_idf_twai](https://github.com/jtroyer76/NMEA2000_esp32_idf_twai) and pinned in `lib_deps` by tag (`#v0.1.0`); the vendored copy is removed. `NMEA2000_esp32` and the Sergei copy are removed. Timo answered #39 on 2026-10-28, including a silent "transmits but receives nothing" ESP32 state seen after days of running: see the driver spec, Q1 and Q4.
- Global object in `src/N2kNode.cpp`: `tNMEA2000_esp32_idf_twai NMEA2000(GPIO_NUM_22, GPIO_NUM_19);`. `SetN2kCANReceiveFrameBufSize(CAN_RX_QUEUE)` and `SetN2kCANSendFrameBufSize(N2K_SEND_FRAME_BUF)` are called before `Open()`.
- **Bus-off recovery and restart are in the driver,** inside `ParseMessages()`. There are no callbacks from other tasks, so rule 4 holds with no flags.
- `src/CanStatus.cpp` polls `GetStatus()` every 100 ms. It logs state changes and new bus-off, recovery, restart-failure, error-warning and error-passive events. The same status (state, TEC/REC, TX failed, RX missed/overrun, counts) feeds the stats line and, in phase 2, the status page.

### 5.4 Serial input
*Implemented 2026-10-27* in `src/AisInput.cpp`. It handles at most 10 sentences per loop. "Decode errors" are the decoder's decoding errors (which include checksum errors) plus sentences too long for the buffer. The buffer is `MAX_NMEA0183_MSG_BUF_LEN + 8`, and `GetMessage()` gets one byte less than that, which leaves room for the `"
"`.

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
  - "Today" comes from PGN 126992/129029, but only dates **not earlier than the firmware build date** (`__DATE__`) are accepted. This rejects old GPS units that report a date years in the past after a GPS week rollover (seen on the test boat: an old SeaTalk GPS reporting 2007 through a SeaTalk → N2k converter), and the 1970 default before any time message has arrived.
  - With no accepted date yet, or month/day not available, the ETA date is NA. Hour/minute not available → ETA time NA.
- **Type 24B mothership ID:** NA (or 0) unless the MMSI is an auxiliary craft (`98xxxxxxx`), in which case the dimension bits carry the mothership MMSI. Today the vessel's own MMSI is sent.
- **Type 19 (PGN 129040):** the hand-built message adds one byte past canboat's layout (`AddByte(0xff)` "Sequence ID"); drop it. The GNSS-type nibble (`_fixtype << 4`) and DTE/AIS-mode bits were checked against canboat and are correct.
- Prefer the library's setters where one exists for the PGN (e.g. 129802 `SetN2kAISSafetyRelatedBroadcastMsg`) over hand-built messages; keep hand-built 129038 only if the library setter can't carry a needed field, and note why in a comment.

### 5.5 LED (non-blocking)
*Implemented 2026-10-27:* decisions in `include/LedPattern.h` (host-tested), driving in `src/Led.cpp`.
- **Priority, highest first:** OTA, boot, CAN fault, AIS flash, blips, off.
- **Timing:**
  - blips are 100 ms
  - the "no AIS" blip is at the start of each 5 s cycle, and the hotspot blip is 2.5 s later, so both can show
  - during a CAN fault the LED only blinks red
- **Boot blue** is latched, so it doesn't come back when `millis()` wraps.
- **"No AIS"** is measured from boot until the first sentence arrives.

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
*Implemented 2026-10-27* in `src/main.cpp` / `src/Ota.cpp`.

- Call `enableLoopWDT()` at the end of `setup()` (5 s timeout).
- Anything that must block on purpose (OTA, §5.10) disables it first and re-enables it afterwards.
- At boot, log the reset reason (`esp_reset_reason()`).

### 5.7 N2k identity
*Implemented 2026-10-27* in `src/N2kNode.cpp`. The model serial code is the same 21-bit unique number (it was "1"). The product code stays 10.

- Keep the existing node address stored in NVS (namespace `nvs`, key `LastNodeAddress`).
- Product information: model ID `"AIS Gateway"`, software version from `FW_VERSION` (`include/Version.h`, starting at `1.1.0`), model version `"M5 Atom + CAN + Tail485"`.
- Device information: function 135 (NMEA 0183 gateway), class 25, manufacturer code 2046. Keep the unique number derived from the MAC, masked to 21 bits.
- **Announce the transmitted PGNs** with `ExtendTransmitMessages()`: 129038, 129039, 129040, 129794, 129802, 129809, 129810. This lets device lists and diagnostic tools show the gateway as an AIS source.

### 5.8 Logging
*Implemented 2026-10-27* in `src/Log.cpp` and `src/Stats.cpp`. Lines are `[uptime s] LEVEL message`. AIS decode errors are logged as WARN lines as well as counted.

- Add a `logf(level, fmt, ...)` function in `src/Log.cpp` with levels INFO, WARN and ERROR, and replace all `Serial.printf` calls with it.
- Phase 1 writes to `Serial` only. Phase 2 adds the in-memory buffer without changing any call sites.
- Every `STATS_LOG_MS`, log one stats line: sentences, AIS decoded, decode errors, N2k sent/failed, CAN state, bus-off count, free/minimum heap.

### 5.9 WiFi and fallback hotspot
*Phase 3 (§7) replaces the `secrets.h` values below with saved settings: the network from §7.2, and the device password for the hotspot. The fallback rules here stay.*

- **Normal operation:** station mode using `WIFI_SSID` / `WIFI_PASS` from `secrets.h`, with hostname `ais-gateway`. `WiFi.begin()` is non-blocking, and reconnection is automatic. Advertise over mDNS as `ais-gateway.local` (`_http._tcp`, `_arduino._tcp`). *Implemented 2026-10-27 (`src/Net.cpp`, decisions in `include/NetPolicy.h`, host-tested):* ArduinoOTA registers the mDNS name and `_arduino._tcp`. `_http._tcp` is added in phase 2, when there is a web server to advertise. If the boat network is still missing 60 s after the last `WiFi.begin()`, the begin is reissued, on top of the core's own auto-reconnect.
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
*Implemented 2026-10-27* in `src/Ota.cpp`. The LED turns white straight away in `onStart`, because the upload then blocks the loop.

- Password is `OTA_PASS` from `secrets.h`. *(Phase 3: the device password, §7.2.)*
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
*Implemented 2026-10-27* in `src/Ota.cpp`, with the decisions in `include/RollbackPolicy.h` (host-tested).
- **"Address claimed" is approximated** (`include/BusHealth.h`, host-tested). The library doesn't expose claim completion: `IsAddressClaimStarted()` is protected. So healthy = all of:
  - the library is open
  - the source address is not the null address (254)
  - TWAI is running
  - **both** error counters are below 96, the CAN error-warning limit (the controller is error-active)
  - **at least one N2k message has been received from another node since boot**
- **Why the last two:** the first version only checked TEC < 96. In the desk test (2026-10-27), a CAN Unit with nothing attached sat at TEC 0 / REC 135 (error passive, nothing transmitted). The image was wrongly marked valid at 60 s, so a build with dead CAN would have been accepted.
- **Check on hardware:** `CONFIG_APP_ROLLBACK_ENABLE` and `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` are set in the Arduino 2.0.17 sdkconfig. The broken-image acceptance test proves the bootloader actually rolls back.

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
src/main.cpp                      setup/loop wiring only
src/N2kNode.{h,cpp}               CAN driver object, N2k identity, address in NVS, time messages, counting send
src/CanStatus.{h,cpp}             polled driver status, state/event logging
src/AisInput.{h,cpp}              serial read + decode
src/Led.{h,cpp}                   LED state machine
src/Log.{h,cpp}                   logging
src/Stats.{h,cpp}                 counters + raw-sentence buffer
src/Net.{h,cpp}                   WiFi station, fallback hotspot
include/LedPattern.h, NetPolicy.h, RollbackPolicy.h, RawRing.h   platform-free decisions, host-tested
src/Ota.{h,cpp}                   ArduinoOTA + rollback validation
include/secrets.h                 WiFi/hotspot/OTA passwords (gitignored; optional dev defaults from phase 3, §7.1)
include/secrets.example.h         committed template
```

Phase 2 adds `src/WebUI.{h,cpp}` (web server, page, API) and the log ring buffer in `src/Log.cpp`.

### 5.13 Phase 1 acceptance
Testing happens on the boat during a trial period; there is no separate bench setup.

**Trial setup:**
- On the test boat, the AIS unit's existing USB → SignalK connection stays in place as a **standby fallback**, with SignalK's N2k AIS output **disabled**. If the gateway misbehaves, re-enable it in SignalK.
- Confirm there are no duplicate or ghost targets on the plotter.

**Tests:**
- [x] `pio run` builds with the pinned versions and no warnings in new code. *(2026-10-27, also checked with `-Wall -Wextra`; the only warning is in the existing `lib/AIS_Decoder/strutils.h`)*
- [ ] The WatchMate feeds the gateway, and AIS targets appear on the plotter **with names and vessel details** within 6 minutes.
- [ ] **Conversion test vectors (§5.4.1):** *(host side done 2026-10-27: `pio test -e native`, 24 tests, real sentences from gpsd and pyais samples; the on-bus `candump`/`analyzer` check below remains for the trial)* a set of real `!AIVDM` sentences covering types 1–3, 5, 14, 18, 19, 24A/B — including targets with SOG, position, rate of turn and ETA not available — taken from the WatchMate's raw-sentence buffer or a public AIS sample set (not hand-written, so checksums and multi-part framing are real). Each is sent into the gateway, the output captured from the bus (`candump` on the Pi, decoded with canboat's `analyzer`), and every field compared with the decoded AIS value. Not-available inputs must decode as "Unknown"/NA, never as a number.
- [ ] A device list or diagnostic tool shows the gateway transmitting the AIS PGNs.
- [ ] CAN recovery is **not** forced; there is no fault injection on the live bus. During the trial, the bus-off and recovery counters are monitored. If a bus-off happens naturally, the log shows whether the gateway recovered.
- [ ] Unplugging the RS485 input: the LED shows "no AIS" after 5 minutes. Reconnecting resumes bridging.
- [x] `pio run -e m5stack-atom-ota -t upload` updates the device over the boat network. *(2026-10-27 on the boat, via espota run directly with `-I` plus a Windows Firewall allow on TCP 3233, because of the boat PC's Tailscale adapter; the image was marked valid at 60 s on the live bus. See TESTING.md.)*
- [ ] Router off for 10 minutes: AIS bridging continues without interruption, and the hotspot appears after 5 minutes.
- [x] Through the hotspot: an OTA upload completes without the connection dropping. *(2026-10-27, desk test at home)*
- [ ] Router back on, laptop disconnected: the device rejoins the boat network and the hotspot shuts down.
- [x] Broken image test: an image where CAN never starts rolls back to the previous version. *(2026-10-27, desk test: an OTA image with no healthy bus rolled back after 300 s. The positive case, an image marked valid on a healthy bus, passed on the boat the same evening.)*
- [ ] A 24-hour soak test: no resets, and minimum free heap is stable.

---

## 6. Phase 2 — Status and console page

### 6.1 Log buffers
- `logf` also writes to an in-memory ring buffer of about 150 lines × 120 characters (about 18 KB). Each line gets a sequence number and an uptime timestamp.
- The last 20 raw AIS sentences are kept in their own ring buffer (filled from phase 1), so raw traffic never pushes log lines out.

### 6.2 Web page and API
Uses the core's `WebServer` (synchronous; every handler must be fast, and there's no upload endpoint). There's no authentication; the page is only reachable on the trusted boat network or through the password-protected hotspot. *(Phase 3 adds settings and a firmware upload, which need the device password; §7.5.)*

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

*Implemented 2026-10-28*, desk-tested the same day (results in TESTING.md):
- **Log ring:** `include/LogRing.h`, 150 lines × 200 bytes (about 32 KB rather than 18 KB). Lines are 200 bytes because the stats line can pass 160 characters; `logf`'s own buffer went from 160 to 200 to match. Lines are numbered from 1. Everything that logs runs in the main loop (ArduinoOTA's callbacks run inside `ArduinoOTA.handle()`), so there's no lock.
- **JSON:** `include/JsonOut.h` writes into one fixed 8 KB buffer, with no heap and no `String` copies (`send_P` sends straight from it). A write that doesn't fit is dropped and flagged, so a response is never cut mid-item.
- **`/api/log`** (`include/LogJson.h`): `{"lines":[[seq,ms,"I|W|E","text"],...],"next":S,"more":B,"lost":L,"reset":R}`. At most 40 lines per response (`LOG_JSON_MAX_LINES`). The page asks again at once while `more` is true. `lost` counts lines the ring dropped before the page read them, and `reset` means `since` was ahead of the device (it restarted). The page also spots a restart from uptime going backwards.
- **`/api/status`** also includes the running partition, N2k frames received from other nodes, and the CAN driver's restart-failure, TX-failed, RX-missed and overrun counts.
- Host tests: `test/test_web`, 16 tests. Build: RAM 28.8%, flash 70.8%. No warnings in new code with `-Wall -Wextra`.
- Code: `src/Web.cpp` (server and handlers), `src/WebPage.h` (the page), `src/SysInfo.cpp` (reset reason and partition, shared with the boot log).

### 6.3 Phase 2 acceptance
- [ ] Phase 2 firmware is delivered to the installed unit by OTA, and it validates (no rollback).
- [ ] The page shows live status, the console, and the last 20 sentences, over the boat network and through the hotspot.
- [ ] A 7-day soak test with the page left open part of the time: no resets and stable minimum heap.

---

## 7. Phase 3 — Setup on the device, releases, web flasher

*Proposed 2026-10-29, not yet agreed.* The same model as the Telltale display firmware, adapted to a device with no screen: the only ways in are USB (before install), WiFi, and the button (bench only).

### 7.1 No credentials in the firmware
*Implemented 2026-10-29* in `src/Settings.cpp` (NVS namespace `gateway`) and `include/SettingsPolicy.h` (host tests: `test/test_settings`). WiFi network and password are one setting: the network name decides whether both come from NVS or from `secrets.h`. An older `secrets.h` with `OTA_PASS` and no `DEVICE_PASS` seeds the device password from `OTA_PASS`, so OTA keeps its password. A saved or `secrets.h` device password outside 8-63 characters is replaced by a made-up one. `WIFI_SSID_OVERRIDE` test builds override the network in memory only. The compile-time `AIS_BAUD` is gone (default `AIS_BAUD_DEFAULT`).

- Nothing boat-specific is compiled in. WiFi, the device password and the AIS baud rate are saved in NVS (`src/Settings.{h,cpp}`, with the platform-free rules in `include/SettingsPolicy.h`, host-tested).
- `include/secrets.h` stays **optional, for development boards only**, so a dev board on the desk skips setup. If a setting has never been saved and `secrets.h` gives a value, that value is **saved to NVS at boot**, and the log says so. A unit running a `secrets.h` build therefore keeps its network and passwords when it later gets a release build by OTA. An installed unit can't be reached over USB, so it must never be left with nothing saved.
- Once a setting is saved, the saved value wins, even over `secrets.h`.
- CI and the release build fail if `include/secrets.h` exists (`test ! -e include/secrets.h`).

### 7.2 Settings
| Setting | Default | Notes |
|---|---|---|
| WiFi network and password | none | Case matters in the network name (seen on the test boat: a lower-case name never joined). |
| Device password | made up on first boot | 12 characters from `a-z 2-9`, without the look-alikes `0 O 1 l i` (as Telltale). Protects the fallback hotspot (WPA2), OTA (ArduinoOTA and the web upload) and saving settings from the boat network. |
| AIS baud | 38400 | 38400 or 4800. Replaces the compile-time `AIS_BAUD`. |

- **Seeing the device password:** it's logged on serial at every boot. Serial needs the device in hand on USB, which is as good as physical access. The web flasher's "Logs & Console" shows it straight after install. The setup page shows it on the setup hotspot (§7.3), and on the boat network after entering it. It's never in `/api/status` or the log ring.
- Changing it needs the current password, except on the setup and fallback hotspots, where joining already needed it or nothing is set up yet.

### 7.3 Setup hotspot (no network saved)
- **When:** at boot when no WiFi network is saved: straight away, not after 5 minutes.
- **Network:** `AIS-Gateway-XXXX` (last 2 MAC bytes, as the fallback hotspot), **open (no password)**, address `192.168.4.1`. Open only while nothing is set up: there is nothing to protect yet, and with no screen there's nowhere to show a password. The risk is someone in WiFi range configuring a new unit before its owner does. Tasmota and others make the same choice.
- **Captive portal:** every DNS name answers `192.168.4.1`, and unknown URLs redirect to the setup page, so a phone opens it by itself.
- Networks in range are scanned just before the hotspot comes up, for the setup page (scanning changes channel and would drop a joined phone).
- **Saving a network** restarts the gateway. From then on, the §5.9 rules apply: join the network, and fall back to the WPA2 hotspot after 5 minutes without it.
- LED: the purple hotspot blip, plus a distinct pattern for "not set up" (pattern to choose in `include/LedPattern.h`).
- AIS bridging runs normally while unconfigured: WiFi state never affects bridging.

*Implemented 2026-10-29* in `src/Net.cpp` (decisions in `include/NetPolicy.h`, host-tested). The scan is asynchronous: a hotspot (setup or fallback) waits for it, or for 15 s, then comes up; the list keeps each name once, at its strongest, up to 20, strongest first, without hidden networks. Captive DNS runs on both hotspots (Arduino `DNSServer`). LED: the setup hotspot blips purple twice, 300 ms apart (`LED_SETUP_GAP_MS`); the fallback hotspot once. Not run on hardware yet.

### 7.4 Bench reset (button)
- Hold the button for **10 s**: clear WiFi and the device password, then restart into the setup hotspot (a new password is made up). The N2k node address and the AIS baud are kept.
- For the bench only: the button can't be reached once the unit is enclosed (§3). Recovery for an installed unit with a lost password: open the enclosure, or USB.
- A short press does nothing, so a knock can't reset it.

*Implemented 2026-10-29* in `src/Button.cpp` (GPIO39, active low, polled every 20 ms; decision in `include/ButtonPolicy.h`, host-tested). Any release restarts the 10 s count. Active low as in M5Stack's own Atom library (`Button(39, true, 10)`, invert = pressed when low).

### 7.5 Setup page and web API
On the phase 2 web server. The status/console page gets a Settings link.

| Endpoint | Auth | Does |
|---|---|---|
| `GET /setup` | none | The setup page: network (with the scanned names to tap), WiFi password (blank keeps the saved one for the same network), AIS baud, device password (shown per §7.2, new one optional). On the setup hotspot `/` serves this page. |
| `GET /api/scan` | none | The networks found by the last scan. |
| `POST /api/settings` | device password, except on a hotspot | Saves and restarts. |
| `POST /ota` | device password | Firmware upload (the release's `-update.bin`) from the page, for owners without PlatformIO. Pauses the loop watchdog like ArduinoOTA (§5.10); the rollback rules (§5.11) apply. |

- The upload blocks the loop while it runs, so AIS bridging pauses, as it already does for ArduinoOTA.
- ArduinoOTA from PlatformIO stays, with the device password.
- `POST /api/restart` stays unauthenticated (phase 2 decision): it can't change anything.

*Implemented 2026-10-29* in `src/Web.cpp` and `src/SetupPage.h`; the save rules are `applySettingsForm()` in `include/SettingsPolicy.h` (host tests in `test/test_settings`). Two endpoints more than the table:
- `GET /api/setup`: version, hostname, `setup` (no network saved), `via_hotspot`, saved network name, AIS baud, and the device password only when `via_hotspot`.
- `POST /api/scan`: starts a scan from the boat network (the page's Scan button). Refused while a hotspot is on.

"Through a hotspot" means the hotspot is up and the request arrived on its address (`192.168.4.1`). A wrong device password gives 403, a bad value 400, both with `{"error": "..."}`. A network name that differs only in case is a different network, so the saved WiFi password isn't carried over. `/api/status` gains `wifi.setup`.

`POST /ota` (*implemented 2026-10-29*, `src/Web.cpp` + `src/Ota.cpp`): body = the app image, header `X-OTA-Password` = the device password (as Telltale), compared in constant time (`passwordMatches()`). The core's WebServer hands a non-form body to the handler in 1436-byte chunks, written with `Update`. A wrong password still has the whole body read and dropped, then 403. **The loop watchdog is paused for every upload, authorised or not:** the desk test found a wrong-password upload reset the gateway (task watchdog), because reading ~950 KB took longer than 5 s. So anyone on the boat network can pause bridging for as long as an upload lasts; the network is trusted (§6.2). The Settings page has a Firmware update card; on a hotspot the password is filled in.

### 7.6 Improv over serial: not in phase 3
WiFi is set up only through the setup hotspot (§7.3), which lists the networks in range and works without USB. Decided 2026-10-29: one setup path for now. Improv in the web flasher is in the backlog (§9).

### 7.7 Releases and web flasher
- **CI** (`.github/workflows/ci.yml`, every pull request): no `secrets.h`, `pio run -e m5stack-atom`, `pio test -e native`.
- **Release** (`.github/workflows/release.yml`, on a `v*` tag), modelled on Telltale's:
  - build without `secrets.h`, run the host tests
  - publish a GitHub release with `ais-gateway-<ver>-full.bin` (bootloader, partitions, OTA data and app merged, written at `0x0`) and `ais-gateway-<ver>-update.bin` (the app alone, for OTA)
  - deploy the web flasher (`site/`, ESP Web Tools, the same version Telltale uses) to GitHub Pages, with the firmware copied into the site
- `FW_VERSION` comes from the tag (`git describe`), not a hand-edited `Version.h`.
- The flasher works from Chrome or Edge over USB. Install **before** fitting the Tail485, while USB can be reached. Installing without erasing keeps the settings.
- The README links the flasher URL directly, and the repo's GitHub "Website" field points to it.

### 7.8 Generic docs
- README, SPEC and TESTING describe a general product. Boat-specific details (network names, addresses, computers, people) don't go in this repo; "the test boat" is fine.
- No history rewrite for what's already committed.

### 7.9 Phase 3 acceptance
- [ ] CI passes on a pull request; a build with `include/secrets.h` present fails the check.
- [ ] A release tag publishes both binaries and the flasher.
- [ ] Fresh board, full erase, install from the flasher: the device password shows in the flasher's console, and the open setup hotspot appears at once.
- [ ] From a phone: the setup page opens by itself, lists the networks in range, and saving joins the network.
- [ ] After setup, the fallback hotspot needs the device password.
- [ ] Saving settings from the boat network without the password is refused; with it, it's saved.
- [ ] A unit running a `secrets.h` build, given a release build by OTA, keeps its network and passwords (§7.1).
- [ ] Web upload of a `-update.bin` validates on a live bus, and an unhealthy one rolls back.
- [ ] Bench reset: 10 s hold brings back the setup hotspot; a short press does nothing.
- [ ] AIS bridging carries on through setup, scans and saves (except during a firmware upload).

---

## 8. Open questions
- **Q2.** Confirm the CAN Unit has no termination fitted, and whether its transceiver is isolated. *(Stage 1 on the backbone ran at TEC/REC 0, but termination wasn't checked directly.)*

### Resolved
- **Q1 (2026-10-27):** WatchMate 850 (WMX850). Per its manual, the NMEA output is non-isolated RS422 (differential), 4800 or 38400 baud. Gray = out+ (A), Yellow = out− (B). The Tail485 is the right interface, and `AIS_BAUD` 38400 stays. Wiring in §3.
- **CAN fault testing:** no forced bus-off test. Unplugging the drop cable also removes the gateway's power, so it proves nothing, and shorting CAN-H/L on the boat's live bus isn't acceptable. The bus-off and recovery counters are monitored during the trial instead.
- **Retry after a failed restart following recovery:** rely on the library's recovery code; no extra check.
- **Library wakes on every frame:** leave it; the library stays unmodified. No frames-per-second counter.
- **11-bit (non-N2k) frames:** leave them; they don't occur on this bus.

## 9. Backlog (not committed)
- A 0183 TCP stream on port 10110 for SignalK, OpenCPN or a display (non-blocking; drop data when a client's buffer is full).
- Automatic baud detection (alternate 38400/4800 until sentences with valid checksums arrive).
- A plain TCP log port (`nc ais-gateway.local 23`).
- A hostname setting, for more than one gateway on one network.
- Improv serial, so the web flasher asks for WiFi straight after installing. Open if added: an existing library (licence must suit LGPL-2.1+) or a small own implementation, and whether it works on the Atom's USB-serial bridge.
- ~~A browser-based installer~~ and ~~WiFiManager provisioning~~: now phase 3 (§7).
