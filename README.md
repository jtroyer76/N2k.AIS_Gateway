# N2k.AIS_Gateway

AIS to NMEA 2000 gateway for an M5Stack Atom Lite, or a Seeed XIAO ESP32-S3 on a CAN + RS485 carrier board.

Reads AIS (`!AIVDM`) sentences from an RS485 input (Vesper WatchMate) and converts them to NMEA 2000 PGNs on the CAN bus. It runs unattended: it recovers from CAN bus-off by itself, falls back to its own WiFi hotspot when the boat network is missing, and is updated over WiFi with rollback protection. Full spec: [docs/SPEC.md](docs/SPEC.md).

## Hardware

Two boards, one firmware source (the PlatformIO env picks the board; pins are in `include/Config.h`):

| | M5Stack Atom stack (env `m5stack-atom`) | XIAO carrier (env `xiao-carrier`) |
|---|---|---|
| MCU | M5 Atom Lite (ESP32) | Seeed XIAO ESP32-S3 |
| CAN | M5 CAN Unit, TX GPIO22, RX GPIO19 | on the carrier (TJA1051), TX GPIO7, RX GPIO8 |
| RS485/RS422 | Tail485, RX GPIO26, TX GPIO32 | on the carrier, isolated (CA-IS2092), RX GPIO2, TX GPIO1, DE GPIO4 held low |
| Power | Tail485, 9–24 V from the N2k drop | carrier, from the N2k drop |
| LED | RGB (colours below) | single-colour user LED: the same pattern, on/off |
| Settings reset | 10 s button hold, or the serial console | serial console (the XIAO's buttons are too small to use) |

The carrier board is open hardware: [n2k_atom_can485_hw](https://github.com/jtroyer76/n2k_atom_can485_hw) (KiCad, JLCPCB files, enclosure).

Wiring and installation: SPEC §3. On the Atom, once the Tail485 is fitted and the unit installed, USB can't be reached: all updates are OTA. The XIAO's USB-C stays reachable through its enclosure.

## Supported AIS Message Types

- Types 1, 2, 3: Class A Position Reports (PGN 129038)
- Type 5: Static and Voyage Data (PGN 129794)
- Type 14: Safety Related Message (PGN 129802)
- Type 18: Class B Position Report (PGN 129039)
- Type 19: Class B Extended Position Report (PGN 129040)
- Type 24A/B: Class B Static Data (PGN 129809, 129810)

## Installing

**Web flasher: https://jtroyer76.github.io/N2k.AIS_Gateway/**. It installs the latest release from Chrome or Edge over USB, with no account or tools needed, and picks the firmware for the chip it finds. On the Atom, install before fitting the Tail485, while the USB port can be reached. Releases are listed under [Releases](https://github.com/jtroyer76/N2k.AIS_Gateway/releases), each with, per board (`<board>` is `m5stack-atom` or `xiao-carrier`):

- `ais-gateway-<ver>-<board>-full.bin`: everything, for esptool at `0x0` on a new board (`esptool.py --chip esp32 write_flash 0x0 ais-gateway-<ver>-m5stack-atom-full.bin`, or `--chip esp32s3` with the `xiao-carrier` file)
- `ais-gateway-<ver>-<board>-update.bin`: the app alone, for updates over WiFi (Settings → Firmware update). The gateway refuses a file built for the other board's chip.

Releases up to v1.2.0 are for the Atom only, named `ais-gateway-<ver>-full.bin` and `-update.bin`.

A release is built by `.github/workflows/release.yml` when a `v*` tag is pushed; the firmware's version is the tag.

## Setup

WiFi, the device password and the AIS baud rate (38400 or 4800) are saved on the device, so no credentials are compiled in.

1. **Join the setup hotspot.** With no WiFi network saved, the gateway starts an open hotspot, `AIS-Gateway-XXXX`, straight away (the LED blips twice every 5 s, purple on the Atom). Join it with a phone: the setup page opens by itself, or go to `http://192.168.4.1/`.
2. **Choose the boat's network** from the list (names are case-sensitive), enter its password, and check the AIS baud. The page shows the **device password**, which protects OTA updates, these settings and the gateway's fallback hotspot. Note it, or set your own (8–63 characters).
3. **Save.** The gateway restarts and joins the network. Settings are then at `http://ais-gateway.local/setup` (saving there needs the device password).
4. On the boat router, create a DHCP reservation for `ais-gateway` so it keeps a fixed IP.

The device password is also printed on the USB serial log at every boot (`Device password: ...`). Forgot it? Connect USB, open the serial console (115200 baud; the web flasher's "Logs & Console" works) and type `reset settings`: WiFi and the device password are cleared, and the setup hotspot comes back with a new password. On the Atom, holding the button for 10 s does the same. `help` lists the console commands.

Development boards: optionally copy `include/secrets.example.h` to `include/secrets.h` (gitignored). At boot, any value there whose setting was never saved is saved on the device, so the board skips setup. Once saved, the device's value wins. Release builds never contain `secrets.h`.

## Building and updating

Requires PlatformIO.

- `pio run -e m5stack-atom` or `pio run -e xiao-carrier` — build.
- `pio run -e <env> -t upload` — first flash over USB (on the Atom, before the Tail485 is fitted). The XIAO needs no button presses: esptool resets it into its bootloader over USB.
- `pio run -e m5stack-atom-ota -t upload` (or `xiao-carrier-ota`) — OTA over the boat network to `ais-gateway.local`. Export the device password as `AIS_OTA_PASS` first.
- Through the fallback hotspot (`AIS-Gateway-XXXX`, appears after 5 minutes without the boat network): join it, then `pio run -e m5stack-atom-ota -t upload --upload-port 192.168.4.1`.
- If espota authenticates but then reports "No response from device", the PC probably has several network adapters (VPN, VirtualBox). Build with `pio run -e m5stack-atom-ota`, then run espota yourself with this PC's address on that network (this worked through the hotspot on 2026-10-27):
  `python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py -i 192.168.4.1 -I 192.168.4.2 -p 3232 -P 3233 -a "$AIS_OTA_PASS" -f .pio/build/m5stack-atom-ota/firmware.bin -r`
- **Without PlatformIO:** Settings (`/setup`) → Firmware update, choose a release's `-update.bin`. Or from a shell: `curl -H "X-OTA-Password: <device password>" --data-binary @ais-gateway-<ver>-update.bin http://ais-gateway.local/ota`. AIS bridging pauses during the upload; the same rollback rule applies.
- Changed router or WiFi password: wait 5 minutes for the fallback hotspot, join it with the device password, and open `http://192.168.4.1/setup`.
- A new image must come up healthy within 5 minutes, or the gateway restarts into the previous image. Healthy means: CAN running and error-active, an N2k address held, and traffic received from other N2k devices. So **an OTA image always rolls back when there is no N2k bus** (on the desk, for example). Flash over USB there instead.

## Status LED

| LED | Meaning |
|---|---|
| Blue (boot) | Starting |
| Green flash | AIS sentence received |
| Red blink 1 Hz | CAN not running (bus-off, stopped, recovering) |
| Amber blip every 5 s | No AIS for more than 5 minutes |
| Purple blip every 5 s | Fallback hotspot on |
| Two purple blips every 5 s | Not set up: open setup hotspot on |
| White | OTA update in progress |
| Off | Healthy and idle |

On the XIAO carrier the LED is single-colour: the same pattern lights it on/off, so it's read by timing (boot: on for 0.5 s; CAN fault: 1 Hz blink; no-AIS and hotspot blips half a period apart; the setup hotspot blips twice; OTA: on).

Serial log (115200 baud, USB only): state changes, errors, and a stats line every minute. The serial console also takes commands: `help`, `reset settings` (SPEC §7.4).

## Web console

Open `http://ais-gateway.local/` on the boat network, or `http://192.168.4.1/` through the fallback hotspot. It shows status tiles (last AIS, N2k, CAN, WiFi, uptime, memory), the live log, the last 20 raw AIS sentences, and a restart button. Viewing needs no login: it's only reachable on the boat network or through a hotspot. **Settings** (`/setup`) changes WiFi, the AIS baud and the device password; saving from the boat network needs the device password. JSON API: `/api/status`, `/api/log?since=N`, `POST /api/restart` (SPEC §6); `/api/setup`, `/api/scan`, `POST /api/settings`, `POST /ota` (SPEC §7.5).

## Tests

`pio test -e native` runs host-side tests (needs a host `gcc`/`g++` on PATH):

- `test/test_conversion` — the AIS → N2k conversion, fed with real `!AIVDM` sentences; the output is read back with the NMEA2000 library's parsers.
- `test/test_logic` — LED pattern, WiFi fallback policy, rollback policy, raw-sentence buffer, serial console commands, the firmware upload's chip check.
- `test/test_settings` — where each setting comes from at boot, valid values, the made-up device password.
- `test/test_web` — console log ring, bounded JSON writer, `/api/log` response.

## Credits

- Original code by AK-Homberger
- AIS decoder based on https://github.com/aduvenhage/ais-decoder (MIT License)
- Uses NMEA2000 libraries by Timo Lappalainen
- CAN driver: [NMEA2000_esp32_idf_twai](https://github.com/jtroyer76/NMEA2000_esp32_idf_twai) (MIT), pinned by tag in `platformio.ini`

## License

LGPL v2.1 or later. See LICENSE file.
