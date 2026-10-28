# Testing log and next steps

Start here when picking up the gateway in a new session. Spec: [SPEC.md](SPEC.md). Repo rules: [../CLAUDE.md](../CLAUDE.md).

## State (2026-10-27)

- **Stage 1 (CAN only) PASSED on the boat, 2026-10-27 evening** — see "Boat test results" below. Stage 2 (AIS via Tail485) deferred: the WatchMate's NMEA serial run has to be dug out of a legacy PO wiring setup first.
- Phase 1 code complete (SPEC §5.1–5.12), on `main`.
- Host tests: `pio test -e native`, 66 tests (24 conversion, 26 logic, 16 web), all passing.
- Phase 2 web console: code done 2026-10-28, not yet run on hardware (see "After the boat test").
- The CAN driver is our own [NMEA2000_esp32_idf_twai](https://github.com/jtroyer76/NMEA2000_esp32_idf_twai) (published 2026-10-28), pinned at `#v0.1.0` in `lib_deps`. It has its own spec and 22 host tests. Driver changes go in that repo, get a new tag, and then the tag is bumped here.
- Timo answered [ttlappalainen/NMEA2000_esp32#39](https://github.com/ttlappalainen/NMEA2000_esp32/issues/39) on 2026-10-28. He has seen an ESP32 state where it still transmits but receives nothing, with no error flag, after 2–20 days of running. A short boat test can't rule it out. Open question Q4 in the driver spec (candidate fix: an RX-silence watchdog).

### What is on the device right now
After the boat test (2026-10-27), the Atom Lite (CAN Unit attached, Tail485 **not** fitted) runs the build from nootka-pc, updated by OTA and marked valid on the live bus:
- `WIFI_SSID` = `Nootka` (the boat network; case matters). On the boat it got 192.168.10.154.
- `AP_PASS` = `change-me-hotspot`, `OTA_PASS` = `change-me-ota`: still the template placeholders.

Away from the boat network it comes up in **hotspot mode after 5 minutes**: SSID `AIS-Gateway-E08C`, password `change-me-hotspot`, address `192.168.4.1`. At home that's the way in (or USB). Replace the placeholder passwords before the permanent install.

## Desk test at home, 2026-10-27 (no CAN bus, no AIS input)

| Test | Result |
|---|---|
| Boot log: reset reason, version, node address, running partition | ✅ |
| Hotspot after 5 min without the configured network | ✅ at 300.7 s |
| No station search while a hotspot client is connected | ✅ |
| OTA through the hotspot (the "changed router" recovery), then joins the new network | ✅ (needed espota with `-I`, see below) |
| OTA over the LAN to `ais-gateway.local` | ✅ |
| Unhealthy OTA image rolls back after 300 s (bootloader rollback really enabled) | ✅ |
| Free heap steady, about 220–227 KB; no crashes | ✅ |
| Hotspot shuts down when the configured network returns | ⏳ not tested (router off would cut the session); boat trial |
| LED colours | ⏳ not observed by Claude |

Bugs found and fixed during the desk test:
- **Rollback health check fooled by a floating bus.** With nothing on the CAN Unit, the controller sits at TEC 0 / REC 135 (error passive). The old TEC-only check marked the image valid. Healthy now also needs REC < 96 and traffic from another N2k node (`include/BusHealth.h`, SPEC §5.11).
- **Loop watchdog resumed twice** when ArduinoOTA reported two errors for one failed upload. Pause and resume are now idempotent.

### OTA from a PC with several network adapters
If espota authenticates but then says "No response from device", the device couldn't connect back to the PC. Build, then run espota with the PC's own address on that network (`-I`):

```
pio run -e m5stack-atom-ota
python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py -i <device IP or 192.168.4.1> -I <PC IP on that network> -p 3232 -P 3233 -a "<OTA_PASS>" -f .pio/build/m5stack-atom-ota/firmware.bin -r
```

## Next: boat test (planned for the evening of 2026-10-27)

### Boat computer setup
1. Install git and PlatformIO: VS Code with the PlatformIO extension, or PlatformIO Core.
2. `git clone https://github.com/jtroyer76/N2k.AIS_Gateway.git`, then `pio run` (the first build downloads the toolchain).
3. Create `include/secrets.h` from `include/secrets.example.h`:
   - **boat** WiFi SSID and password
   - **real** hotspot password (at least 8 characters) and OTA password
   - export the OTA password as `AIS_OTA_PASS` for the OTA environment
4. Plug in the Atom over USB-C and check a serial port appears (`pio device list`). If none does, the PC needs the Atom's USB-serial driver; see M5Stack's documentation.
5. Optional: host tests need a host `gcc`/`g++`. On Windows that's e.g. MSYS2 mingw64; on Linux, the distro's gcc.

### Before connecting anything
- **WatchMate NMEA output (SPEC Q1, resolved 2026-10-27):** RS422 differential. In the WatchMate settings, set the NMEA output to NMEA+AIS at **38400**, which matches `AIS_BAUD` in `include/Config.h`.
- **Trial setup (SPEC §5.13):** leave the WatchMate → SignalK USB link connected as a standby, and **turn off SignalK's N2k AIS output** so targets aren't sent twice.

### Stage 1: CAN only (Atom on USB from the boat computer, no Tail485)
Serial logs are available in this stage, so it's the most informative.

1. Flash the boat build over USB: `pio run -t upload`, then `pio device monitor` (115200 baud).
2. Connect the CAN Unit to a spare drop: CAN-H white, CAN-L blue. No termination resistor, because the backbone already has its terminators.
3. Expected in the log:
   - `CAN: running` with **no** "error passive" line
   - stats line: TEC/REC near 0 and `rx` counting up (traffic from other devices)
   - `WiFi: connected` to the boat network
4. SignalK: the N2k source list should show the gateway ("AIS Gateway", software 1.1.0).
5. **OTA over the boat WiFi:** `pio run -e m5stack-atom-ota -t upload`, or espota with `-I` as above. Expected:
   - after the restart: `OTA: running from app1` + `pending verification`
   - at 60 s: **`OTA: new image marked valid`**
   - this is the path we could not test at home
6. Watch for any `CAN: bus-off` / `recovered` lines. Don't force a bus-off on the live bus (SPEC §7).

### Stage 2: AIS (Tail485 fitted, powered from the drop)
USB can't be reached once the Tail485 is on, so from here it's the LED, the plotters and SignalK only.

1. Wiring (SPEC §3):
   - NET-S (red) → Tail485 9–24 V through a ~1 A inline fuse; NET-C (black) → Tail485 GND
   - CAN-H/CAN-L → CAN Unit; leave the shield unconnected at the device end
   - WatchMate **Gray** (out+) → Tail485 **A**, **Yellow** (out−) → **B**; Tail485 GND tied to the WatchMate's DC negative (black)
2. Expected:
   - blue at boot, then a **green flash per AIS sentence**
   - targets on both plotters **with names and vessel details** within about 6 minutes
   - no duplicate or ghost targets
3. Unplug the RS485 input: amber blip every 5 s after 5 minutes. Reconnect, and bridging resumes.
4. LED meanings are in the README table. Red blinking means CAN isn't running.

### Record results
Update this file (and tick SPEC §5.13 items that pass), then the status line in nootka-specs `refit/electronics/plan.md`.

## Boat test results — 2026-10-27 evening (Stage 1, CAN only)

Boat computer: **nootka-pc** (the boat's permanent Windows 11 PC, not a laptop), PlatformIO Core 6.2.0 (Python 3.12 via winget), repo cloned fresh. nootka-pc is on the tailnet, so firmware builds / OTA can be driven remotely — but it **sleeps**, so it may need waking first (and it has the Tailscale-adapter espota quirk noted below). Atom on USB (COM14, FTDI FT232 bridge), CAN Unit plugged into the Pi's Actisense drop (Pi temporarily off the bus for the test; no spare drop used). Internet: Starlink. Build `m5stack-atom`: RAM 16.5%, Flash 67.7%.

**All Stage 1 checks passed:**
- First run of the vendored `NMEA2000_esp32_idf_twai` driver on a live bus. With the bus connected: `CAN: running`, **no** error-passive line, stats `TEC 0, REC 0, bus-off 0`, `rx ~3850/min` (traffic from other nodes). (Disconnected/floating, it correctly sat at error-passive REC 135 — matches the earlier desk observation.)
- `WiFi: connected, IP 192.168.10.154, RSSI -51` — joined on the first attempt after the SSID fix below.
- Heap steady ~227 KB.
- **OTA over the boat WiFi, then `OTA: new image marked valid` at 60.0 s** — the rollback health check that could not be tested at home (it needs a live bus with REC < 96 and traffic from another node). Confirmed on the real bus.

**Two gotchas found and worked around (boat-computer specific):**
1. **SSID is case-sensitive.** `secrets.h` had `nootka`; the network is **`Nootka`**. The firmware logged `WiFi: joining "nootka"` and never associated (no `WiFi: connected` line, which Net.cpp emits on success). Fixing the case → connected in 0.5 s. 802.11 SSIDs are a raw byte string, so case matters.
2. **espota needs `-I` + a firewall allow on nootka-pc.** nootka-pc has a Tailscale adapter (100.99.127.90) alongside Wi-Fi (192.168.10.163). `pio run -e ...-ota -t upload` and plain espota authenticated then failed `No response from device`. The device's callback on TCP 3233 was also blocked by Windows Firewall (fresh Python install, no exception). Fix that worked:
   - Allow `python.exe` / inbound TCP 3233 through Windows Firewall (one-time).
   - `python <...>/espota.py -i 192.168.10.154 -I 192.168.10.163 -p 3232 -P 3233 -a "<OTA_PASS>" -f .pio/build/m5stack-atom-ota/firmware.bin -r`

**Notes for next time:**
- `pio device monitor` can't run non-interactively in this harness; read the port with a short pyserial script instead. **Do not toggle DTR/RTS during the OTA verify window** — a reset before the 60 s mark would roll the image back. Open with both lines held equal (no reset) to watch the verification.
- Use `pio run -e m5stack-atom ...` to skip the `native` host-test env (fails here: no host g++/gcc installed).
- `OTA_PASS` / `AP_PASS` are still the template placeholders — set real ones before the permanent install (SPEC / README).

## After the boat test
- Phase 2: status and console web page (SPEC §6). **Code done 2026-10-28; desk-tested the same day** (USB flash, home WiFi, no CAN bus or AIS input), tested from the PC with HTTP requests:
  - `/api/status`: all fields present and correct (floating bus shows CAN running at REC 135, AIS "never")
  - `/api/log`: boot lines numbered 1–10; `since=next` returns nothing new; `since` ahead of the device gives `reset: true`
  - `/` serves the page (6.9 KB); unknown paths give 404; `GET /api/restart` gives 404
  - `POST /api/restart`: back within 10 s, reset reason "software restart", the log reports `reset`
  - 60 s of polling at the page's rate (log every 1 s, status every 2 s): no failed requests; free heap 180–181 KB, compared with about 220–227 KB in phase 1; lowest ever 155 KB
  - the page in a browser and on a phone, both in dark mode: checked by Jess, looks good
  - not yet checked: light mode, `more` paging past 40 lines, the hotspot address, the 7-day soak, OTA validation on the boat

  To test the rest, open `http://ais-gateway.local/` (or `http://192.168.4.1/` through the hotspot). Check:
  - the tiles refresh and the console fills, including the boot lines
  - restart from the page: the console shows "gateway restarted" and carries on
  - the page works on a phone and in dark mode
  - free heap stays steady with the page open (expect about 40 KB less than phase 1: log ring plus response buffer)
  - on the boat: an OTA of the phase 2 build validates (SPEC §6.3)
- Before the permanent install: check driver spec Q5. A report says the TWAI errata fix `TX_INTR_LOST`, which Arduino 2.0.17 enables, can cause crashes (espressif/esp-idf#9697).
- Permanent install: sealed enclosure, Micro-C drop, conformal coating if damp; 24-hour soak test.
- On-bus conversion check with `candump` + canboat `analyzer` once the Pi has a native CAN interface (AtomS3 CAN gateway, arriving 2026-10-29).
- Publish the driver repo to GitHub once proven; switch `lib_deps` to a tag.
