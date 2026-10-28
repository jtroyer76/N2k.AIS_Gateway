# N2k.AIS_Gateway

AIS to NMEA 2000 gateway for M5 Atom Lite.

Reads AIS (`!AIVDM`) sentences from an RS485 input (Vesper WatchMate) and converts them to NMEA 2000 PGNs on the CAN bus. It runs unattended: it recovers from CAN bus-off by itself, falls back to its own WiFi hotspot when the boat network is missing, and is updated over WiFi with rollback protection. Full spec: [docs/SPEC.md](docs/SPEC.md).

## Hardware

- M5 Atom Lite (ESP32)
- M5 CAN Unit (GPIO22 TX, GPIO19 RX)
- TAIL 485 RS485 Module (GPIO26 RX, GPIO32 TX), powered 9–24 V from the N2k drop

Wiring and installation: SPEC §3. Once the Tail485 is fitted and the unit installed, USB can't be reached: all updates are OTA.

## Supported AIS Message Types

- Types 1, 2, 3: Class A Position Reports (PGN 129038)
- Type 5: Static and Voyage Data (PGN 129794)
- Type 14: Safety Related Message (PGN 129802)
- Type 18: Class B Position Report (PGN 129039)
- Type 19: Class B Extended Position Report (PGN 129040)
- Type 24A/B: Class B Static Data (PGN 129809, 129810)

## Setup

1. Copy `include/secrets.example.h` to `include/secrets.h` (gitignored) and fill in the boat WiFi, the hotspot password (at least 8 characters) and the OTA password.
2. On the boat router, create a DHCP reservation for `ais-gateway` so it keeps a fixed IP.

## Building and updating

Requires PlatformIO.

- `pio run` — build.
- `pio run -t upload` — first flash over USB (before the Tail485 is fitted).
- `pio run -e m5stack-atom-ota -t upload` — OTA over the boat network to `ais-gateway.local`. Export the OTA password as `AIS_OTA_PASS` first.
- Through the fallback hotspot (`AIS-Gateway-XXXX`, appears after 5 minutes without the boat network): join it, then `pio run -e m5stack-atom-ota -t upload --upload-port 192.168.4.1`.
- If espota authenticates but then reports "No response from device", the PC probably has several network adapters (VPN, VirtualBox). Build with `pio run -e m5stack-atom-ota`, then run espota yourself with this PC's address on that network (this worked through the hotspot on 2026-10-27):
  `python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py -i 192.168.4.1 -I 192.168.4.2 -p 3232 -P 3233 -a "$AIS_OTA_PASS" -f .pio/build/m5stack-atom-ota/firmware.bin -r`
- A new image must come up healthy within 5 minutes, or the gateway restarts into the previous image. Healthy means: CAN running and error-active, an N2k address held, and traffic received from other N2k devices. So **an OTA image always rolls back when there is no N2k bus** (on the desk, for example). Flash over USB there instead.
- Changed router or password: wait for the hotspot, join it, and OTA a build with the new `secrets.h`.

## Status LED

| LED | Meaning |
|---|---|
| Blue (boot) | Starting |
| Green flash | AIS sentence received |
| Red blink 1 Hz | CAN not running (bus-off, stopped, recovering) |
| Amber blip every 5 s | No AIS for more than 5 minutes |
| Purple blip every 5 s | Fallback hotspot on |
| White | OTA update in progress |
| Off | Healthy and idle |

Serial log (115200 baud, USB only): state changes, errors, and a stats line every minute.

## Web console

Open `http://ais-gateway.local/` on the boat network, or `http://192.168.4.1/` through the fallback hotspot. It shows status tiles (last AIS, N2k, CAN, WiFi, uptime, memory), the live log, the last 20 raw AIS sentences, and a restart button. There's no login: it's only reachable on the boat network or through the password-protected hotspot. JSON API: `/api/status`, `/api/log?since=N`, `POST /api/restart` (SPEC §6).

## Tests

`pio test -e native` runs host-side tests (needs a host `gcc`/`g++` on PATH):

- `test/test_conversion` — the AIS → N2k conversion, fed with real `!AIVDM` sentences; the output is read back with the NMEA2000 library's parsers.
- `test/test_logic` — LED pattern, WiFi fallback policy, rollback policy, raw-sentence buffer.
- `test/test_web` — console log ring, bounded JSON writer, `/api/log` response.

## Credits

- Original code by AK-Homberger
- AIS decoder based on https://github.com/aduvenhage/ais-decoder (MIT License)
- Uses NMEA2000 libraries by Timo Lappalainen
- CAN driver: [NMEA2000_esp32_idf_twai](https://github.com/jtroyer76/NMEA2000_esp32_idf_twai) (MIT), pinned by tag in `platformio.ini`

## License

LGPL v2.1 or later. See LICENSE file.
