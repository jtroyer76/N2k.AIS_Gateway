#include "AisInput.h"

#include <Arduino.h>
#include <NMEA0183.h>

#include "Config.h"
#include "Log.h"
#include "N2kNode.h"
#include "NMEA0183AIStoNMEA2000.h"
#include "Stats.h"

static const int kMaxSentencesPerPoll = 10;  // bounded work per loop

static uint32_t tooLong = 0;  // sentences that didn't fit the buffer

static tNMEA0183 nmea0183;
static tNMEA0183Msg nmea0183Msg;
static AIS::DefaultSentenceParser parser;
static MyAisDecoder decoder(
    [](const tN2kMsg &msg) { n2kSend(msg); },
    [](const char *line) { logf(LogLevel::Warn, "AIS: %s", line); });

void aisBegin() {
  Serial1.setRxBufferSize(SERIAL_RX_BUFFER);  // before begin()
  Serial1.begin(AIS_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  // SetMessageStream + Open, not Begin(): Begin() calls begin() again without our pins.
  nmea0183.SetMessageStream(&Serial1);
  nmea0183.Open();
  stats.lastAisMs = millis();
  logf(LogLevel::Info, "AIS: %d baud on RX GPIO%d, own ship %s", AIS_BAUD, RS485_RX_PIN,
       FORWARD_OWN_SHIP ? "forwarded" : "ignored");
}

void aisSetToday(uint16_t daysSince1970) { decoder.setDaysSince1970(daysSince1970); }

static void handleSentence(uint32_t now) {
  stats.sentences++;
  bool isVdm = nmea0183Msg.IsMessageCode("VDM");
  if (!isVdm && !(FORWARD_OWN_SHIP && nmea0183Msg.IsMessageCode("VDO"))) return;

  // Room for the "\n" the decoder expects: GetMessage() is given one byte less than the buffer.
  char buf[MAX_NMEA0183_MSG_BUF_LEN + 8];
  if (!nmea0183Msg.GetMessage(buf, sizeof(buf) - 1)) {
    tooLong++;
    stats.decodeErrors = (uint32_t)decoder.getDecodingErrorCount() + tooLong;
    logf(LogLevel::Warn, "AIS: sentence too long for buffer");
    return;
  }
  stats.aisSentences++;
  stats.lastAisMs = now;
  stats.everAis = true;
  stats.raw.push(buf);

  strcat(buf, "\n");
  size_t i = 0;
  do {
    i = decoder.decodeMsg(buf, strlen(buf), i, parser);
  } while (i != 0);

  stats.aisDecoded = (uint32_t)decoder.getTotalMessageCount();
  stats.decodeErrors = (uint32_t)decoder.getDecodingErrorCount() + tooLong;
}

void aisPoll(uint32_t now) {
  for (int n = 0; n < kMaxSentencesPerPoll && nmea0183.GetMessage(nmea0183Msg); n++) handleSentence(now);
}
