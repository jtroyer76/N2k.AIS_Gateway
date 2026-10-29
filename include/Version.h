// Firmware version (SPEC 7.7). Firmware builds get it from git (`git describe --tags`, without the
// leading "v") through scripts/version.py; a tagged release is e.g. "1.2.0", a build after it
// "1.2.0-3-gabc1234", with "-dirty" for uncommitted changes. This default is for builds without
// the script (the host tests).
#pragma once
#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif
