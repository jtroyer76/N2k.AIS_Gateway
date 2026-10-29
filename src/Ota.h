// OTA updates and rollback protection (SPEC 5.10, 5.11), from PlatformIO (ArduinoOTA) or as a
// firmware upload to the web server (7.5).
#pragma once

#include <stddef.h>
#include <stdint.h>

void otaBegin();
void otaLoop(uint32_t now, bool healthy);  // healthy: see n2kHealthy()
bool otaActive();

// Firmware upload over HTTP: the web server feeds the request body in chunks. Reading the body
// blocks the loop, even when the password is wrong and the body is only dropped, so the web
// server pauses the loop watchdog for the whole request. Idempotent.
void otaWatchdogPause();
void otaWatchdogResume();

bool otaUploadBegin(size_t size);
bool otaUploadWrite(uint8_t *data, size_t len);
bool otaUploadEnd();  // true: the image is complete and set to boot (pending verification, 5.11)
void otaUploadAbort();
