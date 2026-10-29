#include "Web.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <stdlib.h>

#include "CanStatus.h"
#include "Config.h"
#include "JsonOut.h"
#include "Log.h"
#include "LogJson.h"
#include "N2kNode.h"
#include "Net.h"
#include "Ota.h"
#include "Settings.h"
#include "SetupPage.h"
#include "Stats.h"
#include "SysInfo.h"
#include "Version.h"
#include "WebPage.h"

static const uint32_t kRestartDelayMs = 500;  // lets the response reach the browser

static WebServer server(WEB_PORT);
static char out[8192];  // one response at a time: the server is synchronous
static uint32_t restartAt = 0;
static bool restartPending = false;

// send_P writes straight from the buffer (PROGMEM is ordinary memory on the ESP32), avoiding the
// String copy that send() makes.
static void sendJson(const JsonOut &j, int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(code, "application/json", out, j.length());
}

static void sendError(int code, const char *message) {
  JsonOut j(out, sizeof(out));
  j.raw("{\"error\":");
  j.str(message);
  j.raw("}");
  sendJson(j, code);
}

static void scheduleRestart() {
  restartAt = millis() + kRestartDelayMs;
  restartPending = true;
}

// The request came in through one of the gateway's hotspots, not the boat network (SPEC 7.2).
static bool viaHotspot() { return netHotspotOn() && server.client().localIP() == WiFi.softAPIP(); }

static void handlePage() { server.send_P(200, "text/html", kWebPage, strlen_P(kWebPage)); }

static void handleSetupPage() { server.send_P(200, "text/html", kSetupPage, strlen_P(kSetupPage)); }

// With no network saved, the setup page is the only page that makes sense.
static void handleRoot() {
  if (netSetupMode()) handleSetupPage();
  else handlePage();
}

// The device password is only sent through a hotspot (SPEC 7.2).
static void handleSetupGet() {
  const bool hotspot = viaHotspot();
  JsonOut j(out, sizeof(out));
  j.raw("{\"version\":");
  j.str(FW_VERSION);
  j.raw(",\"hostname\":");
  j.str(HOSTNAME);
  j.raw(",\"setup\":");
  j.boolean(netSetupMode());
  j.raw(",\"via_hotspot\":");
  j.boolean(hotspot);
  j.raw(",\"ssid\":");
  j.str(settings().ssid);
  j.raw(",\"ais_baud\":");
  j.num(settings().aisBaud);
  if (hotspot) {
    j.raw(",\"device_pass\":");
    j.str(settings().devicePass);
  }
  j.raw("}");
  sendJson(j);
}

static void handleScanGet() {
  JsonOut j(out, sizeof(out));
  j.raw("{\"scanning\":");
  j.boolean(netScanning());
  j.raw(",\"networks\":[");
  for (int i = 0; i < netFoundCount(); i++) {
    const NetFound &f = netFound(i);
    if (i) j.raw(",");
    j.raw("[");
    j.str(f.ssid);
    j.raw(",");
    j.num((int32_t)f.rssi);
    j.raw(",");
    j.boolean(f.open);
    j.raw("]");
  }
  j.raw("]}");
  sendJson(j);
}

static void handleScanPost() {
  JsonOut j(out, sizeof(out));
  j.raw("{\"started\":");
  j.boolean(netStartScan());
  j.raw("}");
  sendJson(j);
}

static void handleSettingsPost() {
  const String ssid = server.arg("ssid"), wifiPass = server.arg("wifi_pass"), newPass = server.arg("new_pass"),
               password = server.arg("password");
  SettingsForm form = {ssid.c_str(), wifiPass.c_str(), (uint32_t)strtoul(server.arg("ais_baud").c_str(), nullptr, 10),
                       newPass.c_str(), password.c_str()};
  GatewaySettings next = settings();
  const char *error = applySettingsForm(settings(), form, viaHotspot(), next);
  if (error) {
    logf(LogLevel::Warn, "Web: settings not saved: %s", error);
    sendError(strcmp(error, "wrong device password") == 0 ? 403 : 400, error);
    return;
  }
  settingsSave(next);
  server.send(200, "application/json", "{\"saved\":true}");
  logf(LogLevel::Info, "Web: settings saved, restarting");
  scheduleRestart();
}

// Firmware upload (SPEC 7.5): POST /ota, body = the app image (the release's -update.bin),
// header X-OTA-Password = the device password, as Telltale. The server hands the body over in
// chunks; with a wrong password the chunks are read and dropped, and the answer is 403.
static const char *uploadError = nullptr;
static bool uploadStarted = false;

static void handleOtaChunk() {
  HTTPRaw &raw = server.raw();
  switch (raw.status) {
    case RAW_START:
      otaWatchdogPause();  // reading the body blocks the loop, whatever happens to it
      uploadError = nullptr;
      uploadStarted = false;
      if (!passwordMatches(server.header("X-OTA-Password").c_str(), settings().devicePass)) {
        uploadError = "wrong device password";
        logf(LogLevel::Warn, "Web: firmware upload refused: wrong device password");
      } else if (server.clientContentLength() <= 0) {
        uploadError = "no firmware in the request";
      } else if (!otaUploadBegin(server.clientContentLength())) {
        uploadError = "the update could not start";
      } else {
        uploadStarted = true;
      }
      break;
    case RAW_WRITE:
      if (uploadStarted && !otaUploadWrite(raw.buf, raw.currentSize)) {
        uploadError = "writing the firmware failed";
        uploadStarted = false;
        otaUploadAbort();
      }
      break;
    case RAW_END:
      if (uploadStarted && !otaUploadEnd()) uploadError = "the firmware image is not valid";
      uploadStarted = false;
      otaWatchdogResume();
      break;
    case RAW_ABORTED:
      if (uploadStarted) otaUploadAbort();
      uploadStarted = false;
      uploadError = "the upload was interrupted";
      otaWatchdogResume();
      break;
  }
}

static void handleOtaDone() {
  if (uploadError) {
    sendError(strcmp(uploadError, "wrong device password") == 0 ? 403 : 400, uploadError);
    return;
  }
  server.send(200, "application/json", "{\"updated\":true}");
  scheduleRestart();
}

// On a hotspot, unknown URLs (a phone's connectivity check, for one) go to the page, so the
// phone opens it by itself.
static void handleNotFound() {
  if (viaHotspot()) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "not found");
}

static void handleStatus() {
  const uint32_t now = millis();
  tNMEA2000_esp32_idf_twai::Status can = NMEA2000.GetStatus();
  JsonOut j(out, sizeof(out));

  j.raw("{\"version\":");
  j.str(FW_VERSION);
  j.raw(",\"uptime_ms\":");
  j.num(now);
  j.raw(",\"reset\":");
  j.str(resetReason());
  j.raw(",\"partition\":");
  j.str(runningPartition());

  j.raw(",\"heap\":{\"free\":");
  j.num((uint32_t)ESP.getFreeHeap());
  j.raw(",\"min\":");
  j.num((uint32_t)ESP.getMinFreeHeap());
  j.raw("}");

  bool connected = WiFi.status() == WL_CONNECTED;
  j.raw(",\"wifi\":{\"connected\":");
  j.boolean(connected);
  j.raw(",\"ssid\":");
  j.str(connected ? WiFi.SSID().c_str() : "");
  j.raw(",\"rssi\":");
  j.num((int32_t)(connected ? WiFi.RSSI() : 0));
  j.raw(",\"ip\":");
  j.str(connected ? WiFi.localIP().toString().c_str() : "");
  j.raw(",\"hotspot\":");
  j.boolean(netHotspotOn());
  j.raw(",\"hotspot_ip\":");
  j.str(netHotspotOn() ? WiFi.softAPIP().toString().c_str() : "");
  j.raw(",\"hotspot_clients\":");
  j.num((uint32_t)(netHotspotOn() ? WiFi.softAPgetStationNum() : 0));
  j.raw(",\"setup\":");
  j.boolean(netSetupMode());
  j.raw("}");

  j.raw(",\"ais\":{\"baud\":");
  j.num(settings().aisBaud);
  j.raw(",\"sentences\":");
  j.num(stats.sentences);
  j.raw(",\"ais\":");
  j.num(stats.aisSentences);
  j.raw(",\"decoded\":");
  j.num(stats.aisDecoded);
  j.raw(",\"errors\":");
  j.num(stats.decodeErrors);
  j.raw(",\"since_last_s\":");
  if (stats.everAis) j.num((now - stats.lastAisMs) / 1000);
  else j.raw("null");
  j.raw("}");

  j.raw(",\"n2k\":{\"address\":");
  j.num((uint32_t)NMEA2000.GetN2kSource());
  j.raw(",\"sent\":");
  j.num(stats.n2kSent);
  j.raw(",\"failed\":");
  j.num(stats.n2kFailed);
  j.raw(",\"rx\":");
  j.num(stats.n2kReceived);
  j.raw("}");

  j.raw(",\"can\":{\"state\":");
  j.str(canStateName());
  j.raw(",\"tec\":");
  j.num(can.txErrorCounter);
  j.raw(",\"rec\":");
  j.num(can.rxErrorCounter);
  j.raw(",\"bus_off\":");
  j.num(can.busOffCount);
  j.raw(",\"recoveries\":");
  j.num(can.recoveryCount);
  j.raw(",\"restart_failures\":");
  j.num(can.restartFailures);
  j.raw(",\"tx_failed\":");
  j.num(can.txFailed);
  j.raw(",\"rx_missed\":");
  j.num(can.rxMissed);
  j.raw(",\"rx_overrun\":");
  j.num(can.rxOverrun);
  j.raw("}");

  j.raw(",\"raw\":[");
  for (size_t i = 0; i < stats.raw.size(); i++) {
    if (i) j.raw(",");
    j.str(stats.raw.at(i));
  }
  j.raw("]}");

  if (!j.ok()) {
    logf(LogLevel::Error, "Web: status response did not fit the buffer");
    server.send(500, "text/plain", "status too large");
    return;
  }
  sendJson(j);
}

static void handleLog() {
  uint32_t since = server.hasArg("since") ? strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
  JsonOut j(out, sizeof(out));
  writeLogJson(j, consoleLog(), since, LOG_JSON_MAX_LINES);
  sendJson(j);
}

static void handleRestart() {
  server.send(200, "application/json", "{\"restarting\":true}");
  logf(LogLevel::Warn, "Web: restart requested");
  scheduleRestart();
}

void webBegin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/setup", HTTP_GET, handleSetupGet);
  server.on("/api/scan", HTTP_GET, handleScanGet);
  server.on("/api/scan", HTTP_POST, handleScanPost);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/ota", HTTP_POST, handleOtaDone, handleOtaChunk);
  server.onNotFound(handleNotFound);
  static const char *headers[] = {"X-OTA-Password"};
  server.collectHeaders(headers, 1);
  server.begin();
  logf(LogLevel::Info, "Web: console on port %d", WEB_PORT);
}

void webLoop(uint32_t now) {
  server.handleClient();
  if (restartPending && (int32_t)(now - restartAt) >= 0) {
    Serial.flush();
    ESP.restart();
  }
}
