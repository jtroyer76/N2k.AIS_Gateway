// AIS -> N2k conversion tests (SPEC §5.4.1), run on the host: `pio test -e native`.
//
// Every input is a real !AIVDM sentence with its original checksum and multi-part framing,
// not hand-written. Sources:
//   [gpsd]  gpsd test/sample.aivdm            https://gitlab.com/gpsd/gpsd (BSD-2-Clause)
//   [pyais] pyais tests/nmea-sample            https://github.com/M0r13n/pyais (MIT)
// One exception, marked [pyais-enc]: no auxiliary-craft type 24B (MMSI 98xxxxxxx) was found in
// the recorded samples, so that vector is pyais's own encoder test vector (tests/test_decode.py,
// test_msg_type_24_b_aux_craft). Its checksum and framing are valid, but it is not live traffic.
// Replace it with a WatchMate capture when one turns up.
//
// Expected values come from pyais 3.3.1 decoding the same sentences, not from this code.
// The converter's output is read back with the NMEA2000 library's own parsers (or at canboat's
// field offsets for PGN 129040, which the library has no parser for).

#include <unity.h>
#include <string.h>
#include <vector>

#include "NMEA0183AIStoNMEA2000.h"

// The NMEA2000 library needs the application to provide millis() off Arduino (N2kTimer.cpp).
extern "C" uint32_t millis() { return 0; }

static const double kn = 1852.0 / 3600.0;
static const double deg = 3.1415926535897932384626433832795 / 180.0;

// 2026-10-27, the date these tests were written; stands in for the firmware build date.
static const uint16_t kBuildDay = (uint16_t)AisDate::daysFromCivil(2026, 10, 27);

static std::vector<tN2kMsg> sent;
static MyAisDecoder *decoder = nullptr;
static AIS::DefaultSentenceParser parser;

static void newDecoder() {
  delete decoder;
  decoder = new MyAisDecoder([](const tN2kMsg &m) { sent.push_back(m); }, nullptr, kBuildDay);
}

void setUp() {
  sent.clear();
  newDecoder();
}

void tearDown() {}

// Feeds sentences exactly as the gateway does (one line, "\n" appended) and returns the
// single N2k message they produce.
static const tN2kMsg &feed(std::initializer_list<const char *> lines) {
  for (const char *line : lines) {
    std::string buf = std::string(line) + "\n";
    size_t i = 0;
    do {
      i = decoder->decodeMsg(buf.c_str(), buf.size(), i, parser);
    } while (i != 0);
  }
  TEST_ASSERT_EQUAL_MESSAGE(1, sent.size(), "expected exactly one N2k message");
  return sent.back();
}

// The converter space-pads AIS strings to their field width; compare without the padding.
static std::string trimmed(const char *s) {
  std::string r(s);
  r.erase(r.find_last_not_of(' ') + 1);
  return r;
}
#define ASSERT_STR(expected, actual) TEST_ASSERT_EQUAL_STRING(expected, trimmed(actual).c_str())

static void assertNA(double v, const char *what) {
  TEST_ASSERT_TRUE_MESSAGE(N2kIsNA(v), what);
}

//*****************************************************************************
// Types 1-3 -> PGN 129038

struct ClassAPos {
  uint8_t MessageID; tN2kAISRepeat Repeat; uint32_t UserID;
  double Latitude, Longitude; bool Accuracy, RAIM; uint8_t Seconds;
  double COG, SOG, Heading, ROT; tN2kAISNavStatus NavStatus;
  tN2kAISTransceiverInformation AISInfo; uint8_t SID;
};

static ClassAPos parse129038(const tN2kMsg &m) {
  ClassAPos p;
  TEST_ASSERT_TRUE(ParseN2kPGN129038(m, p.MessageID, p.Repeat, p.UserID, p.Latitude, p.Longitude,
                                     p.Accuracy, p.RAIM, p.Seconds, p.COG, p.SOG, p.Heading, p.ROT,
                                     p.NavStatus, p.AISInfo, p.SID));
  return p;
}

// [gpsd] type 1, all fields available.
void test_type1_available_fields() {
  ClassAPos p = parse129038(feed({"!AIVDM,1,1,,A,15RTgt0PAso;90TKcjM8h6g208CQ,0*4A"}));
  TEST_ASSERT_EQUAL_UINT32(371798000, p.UserID);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 48.381633, p.Latitude);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, -123.395383, p.Longitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 12.3 * kn, p.SOG);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 224.0 * deg, p.COG);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 215.0 * deg, p.Heading);
  TEST_ASSERT_FALSE(N2kIsNA(p.ROT));
  TEST_ASSERT_TRUE(p.ROT < 0);  // ROT -127: turning left, no turn indicator
  TEST_ASSERT_EQUAL(33, p.Seconds);
  TEST_ASSERT_EQUAL(N2kaischannel_A_VDL_reception, p.AISInfo);
}

// [gpsd] type 1, rate of turn not available (-128), heading not available (511).
void test_type1_rot_not_available() {
  ClassAPos p = parse129038(feed({"!AIVDM,1,1,,A,16SteH0P00Jt63hHaa6SagvJ087r,0*42"}));
  TEST_ASSERT_EQUAL_UINT32(440348000, p.UserID);
  assertNA(p.ROT, "ROT -128 must be NA");
  assertNA(p.Heading, "heading 511 must be NA");
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, p.SOG);
}

// [pyais] type 3: SOG 1023, lon 181, lat 91, ROT -128, COG 3600, heading 511 - all not available.
void test_type3_all_not_available() {
  ClassAPos p = parse129038(feed({"!AIVDM,1,1,,B,33`hBOEP?w<tSF0l4Q@>4?wv0qiP,0*15"}));
  TEST_ASSERT_EQUAL_UINT32(244060797, p.UserID);
  TEST_ASSERT_EQUAL(3, p.MessageID);
  assertNA(p.SOG, "SOG 1023 must be NA");
  assertNA(p.Longitude, "longitude 181 must be NA");
  assertNA(p.Latitude, "latitude 91 must be NA");
  assertNA(p.ROT, "ROT -128 must be NA");
  assertNA(p.COG, "COG 3600 must be NA");
  assertNA(p.Heading, "heading 511 must be NA");
  TEST_ASSERT_EQUAL(N2kaischannel_B_VDL_reception, p.AISInfo);
}

//*****************************************************************************
// Type 18 -> PGN 129039

struct ClassBPos {
  uint8_t MessageID; tN2kAISRepeat Repeat; uint32_t UserID;
  double Latitude, Longitude; bool Accuracy, RAIM; uint8_t Seconds;
  double COG, SOG; tN2kAISTransceiverInformation AISInfo; double Heading;
  tN2kAISUnit Unit; bool Display, DSC, Band, Msg22; tN2kAISMode Mode; bool State; uint8_t SID;
};

static ClassBPos parse129039(const tN2kMsg &m) {
  ClassBPos p;
  TEST_ASSERT_TRUE(ParseN2kPGN129039(m, p.MessageID, p.Repeat, p.UserID, p.Latitude, p.Longitude,
                                     p.Accuracy, p.RAIM, p.Seconds, p.COG, p.SOG, p.AISInfo,
                                     p.Heading, p.Unit, p.Display, p.DSC, p.Band, p.Msg22, p.Mode,
                                     p.State, p.SID));
  return p;
}

// [gpsd] type 18, available fields.
void test_type18_available_fields() {
  ClassBPos p = parse129039(feed({"!AIVDM,1,1,,B,B5O6hr00<veEKmUaMFdEow`UWP06,0*4F"}));
  TEST_ASSERT_EQUAL_UINT32(368161000, p.UserID);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 39.480925, p.Latitude);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, -72.233848, p.Longitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 5.1 * kn, p.SOG);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 34.9 * deg, p.COG);
  assertNA(p.Heading, "heading 511 must be NA");
  TEST_ASSERT_EQUAL(N2kaischannel_B_VDL_reception, p.AISInfo);
}

// [pyais] type 18: SOG, position, COG, heading not available.
void test_type18_not_available() {
  ClassBPos p = parse129039(feed({"!AIVDM,1,1,,A,B69A5U@3wk?8mP=18D3Q3wSRP51L,0*50"}));
  TEST_ASSERT_EQUAL_UINT32(412370325, p.UserID);
  assertNA(p.SOG, "SOG 1023 must be NA");
  assertNA(p.Longitude, "longitude 181 must be NA");
  assertNA(p.Latitude, "latitude 91 must be NA");
  assertNA(p.COG, "COG 3600 must be NA");
}

//*****************************************************************************
// Type 19 -> PGN 129040 (hand-built; checked against canboat's field layout, 54 bytes)

// canboat aisClassBExtendedPositionReport byte offsets
static const int k129040Len = 54;

struct ClassBExt {
  uint32_t UserID; double Longitude, Latitude, COG, SOG; uint8_t ShipType; double Heading;
  uint8_t Gnss; double Length, Beam, PosRefStbd, PosRefBow; char Name[21];
  bool DTE, Assigned; uint8_t AISInfo;
};

static ClassBExt parse129040(const tN2kMsg &m) {
  TEST_ASSERT_EQUAL_UINT32(129040UL, m.PGN);
  ClassBExt p;
  int i = 1;
  p.UserID = m.Get4ByteUInt(i);
  p.Longitude = m.Get4ByteDouble(1e-07, i);
  p.Latitude = m.Get4ByteDouble(1e-07, i);
  i++;                                          // accuracy / RAIM / timestamp
  p.COG = m.Get2ByteUDouble(1e-04, i);
  p.SOG = m.Get2ByteUDouble(0.01, i);
  i += 2;                                       // regional application
  p.ShipType = m.GetByte(i);
  p.Heading = m.Get2ByteUDouble(1e-04, i);
  p.Gnss = m.GetByte(i) >> 4;
  p.Length = m.Get2ByteDouble(0.1, i);
  p.Beam = m.Get2ByteDouble(0.1, i);
  p.PosRefStbd = m.Get2ByteDouble(0.1, i);
  p.PosRefBow = m.Get2ByteDouble(0.1, i);
  m.GetStr(sizeof(p.Name), p.Name, 20, '@', i);
  uint8_t b52 = m.GetByte(i), b53 = m.GetByte(i);
  p.DTE = b52 & 0x01;
  p.Assigned = (b52 >> 1) & 0x01;
  p.AISInfo = ((b52 >> 6) & 0x03) | ((b53 & 0x07) << 2);  // bits 422..426
  return p;
}

// [gpsd] type 19, available fields.
void test_type19_available_fields_and_length() {
  const tN2kMsg &m = feed({"!AIVDM,1,1,,B,C5N3SRgPEnJGEBT>NhWAwwo862PaLELTBJ:V00000000S0D:R220,0*0B"});
  TEST_ASSERT_EQUAL_MESSAGE(k129040Len, m.DataLen, "PGN 129040 must match canboat's 54-byte layout");
  ClassBExt p = parse129040(m);
  TEST_ASSERT_EQUAL_UINT32(367059850, p.UserID);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 29.543695, p.Latitude);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, -88.810392, p.Longitude);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 8.7 * kn, p.SOG);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, 335.9 * deg, p.COG);
  assertNA(p.Heading, "heading 511 must be NA");
  TEST_ASSERT_EQUAL(70, p.ShipType);
  TEST_ASSERT_EQUAL(1, p.Gnss);  // GPS
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 26.0, p.Length);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 8.0, p.Beam);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 4.0, p.PosRefStbd);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 5.0, p.PosRefBow);
  ASSERT_STR("CAPT.J.RIMES", p.Name);
  TEST_ASSERT_FALSE(p.DTE);
  TEST_ASSERT_FALSE(p.Assigned);
  TEST_ASSERT_EQUAL(N2kaischannel_B_VDL_reception, p.AISInfo);
}

// [pyais] type 19: SOG, position, COG, heading not available.
void test_type19_not_available() {
  const tN2kMsg &m = feed({"!AIVDM,1,1,,B,C95jQj@3wk?8mP=18D3Q3wgVhBLjBM1kQ11111111110S3L7SRP7,0*4C"});
  TEST_ASSERT_EQUAL(k129040Len, m.DataLen);
  ClassBExt p = parse129040(m);
  TEST_ASSERT_EQUAL_UINT32(610050505, p.UserID);
  assertNA(p.SOG, "SOG 1023 must be NA");
  assertNA(p.Longitude, "longitude 181 must be NA");
  assertNA(p.Latitude, "latitude 91 must be NA");
  assertNA(p.COG, "COG 3600 must be NA");
  ASSERT_STR("XINYIN 90", p.Name);
}

//*****************************************************************************
// Type 5 -> PGN 129794

struct ClassAStatic {
  uint8_t MessageID; tN2kAISRepeat Repeat; uint32_t UserID, IMO;
  char Callsign[8], Name[21], Destination[21]; uint8_t VesselType;
  double Length, Beam, PosRefStbd, PosRefBow; uint16_t ETAdate; double ETAtime, Draught;
  tN2kAISVersion Version; tN2kGNSStype Gnss; tN2kAISDTE DTE;
  tN2kAISTransceiverInformation AISInfo; uint8_t SID;
};

static ClassAStatic parse129794(const tN2kMsg &m) {
  ClassAStatic p;
  TEST_ASSERT_TRUE(ParseN2kPGN129794(m, p.MessageID, p.Repeat, p.UserID, p.IMO,
                                     p.Callsign, sizeof(p.Callsign), p.Name, sizeof(p.Name),
                                     p.VesselType, p.Length, p.Beam, p.PosRefStbd, p.PosRefBow,
                                     p.ETAdate, p.ETAtime, p.Draught,
                                     p.Destination, sizeof(p.Destination),
                                     p.Version, p.Gnss, p.DTE, p.AISInfo, p.SID));
  return p;
}

// [pyais] type 5, channel A, AIS version 2, ETA 02-27 14:30, draught 1.7 m.
static const char *kMerwestroom1 = "!AIVDM,2,1,6,A,53`o0Fp2F9T10@h4000lE9LE=A8ttl000000000o10P734efN4C1CCAD,0*29";
static const char *kMerwestroom2 = "!AIVDM,2,2,6,A,P00000000000000,2*42";

static ClassAStatic merwestroomOn(int y, unsigned mo, unsigned d) {
  decoder->setDaysSince1970((uint16_t)AisDate::daysFromCivil(y, mo, d));
  return parse129794(feed({kMerwestroom1, kMerwestroom2}));
}

void test_type5_available_fields() {
  ClassAStatic p = merwestroomOn(2026, 10, 27);
  TEST_ASSERT_EQUAL_UINT32(244170843, p.UserID);
  ASSERT_STR("MERWESTROOM", p.Name);
  ASSERT_STR("PDLA", p.Callsign);
  ASSERT_STR("LEMMER", p.Destination);
  TEST_ASSERT_EQUAL(55, p.VesselType);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 40.0, p.Length);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 10.0, p.Beam);
  TEST_ASSERT_DOUBLE_WITHIN(0.001, 1.7, p.Draught);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 14 * 3600 + 30 * 60, p.ETAtime);
  TEST_ASSERT_EQUAL(2, p.Version);
}

// Transceiver information is the reception channel, not the AIS version.
void test_type5_transceiver_info_channel_A() {
  ClassAStatic p = merwestroomOn(2026, 10, 27);  // version 2, channel A
  TEST_ASSERT_EQUAL(N2kaischannel_A_VDL_reception, p.AISInfo);
}

// [pyais] type 5, channel B, AIS version 2.
void test_type5_transceiver_info_channel_B() {
  ClassAStatic p = parse129794(feed({
      "!AIVDM,2,1,9,B,55R3Vn82=ILTQ3KKS>1<D60Dq@E918U<F222221J1`?164vc03S1CCAD,0*2A",
      "!AIVDM,2,2,9,B,`88888888888880,2*76"}));
  TEST_ASSERT_EQUAL_UINT32(371255000, p.UserID);
  TEST_ASSERT_EQUAL(N2kaischannel_B_VDL_reception, p.AISInfo);
}

// ETA year is the next occurrence of month/day relative to today.
void test_type5_eta_next_occurrence_later_this_year_or_next() {
  TEST_ASSERT_EQUAL_UINT16(AisDate::daysFromCivil(2027, 2, 27), merwestroomOn(2026, 10, 27).ETAdate);
}

void test_type5_eta_year_rollover() {
  TEST_ASSERT_EQUAL_UINT16(AisDate::daysFromCivil(2027, 2, 27), merwestroomOn(2026, 12, 28).ETAdate);
}

void test_type5_eta_today_is_this_occurrence() {
  TEST_ASSERT_EQUAL_UINT16(AisDate::daysFromCivil(2027, 2, 27), merwestroomOn(2027, 2, 27).ETAdate);
}

void test_type5_eta_day_after_is_next_year() {
  TEST_ASSERT_EQUAL_UINT16(AisDate::daysFromCivil(2028, 2, 27), merwestroomOn(2027, 2, 28).ETAdate);
}

void test_type5_eta_date_na_without_today() {
  ClassAStatic p = parse129794(feed({kMerwestroom1, kMerwestroom2}));
  TEST_ASSERT_EQUAL_UINT16(N2kUInt16NA, p.ETAdate);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 14 * 3600 + 30 * 60, p.ETAtime);  // time is still known
}

// The old SeaTalk GPS reports 2007 (GPS week rollover); dates before the build date are rejected.
void test_type5_eta_date_na_with_date_before_build() {
  TEST_ASSERT_EQUAL_UINT16(N2kUInt16NA, merwestroomOn(2007, 3, 15).ETAdate);
}

// A rejected date must not overwrite an accepted one.
void test_type5_rejected_date_keeps_previous_today() {
  decoder->setDaysSince1970((uint16_t)AisDate::daysFromCivil(2026, 12, 28));
  TEST_ASSERT_EQUAL_UINT16(AisDate::daysFromCivil(2027, 2, 27), merwestroomOn(2007, 3, 15).ETAdate);
}

// [pyais] type 5: ETA month 0, day 0, hour 24, minute 60 and draught 0 - all not available.
void test_type5_eta_and_draught_not_available() {
  decoder->setDaysSince1970(kBuildDay);
  ClassAStatic p = parse129794(feed({
      "!AIVDM,2,1,4,B,53aIk>`000010OW7S80L45=@E8h4p@000000000Q:0665t0Ht0000000,0*4B",
      "!AIVDM,2,2,4,B,000000000000000,2*23"}));
  TEST_ASSERT_EQUAL_UINT32(244740922, p.UserID);
  ASSERT_STR("GAASTERLAND", p.Name);
  TEST_ASSERT_EQUAL_UINT16_MESSAGE(N2kUInt16NA, p.ETAdate, "ETA month/day 0 must be NA");
  assertNA(p.ETAtime, "ETA hour 24 / minute 60 must be NA");
  assertNA(p.Draught, "draught 0 must be NA");
}

//*****************************************************************************
// Type 14 -> PGN 129802

// [gpsd] type 14.
void test_type14_safety_text() {
  const tN2kMsg &m = feed({"!AIVDM,1,1,,A,>5?Per18=HB1U:1@E=B0m<L,2*51"});
  uint8_t MessageID; tN2kAISRepeat Repeat; uint32_t SourceID; tN2kAISTransceiverInformation Info;
  char text[162]; size_t textSize = sizeof(text);
  TEST_ASSERT_TRUE(ParseN2kPGN129802(m, MessageID, Repeat, SourceID, Info, text, textSize));
  TEST_ASSERT_EQUAL(14, MessageID);
  TEST_ASSERT_EQUAL_UINT32(351809000, SourceID);
  TEST_ASSERT_EQUAL_STRING("RCVD YR TEST MSG", text);
  TEST_ASSERT_EQUAL(N2kaischannel_A_VDL_reception, Info);
}

//*****************************************************************************
// Type 24 -> PGN 129809 / 129810

// [gpsd] type 24A.
void test_type24A_name() {
  const tN2kMsg &m = feed({"!AIVDM,1,1,,A,H42O55i18tMET00000000000000,2*6D"});
  uint8_t MessageID; tN2kAISRepeat Repeat; uint32_t UserID; char Name[21];
  TEST_ASSERT_TRUE(ParseN2kAISClassBStaticPartA(m, MessageID, Repeat, UserID, Name, sizeof(Name)));
  TEST_ASSERT_EQUAL_UINT32(271041815, UserID);
  ASSERT_STR("PROGUY", Name);
}

struct ClassBStaticB {
  uint8_t MessageID; tN2kAISRepeat Repeat; uint32_t UserID; uint8_t VesselType;
  char Vendor[8], Callsign[8]; double Length, Beam, PosRefStbd, PosRefBow;
  uint32_t MothershipID; tN2kAISTransceiverInformation AISInfo; uint8_t SID;
};

static ClassBStaticB parse129810(const tN2kMsg &m) {
  ClassBStaticB p;
  TEST_ASSERT_TRUE(ParseN2kPGN129810(m, p.MessageID, p.Repeat, p.UserID, p.VesselType,
                                     p.Vendor, sizeof(p.Vendor), p.Callsign, sizeof(p.Callsign),
                                     p.Length, p.Beam, p.PosRefStbd, p.PosRefBow,
                                     p.MothershipID, p.AISInfo, p.SID));
  return p;
}

// [gpsd] type 24B, ordinary vessel: no mothership.
void test_type24B_regular_vessel_has_no_mothership() {
  ClassBStaticB p = parse129810(feed({"!AIVDM,1,1,,A,H42O55lti4hhhilD3nink000?050,0*40"}));
  TEST_ASSERT_EQUAL_UINT32(271041815, p.UserID);
  ASSERT_STR("TC6163", p.Callsign);
  TEST_ASSERT_EQUAL(60, p.VesselType);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 15.0, p.Length);
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 5.0, p.Beam);
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(N2kUInt32NA, p.MothershipID, "mothership must be NA, not own MMSI");
}

// [pyais-enc] type 24B, auxiliary craft 980696666 of mothership 666666666.
void test_type24B_auxiliary_craft_mothership() {
  ClassBStaticB p = parse129810(feed({"!AIVDO,1,1,,A,H>W@vFTe6??406t2??21J0Wg8Jb0,0*6F"}));
  TEST_ASSERT_EQUAL_UINT32(980696666, p.UserID);
  ASSERT_STR("BOOBAZ", p.Callsign);
  TEST_ASSERT_EQUAL(45, p.VesselType);
  TEST_ASSERT_EQUAL_UINT32(666666666, p.MothershipID);
  assertNA(p.Length, "aux craft: dimension bits carry the mothership, so length is NA");
  assertNA(p.Beam, "aux craft: beam is NA");
}

//*****************************************************************************
// Date helpers

void test_build_date_parsing() {
  TEST_ASSERT_EQUAL_INT32(0, AisDate::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT32(11017, AisDate::daysFromCivil(2000, 3, 1));
  TEST_ASSERT_EQUAL_INT32(AisDate::daysFromCivil(2026, 10, 27), AisDate::buildDateDays("Oct 27 2026"));
  TEST_ASSERT_EQUAL_INT32(AisDate::daysFromCivil(2027, 1, 5), AisDate::buildDateDays("Jan  5 2027"));
}

void test_next_occurrence_feb29() {
  // An ETA of 29 February goes to the next leap year.
  TEST_ASSERT_EQUAL_INT32(AisDate::daysFromCivil(2028, 2, 29),
                          AisDate::nextOccurrence((uint16_t)AisDate::daysFromCivil(2026, 10, 27), 2, 29));
  TEST_ASSERT_EQUAL_UINT16(N2kUInt16NA, AisDate::nextOccurrence(kBuildDay, 2, 30));
  TEST_ASSERT_EQUAL_UINT16(N2kUInt16NA, AisDate::nextOccurrence(kBuildDay, 13, 1));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_type1_available_fields);
  RUN_TEST(test_type1_rot_not_available);
  RUN_TEST(test_type3_all_not_available);
  RUN_TEST(test_type18_available_fields);
  RUN_TEST(test_type18_not_available);
  RUN_TEST(test_type19_available_fields_and_length);
  RUN_TEST(test_type19_not_available);
  RUN_TEST(test_type5_available_fields);
  RUN_TEST(test_type5_transceiver_info_channel_A);
  RUN_TEST(test_type5_transceiver_info_channel_B);
  RUN_TEST(test_type5_eta_next_occurrence_later_this_year_or_next);
  RUN_TEST(test_type5_eta_year_rollover);
  RUN_TEST(test_type5_eta_today_is_this_occurrence);
  RUN_TEST(test_type5_eta_day_after_is_next_year);
  RUN_TEST(test_type5_eta_date_na_without_today);
  RUN_TEST(test_type5_eta_date_na_with_date_before_build);
  RUN_TEST(test_type5_rejected_date_keeps_previous_today);
  RUN_TEST(test_type5_eta_and_draught_not_available);
  RUN_TEST(test_type14_safety_text);
  RUN_TEST(test_type24A_name);
  RUN_TEST(test_type24B_regular_vessel_has_no_mothership);
  RUN_TEST(test_type24B_auxiliary_craft_mothership);
  RUN_TEST(test_build_date_parsing);
  RUN_TEST(test_next_occurrence_feb29);
  return UNITY_END();
}
