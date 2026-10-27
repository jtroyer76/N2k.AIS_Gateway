#include "Ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_ota_ops.h>

#include "Config.h"
#include "Led.h"
#include "Log.h"
#include "RollbackPolicy.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill it in"
#endif

// Tell the Arduino core not to mark a new OTA image valid at boot; otaLoop() decides (5.11).
extern "C" bool verifyRollbackLater() { return true; }

static RollbackPolicy *rollback = nullptr;
static bool active = false;

void otaBegin() {
  esp_ota_img_states_t state;
  bool pending = esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
                 state == ESP_OTA_IMG_PENDING_VERIFY;
  rollback = new RollbackPolicy(pending);
  if (pending)
    logf(LogLevel::Warn, "OTA: new image pending verification (valid after %lu s healthy, rollback after %lu s)",
         ROLLBACK_MIN_UPTIME_MS / 1000, ROLLBACK_TIMEOUT_MS / 1000);

  ArduinoOTA.setHostname(HOSTNAME);  // also mDNS ais-gateway.local with _arduino._tcp
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() {
    active = true;
    logf(LogLevel::Info, "OTA: update started");
    disableLoopWDT();  // the upload runs inside ArduinoOTA.handle() and blocks the loop
    ledShowNow(LedColor::White);
  });
  ArduinoOTA.onEnd([]() {
    logf(LogLevel::Info, "OTA: update complete, restarting");
    enableLoopWDT();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    active = false;
    logf(LogLevel::Error, "OTA: update failed (error %u)", (unsigned)error);
    enableLoopWDT();
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
