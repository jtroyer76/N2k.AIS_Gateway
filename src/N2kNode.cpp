#include "N2kNode.h"

#include <Arduino.h>
#include <N2kMessages.h>
#include <Preferences.h>

#include "AisInput.h"
#include "BusHealth.h"
#include "Config.h"
#include "Log.h"
#include "Stats.h"
#include "Version.h"

tNMEA2000_esp32_idf_twai NMEA2000(CAN_TX_PIN, CAN_RX_PIN);

static int nodeAddress;
static Preferences preferences;  // NVS namespace "nvs", key "LastNodeAddress"

static const unsigned long transmitMessages[] = {129038L, 129039L, 129040L, 129794L, 129802L, 129809L, 129810L, 0};

// Today's date for the AIS type 5 ETA year (the converter rejects dates before the build).
static void handleN2kMsg(const tN2kMsg &msg) {
  if (msg.Source != NMEA2000.GetN2kSource()) stats.n2kReceived++;

  uint16_t days;
  double secondsSinceMidnight;
  unsigned char sid;

  if (msg.PGN == 126992UL) {
    tN2kTimeSource source;
    if (ParseN2kSystemTime(msg, sid, days, secondsSinceMidnight, source)) aisSetToday(days);
  } else if (msg.PGN == 129029UL) {
    double lat, lon, alt, hdop, pdop, geoidal, ageOfCorrection;
    tN2kGNSStype gnssType, refType;
    tN2kGNSSmethod method;
    unsigned char nSatellites, nRefStations;
    uint16_t refStationId;
    if (ParseN2kGNSS(msg, sid, days, secondsSinceMidnight, lat, lon, alt, gnssType, method, nSatellites,
                     hdop, pdop, geoidal, nRefStations, refType, refStationId, ageOfCorrection))
      aisSetToday(days);
  }
}

void n2kBegin() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  uint32_t id = 0;
  for (int i = 0; i < 6; i++) id += (uint32_t)mac[i] << (7 * i);
  id &= 0x1FFFFF;  // unique number is 21 bits
  char serial[12];
  snprintf(serial, sizeof(serial), "%lu", (unsigned long)id);

  NMEA2000.SetN2kCANReceiveFrameBufSize(CAN_RX_QUEUE);
  NMEA2000.SetN2kCANSendFrameBufSize(N2K_SEND_FRAME_BUF);

  NMEA2000.SetProductInformation(serial,                     // model serial code
                                 10,                         // product code
                                 "AIS Gateway",              // model ID
                                 FW_VERSION,                 // software version
                                 "M5 Atom + CAN + Tail485"); // model version
  NMEA2000.SetDeviceInformation(id,
                                135,    // function: NMEA 0183 gateway
                                25,     // class: inter/intranetwork device
                                2046);  // manufacturer code (unregistered)
  NMEA2000.ExtendTransmitMessages(transmitMessages);

  preferences.begin("nvs", false);
  nodeAddress = preferences.getInt("LastNodeAddress", 32);
  preferences.end();
  logf(LogLevel::Info, "N2k: node address %d (from NVS)", nodeAddress);

  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, nodeAddress);
  NMEA2000.EnableForward(false);
  NMEA2000.SetMsgHandler(handleN2kMsg);
  NMEA2000.Open();
}

void n2kLoop() {
  NMEA2000.ParseMessages();

  int source = NMEA2000.GetN2kSource();
  if (source != nodeAddress) {  // save a changed address, once
    nodeAddress = source;
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", source);
    preferences.end();
    logf(LogLevel::Info, "N2k: address changed to %d", source);
  }
}

bool n2kSend(const tN2kMsg &msg) {
  bool ok = NMEA2000.SendMsg(msg);
  if (ok) stats.n2kSent++;
  else stats.n2kFailed++;
  return ok;
}

bool n2kHealthy() {
  tNMEA2000_esp32_idf_twai::Status s = NMEA2000.GetStatus();
  BusHealthInputs b;
  b.libraryOpen = NMEA2000.IsOpen();
  b.source = NMEA2000.GetN2kSource();
  b.canRunning = s.state == TWAI_STATE_RUNNING;
  b.txErrorCounter = s.txErrorCounter;
  b.rxErrorCounter = s.rxErrorCounter;
  b.framesFromOthers = stats.n2kReceived;
  return busHealthy(b);
}
