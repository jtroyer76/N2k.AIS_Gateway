#include "Ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_ota_ops.h>

#include "Config.h"
#include "ImageCheck.h"
#include "Led.h"
#include "Log.h"
#include "RollbackPolicy.h"
#include "Settings.h"

// Tell the Arduino core not to mark a new OTA image valid at boot; otaLoop() decides (5.11).
extern "C" bool verifyRollbackLater() { return true; }

static RollbackPolicy *rollback = nullptr;
static bool active = false;
static bool wdtPaused = false;

// ArduinoOTA can report several errors for one failed upload, so pause/resume only once each.
static void pauseWdt() {
  if (wdtPaused) return;
  disableLoopWDT();
  wdtPaused = true;
}

static void resumeWdt() {
  if (!wdtPaused) return;
  enableLoopWDT();
  wdtPaused = false;
}

void otaBegin() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  bool pending = esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
  rollback = new RollbackPolicy(pending);
  logf(LogLevel::Info, "OTA: running from %s", running ? running->label : "?");
  if (pending)
    logf(LogLevel::Warn, "OTA: new image pending verification (valid after %lu s healthy, rollback after %lu s)",
         ROLLBACK_MIN_UPTIME_MS / 1000, ROLLBACK_TIMEOUT_MS / 1000);

  ArduinoOTA.setHostname(HOSTNAME);  // also mDNS ais-gateway.local with _arduino._tcp
  ArduinoOTA.setPassword(settings().devicePass);
  ArduinoOTA.onStart([]() {
    active = true;
    logf(LogLevel::Info, "OTA: update started");
    pauseWdt();  // the upload runs inside ArduinoOTA.handle() and blocks the loop
    ledShowNow(LedColor::White);
  });
  ArduinoOTA.onEnd([]() {
    logf(LogLevel::Info, "OTA: update complete, restarting");
    resumeWdt();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    active = false;
    logf(LogLevel::Error, "OTA: update failed (error %u)", (unsigned)error);
    resumeWdt();
  });
  ArduinoOTA.begin();
}

void otaLoop(uint32_t now, bool healthy) {
  ArduinoOTA.handle();

  switch (rollback->step(now, healthy)) {
    case RollbackAction::None:
      break;
    case RollbackAction::MarkValid:
      if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) logf(LogLevel::Info, "OTA: new image marked valid");
      else logf(LogLevel::Error, "OTA: marking image valid failed");
      break;
    case RollbackAction::Restart:
      logf(LogLevel::Error, "OTA: new image not healthy after %lu s, restarting to roll back", ROLLBACK_TIMEOUT_MS / 1000);
      Serial.flush();
      ESP.restart();
      break;
  }
}

bool otaActive() { return active; }

void otaWatchdogPause() { pauseWdt(); }
void otaWatchdogResume() { resumeWdt(); }

// The first bytes of the upload, collected until the image header can be checked (ImageCheck.h).
static uint8_t imageHead[IMAGE_HEADER_MIN];
static size_t imageHeadLen = 0;
static const char *uploadError = nullptr;
static char uploadErrorText[96];

const char *otaUploadError() { return uploadError; }

// False if the header shows the image can't run here. Bytes already passed to Update only reach
// the inactive slot, and an aborted update is never made bootable.
static bool checkHead(const uint8_t *data, size_t len) {
  if (imageHeadLen >= IMAGE_HEADER_MIN) return true;  // already checked
  size_t take = IMAGE_HEADER_MIN - imageHeadLen;
  if (take > len) take = len;
  memcpy(imageHead + imageHeadLen, data, take);
  imageHeadLen += take;
  if (imageHeadLen < IMAGE_HEADER_MIN) return true;  // wait for more

  uint16_t chip = 0;
  switch (checkImageStart(imageHead, imageHeadLen, CONFIG_IDF_FIRMWARE_CHIP_ID, &chip)) {
    case ImageCheck::Ok: return true;
    case ImageCheck::TooShort: return true;  // not reached: the header is complete here
    case ImageCheck::NotAnImage: uploadError = "the file is not a firmware image"; break;
    case ImageCheck::WrongChip:
      snprintf(uploadErrorText, sizeof(uploadErrorText), "the firmware is for %s, this gateway is %s (%s)",
               chipName(chip), chipName(CONFIG_IDF_FIRMWARE_CHIP_ID), BOARD_NAME);
      uploadError = uploadErrorText;
      break;
  }
  logf(LogLevel::Error, "OTA: web upload refused: %s", uploadError);
  return false;
}

bool otaUploadBegin(size_t size) {
  imageHeadLen = 0;
  uploadError = nullptr;
  if (!Update.begin(size)) {
    logf(LogLevel::Error, "OTA: web upload refused: %s", Update.errorString());
    return false;
  }
  active = true;
  logf(LogLevel::Info, "OTA: web upload started (%u bytes)", (unsigned)size);
  pauseWdt();  // the upload blocks the loop
  ledShowNow(LedColor::White);
  return true;
}

bool otaUploadWrite(uint8_t *data, size_t len) {
  if (!checkHead(data, len)) return false;
  if (Update.write(data, len) == len) return true;
  logf(LogLevel::Error, "OTA: web upload write failed: %s", Update.errorString());
  return false;
}

bool otaUploadEnd() {
  if (!Update.end()) {
    logf(LogLevel::Error, "OTA: web upload failed: %s", Update.errorString());
    active = false;
    resumeWdt();
    return false;
  }
  logf(LogLevel::Info, "OTA: web upload complete, restarting");
  resumeWdt();
  return true;
}

// The watchdog stays paused: the web server still reads and drops the rest of the body, which
// blocks the loop for as long as the upload takes. It resumes the watchdog when the request ends.
void otaUploadAbort() {
  Update.abort();
  logf(LogLevel::Error, "OTA: web upload aborted");
  active = false;
}
