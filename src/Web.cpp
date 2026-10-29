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
#include "Settings.h"
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
static void sendJson(const JsonOut &j) {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/json", out, j.length());
}

static void handlePage() { server.send_P(200, "text/html", kWebPage, strlen_P(kWebPage)); }

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
  restartAt = millis() + kRestartDelayMs;
  restartPending = true;
}

void webBegin() {
  server.on("/", HTTP_GET, handlePage);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
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
