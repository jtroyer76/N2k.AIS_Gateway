# N2k.AIS_Gateway

AIS (NMEA 0183) → NMEA 2000 gateway for an M5Stack Atom Lite.

This is a public, general product: no boat names, hostnames, network names, addresses or other boat-specific details in docs, examples, test data or screenshots. "The test boat" is fine.

**Start here:** [docs/TESTING.md](docs/TESTING.md) (current state, test results, next steps), then [docs/SPEC.md](docs/SPEC.md).

## Rules
- **Never guess.** No part numbers, spec values, ratings, API behaviour or quantitative claims unless verified (datasheet, library source, docs, this repo). Say "I don't know" otherwise.
- No AI attribution in commits or PRs (no Co-Authored-By lines, no "Generated with Claude Code" footers).
- Changes in behaviour are test-first where the logic can run on the host: platform-free decisions go in `include/*.h` with tests in `test/`.
- Nothing in the main loop may block (SPEC §4).
- The CAN driver lives in its own repo ([NMEA2000_esp32_idf_twai](https://github.com/jtroyer76/NMEA2000_esp32_idf_twai)), pinned by tag in `platformio.ini`. Change it there, tag it, then bump the tag here.
- `include/secrets.h` is gitignored and optional (dev boards only; SPEC §7.1). Never commit it or print its contents. Release builds never contain it.
- Keep SPEC.md and TESTING.md up to date with what was actually done and verified.

## Commands
- `pio run`: build. `pio run -t upload`: USB flash. `pio device monitor`: serial log (115200).
- `pio run -e m5stack-atom-ota -t upload`: OTA (needs `AIS_OTA_PASS`). If it fails after authenticating, see TESTING.md "OTA from a PC with several network adapters".
- `pio test -e native`: host tests (needs a host gcc/g++). Run them before `pio run`: a changed `platformio.ini` makes the next run wipe `.pio/build`.
- Release: push a `v*` tag on `main`; `release.yml` builds, publishes the release and the web flasher. The version is the tag (`scripts/version.py`). Tagging is the owner's call.
- Changes go through PRs that pass CI (`ci.yml`); Conventional Commit titles.
