# N2k.AIS_Gateway

AIS to NMEA 2000 gateway for M5 Atom Lite.

Reads AIS (AIVDM) messages from an RS485 input and converts them to NMEA 2000 PGNs on the CAN bus.

## Hardware

- M5 Atom Lite (ESP32)
- M5 CAN Unit (GPIO22 TX, GPIO19 RX)
- TAIL 485 RS485 Module (GPIO26 RX, GPIO32 TX)

## Supported AIS Message Types

- Types 1, 2, 3: Class A Position Reports (PGN 129038)
- Type 5: Static and Voyage Data (PGN 129794)
- Type 14: Safety Related Message (PGN 129802)
- Type 18: Class B Position Report (PGN 129039)
- Type 19: Class B Extended Position Report (PGN 129040)
- Type 24A/B: Class B Static Data (PGN 129809, 129810)

## Building

Requires PlatformIO. Run `pio run` to build.

## Credits

- Original code by AK-Homberger
- AIS decoder based on https://github.com/aduvenhage/ais-decoder (MIT License)
- Uses NMEA2000 libraries by Timo Lappalainen

## License

LGPL v2.1 or later. See LICENSE file.
