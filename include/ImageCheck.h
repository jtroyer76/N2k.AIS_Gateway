// Firmware upload check (SPEC 7.5): an update file built for another board's chip is refused
// before anything is written. The ESP-IDF app image header (esp_image_header_t in
// esp_app_format.h) starts with the magic byte 0xE9 and holds the chip ID as a little-endian
// uint16 at offset 12: 0x0000 ESP32 (Atom), 0x0009 ESP32-S3 (XIAO). The Update library only checks
// the magic byte. Platform-free so it runs in the host tests; src/Ota.cpp applies it.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define IMAGE_MAGIC           0xE9
#define IMAGE_CHIP_ID_OFFSET  12
#define IMAGE_HEADER_MIN      (IMAGE_CHIP_ID_OFFSET + 2)

enum class ImageCheck { Ok, TooShort, NotAnImage, WrongChip };

// `start` is the first `len` bytes of the upload. `chip` gets the image's chip ID when there is one.
inline ImageCheck checkImageStart(const uint8_t *start, size_t len, uint16_t expectedChip, uint16_t *chip) {
  if (len < IMAGE_HEADER_MIN) return ImageCheck::TooShort;
  if (start[0] != IMAGE_MAGIC) return ImageCheck::NotAnImage;
  uint16_t id = (uint16_t)(start[IMAGE_CHIP_ID_OFFSET] | (start[IMAGE_CHIP_ID_OFFSET + 1] << 8));
  if (chip) *chip = id;
  return id == expectedChip ? ImageCheck::Ok : ImageCheck::WrongChip;
}

inline const char *chipName(uint16_t id) {
  switch (id) {
    case 0x0000: return "ESP32";
    case 0x0002: return "ESP32-S2";
    case 0x0005: return "ESP32-C3";
    case 0x0009: return "ESP32-S3";
  }
  return "another chip";
}
