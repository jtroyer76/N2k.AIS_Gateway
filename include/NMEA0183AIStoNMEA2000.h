/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <functional>
#include <string.h>
#include <N2kMessages.h>
#include "ais_decoder.h"
#include "default_sentence_parser.h"
#include "AisDate.h"

const double pi = 3.1415926535897932384626433832795;
const double knToms = 1852.0 / 3600.0;
const double degToRad = pi / 180.0;
const double nmTom = 1.852 * 1000;

class MyAisDecoder : public AIS::AisDecoder
{
  public:
    using SendFn = std::function<void(const tN2kMsg &)>;
    using LogFn = std::function<void(const char *)>;

    // The converter builds N2k messages and hands them to send(); it never talks to the bus itself.
    // minValidDay: dates earlier than this are rejected by setDaysSince1970(); defaults to the
    // firmware build date.
    explicit MyAisDecoder(SendFn send, LogFn log = nullptr,
                          uint16_t minValidDay = (uint16_t)AisDate::buildDateDays(__DATE__))
      : _send(std::move(send)), _log(std::move(log)), _minValidDay(minValidDay)
    {}

    // Today's date (days since 1970) from PGN 126992 / 129029, used for the type 5 ETA year.
    // Dates before the firmware build are rejected: the old SeaTalk GPS reports 2007 (GPS week
    // rollover) through the SeaTalk -> N2k converter, and 1970 is the default before any time
    // message. A rejected date keeps the last accepted one. Returns true if accepted.
    bool setDaysSince1970(uint16_t days) {
      if (days == N2kUInt16NA || days < _minValidDay) return false;
      _today = days;
      return true;
    }

  private:
    SendFn _send;
    LogFn _log;
    uint16_t _minValidDay;
    uint16_t _today = N2kUInt16NA;  // no accepted date yet

    void send(const tN2kMsg &N2kMsg) { if (_send) _send(N2kMsg); }

  protected:

    // AIS "not available" values must become NA before any scaling: the NMEA2000 library writes
    // a scaled NA as "out of range", which receivers show as a number (SPEC 5.4.1).

    double decodeRot(int iRot) {
      //see https://gpsd.gitlab.io/gpsd/AIVDM.html#_type_5_static_and_voyage_related_data
      //and https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:supplementary_software:nmea2000
      if (iRot < -127 || iRot > 127) return N2kDoubleNA;  // -128 = not available
      double rot = 0;
      if (iRot == 127) rot = 10;
      else if (iRot == -127) rot = -10;
      else if (iRot == 0) rot = 0;
      else if ( 1 <= iRot && iRot <= 126 ) rot = iRot * iRot / 22.401289;
      else if ( iRot >= -126 && iRot <= -1) rot = iRot * iRot / -22.401289 ;
      //rot now in deg/minute
      rot = rot * degToRad / 60.0; //N"K expects rot in radian/s
      return rot;
    }
    double decodeCog(int iCog) {
      double cog = N2kDoubleNA;
      if (iCog >= 0 && iCog < 3600) {
        cog = iCog / 10.0 * degToRad;
      }
      return cog;
    }
    double decodeHeading(int iHeading) {
      double heading = N2kDoubleNA;
      if ( iHeading >= 0 && iHeading <= 359) {
        heading = iHeading * degToRad;
      }
      return heading;
    }
    double decodeSog(unsigned int uSog) {  // 1/10 kn; 1023 = not available
      return uSog == 1023 ? N2kDoubleNA : uSog * knToms / 10.0;
    }
    double decodeLon(long iLon) {  // 1/10000 min; 181 deg = not available
      return iLon == 181L * 600000 ? N2kDoubleNA : iLon / 600000.0;
    }
    double decodeLat(long iLat) {  // 1/10000 min; 91 deg = not available
      return iLat == 91L * 600000 ? N2kDoubleNA : iLat / 600000.0;
    }
    double decodeDraught(unsigned int uDraught) {  // 1/10 m; 0 = not available
      return uDraught == 0 ? N2kDoubleNA : uDraught / 10.0;
    }
    double decodeEtaTime(unsigned int uHour, unsigned int uMinute) {  // hour 24 / minute 60 = not available
      return (uHour >= 24 || uMinute >= 60) ? N2kDoubleNA : uHour * 3600.0 + uMinute * 60.0;
    }
    // ETA month/day carry no year: use the next occurrence relative to today.
    uint16_t decodeEtaDate(unsigned int uMonth, unsigned int uDay) {  // month/day 0 = not available
      if (_today == N2kUInt16NA || uMonth == 0 || uDay == 0) return N2kUInt16NA;
      return AisDate::nextOccurrence(_today, uMonth, uDay);
    }

    // Channel the message was received on, from the first sentence's channel field
    // (!AIVDM,count,num,seqid,channel,...). 'B' or '2' is channel B; anything else defaults to A.
    tN2kAISTransceiverInformation receptionChannel() const {
      const std::vector<AIS::StringRef> &lines = sentences();
      if (lines.empty()) return N2kaischannel_A_VDL_reception;
      const AIS::StringRef &line = lines.front();
      int field = 0;
      for (size_t i = 0; i + 1 < line.size(); i++) {
        if (line.data()[i] == ',' && ++field == 4) {
          char ch = line.data()[i + 1];
          return (ch == 'B' || ch == '2') ? N2kaischannel_B_VDL_reception : N2kaischannel_A_VDL_reception;
        }
      }
      return N2kaischannel_A_VDL_reception;
    }

    // Auxiliary craft associated with a parent ship have MMSI 98xxxxxxx.
    static bool isAuxiliaryCraft(unsigned int uMmsi) {
      return uMmsi >= 980000000 && uMmsi <= 989999999;
    }

    virtual void onType123(unsigned int _uMsgType, unsigned int _uMmsi, unsigned int _uNavstatus,
                           int _iRot, unsigned int _uSog, bool _bPosAccuracy,
                           long _iPosLon, long _iPosLat, int _iCog, int _iHeading, int _Repeat, bool _Raim,
                           unsigned int _timestamp, unsigned int _maneuver_i) override {

      //Serial.println("123");

      tN2kMsg N2kMsg;

      // PGN129038
      SetN2kAISClassAPosition(N2kMsg, _uMsgType, (tN2kAISRepeat)_Repeat, _uMmsi,
                              decodeLat(_iPosLat), decodeLon(_iPosLon),
                              _bPosAccuracy, _Raim, _timestamp,
                              decodeCog(_iCog), decodeSog(_uSog), receptionChannel(),
                              decodeHeading(_iHeading), decodeRot(_iRot), (tN2kAISNavStatus)_uNavstatus);

      send(N2kMsg);
    }

    virtual void onType411(unsigned int , unsigned int , unsigned int , unsigned int , unsigned int , unsigned int , unsigned int , unsigned int , bool , int , int ) override {
      //Serial.println("411");
    }

    virtual void onType5(unsigned int _uMsgType, unsigned int _uMmsi, unsigned int _uImo, const std::string &_strCallsign,
                         const std::string &_strName,
                         unsigned int _uType, unsigned int _uToBow, unsigned int _uToStern,
                         unsigned int _uToPort, unsigned int _uToStarboard, unsigned int _uFixType,
                         unsigned int _uEtaMonth, unsigned int _uEtaDay, unsigned int _uEtaHour,
                         unsigned int _uEtaMinute, unsigned int _uDraught,
                         const std::string &_strDestination, unsigned int _ais_version,
                         unsigned int _repeat, bool _dte) override {

      // Serial.println("5");

      tN2kMsg N2kMsg;
      char CS[8];
      char Name[21];
      char Dest[21];

      strncpy(CS, _strCallsign.c_str(), sizeof(CS) - 1);
      CS[7] = 0;
      for (int i = strlen(CS); i < 7; i++) CS[i] = 32;
      
      strncpy(Name, _strName.c_str(), sizeof(Name) - 1);
      Name[20] = 0;
      for (int i = strlen(Name); i < 20; i++) Name[i] = 32;
      
      strncpy(Dest, _strDestination.c_str(), sizeof(Dest) - 1);
      Dest[20] = 0;
      for (int i = strlen(Dest); i < 20; i++) Dest[i] = 32;


      // PGN129794
      SetN2kAISClassAStatic(N2kMsg, _uMsgType, (tN2kAISRepeat) _repeat, _uMmsi,
                            _uImo, CS, Name, _uType, _uToBow + _uToStern,
                            _uToPort + _uToStarboard, _uToStarboard, _uToBow,
                            decodeEtaDate(_uEtaMonth, _uEtaDay), decodeEtaTime(_uEtaHour, _uEtaMinute),
                            decodeDraught(_uDraught), Dest,
                            (tN2kAISVersion) _ais_version, (tN2kGNSStype) _uFixType,
                            (tN2kAISDTE) _dte, receptionChannel());

      send(N2kMsg);
    }

    virtual void onType9(unsigned int , unsigned int , bool , int , int , int , unsigned int ) override {
      //Serial.println("9");
    }

    virtual void onType14(unsigned int _repeat, unsigned int _uMmsi,
                          const std::string &_strText, int _iPayloadSizeBits) override {

      tN2kMsg N2kMsg;

      // PGN129802
      SetN2kAISSafetyRelatedBroadcastMsg(N2kMsg, 14, (tN2kAISRepeat)_repeat, _uMmsi,
                                         receptionChannel(), _strText.c_str());

      send(N2kMsg);
    }

    virtual void onType18(unsigned int _uMsgType, unsigned int _uMmsi, unsigned int _uSog, bool _bPosAccuracy,
                          long _iPosLon, long _iPosLat, int _iCog, int _iHeading, bool _raim, unsigned int _repeat,
                          bool _unit, bool _diplay, bool _dsc, bool _band, bool _msg22, bool _assigned,
                          unsigned int _timestamp, bool _state ) override {
      //Serial.println("18");

      tN2kMsg N2kMsg;

      // PGN129039
      SetN2kAISClassBPosition(N2kMsg, _uMsgType, (tN2kAISRepeat) _repeat, _uMmsi,
                              decodeLat(_iPosLat), decodeLon(_iPosLon), _bPosAccuracy, _raim,
                              _timestamp, decodeCog(_iCog), decodeSog(_uSog), receptionChannel(),
                              decodeHeading(_iHeading), (tN2kAISUnit) _unit,
                              _diplay, _dsc, _band, _msg22, (tN2kAISMode) _assigned, _state);

      send(N2kMsg);
    }

    virtual void onType19(unsigned int _uMmsi, unsigned int _uSog, bool _bPosAccuracy, int _iPosLon, int _iPosLat,
                          int _iCog, int _iHeading, const std::string &_strName, unsigned int _uType,
                          unsigned int _uToBow, unsigned int _uToStern, unsigned int _uToPort,
                          unsigned int _uToStarboard, unsigned int _timestamp, unsigned int _fixtype,
                          bool _dte, bool _assigned, unsigned int _repeat, bool _raim) override {

      //Serial.println("19");
      tN2kMsg N2kMsg;

      // PGN129040 - the library has no setter; layout follows canboat (54 bytes)

      char Name[21];
      strncpy(Name, _strName.c_str(), sizeof(Name) - 1);
      Name[20] = 0;
      for (int i = strlen(Name); i < 20; i++) Name[i] = 32;
      
      N2kMsg.SetPGN(129040UL);
      N2kMsg.Priority = 4;
      N2kMsg.AddByte((_repeat & 0x03) << 6 | (19 & 0x3f));
      N2kMsg.Add4ByteUInt(_uMmsi);
      N2kMsg.Add4ByteDouble(decodeLon(_iPosLon), 1e-07);
      N2kMsg.Add4ByteDouble(decodeLat(_iPosLat), 1e-07);
      N2kMsg.AddByte((_timestamp & 0x3f) << 2 | (_raim & 0x01) << 1 | (_bPosAccuracy & 0x01));
      N2kMsg.Add2ByteUDouble(decodeCog(_iCog), 1e-04);
      N2kMsg.Add2ByteUDouble(decodeSog(_uSog), 0.01);
      N2kMsg.AddByte(0xff); // Regional Application
      N2kMsg.AddByte(0xff); // Regional Application
      N2kMsg.AddByte(_uType );
      N2kMsg.Add2ByteUDouble(decodeHeading(_iHeading), 1e-04);
      N2kMsg.AddByte(_fixtype << 4);
      N2kMsg.Add2ByteDouble(_uToBow + _uToStern, 0.1);
      N2kMsg.Add2ByteDouble(_uToPort + _uToStarboard, 0.1);
      N2kMsg.Add2ByteDouble(_uToStarboard, 0.1);
      N2kMsg.Add2ByteDouble(_uToBow, 0.1);
      N2kMsg.AddStr(Name, 20);
      // DTE (1), AIS mode (1), spare (4), AIS transceiver information (5), reserved (5)
      tN2kAISTransceiverInformation info = receptionChannel();
      N2kMsg.AddByte((_dte & 0x01) | (_assigned & 0x01) << 1 | (info & 0x03) << 6);
      N2kMsg.AddByte(0xf8 | ((info >> 2) & 0x07));

      send(N2kMsg);
    }


    virtual void onType21(unsigned int , unsigned int , const std::string &, bool , int , int , unsigned int , unsigned int , unsigned int , unsigned int ) override {
      //Serial.println("21");
    }

    virtual void onType24A(unsigned int _uMsgType, unsigned int _repeat, unsigned int _uMmsi,
                           const std::string &_strName) override {
      //Serial.println("24A");

      tN2kMsg N2kMsg;

      char Name[21];
      strncpy(Name, _strName.c_str(), sizeof(Name) - 1);
      Name[20] = 0;
      for (int i = strlen(Name); i < 20; i++) Name[i] = 32;
      
      // PGN129809
      SetN2kAISClassBStaticPartA(N2kMsg, _uMsgType, (tN2kAISRepeat) _repeat, _uMmsi, Name, receptionChannel());

      send(N2kMsg);
    }

    virtual void onType24B(unsigned int _uMsgType, unsigned int _repeat, unsigned int _uMmsi,
                           const std::string &_strCallsign, unsigned int _uType,
                           unsigned int _uToBow, unsigned int _uToStern, unsigned int _uToPort,
                           unsigned int _uToStarboard, const std::string &_strVendor) override {

      // Serial.println("24B");


      tN2kMsg N2kMsg;
      char CS[8];
      char Vendor[8];

      strncpy(CS, _strCallsign.c_str(), sizeof(CS) - 1);
      CS[7] = 0;
      for (int i = strlen(CS); i < 7; i++) CS[i] = 32;
      
      strncpy(Vendor, _strVendor.c_str(), sizeof(Vendor) - 1);
      Vendor[7] = 0;
      for (int i = strlen(Vendor); i < 7; i++) Vendor[i] = 32;
      
      // For an auxiliary craft the 30 dimension bits carry the mothership MMSI instead
      // (bow 9 | stern 9 | port 6 | starboard 6); otherwise there is no mothership.
      double length = _uToBow + _uToStern, beam = _uToPort + _uToStarboard;
      double posRefStbd = _uToStarboard, posRefBow = _uToBow;
      uint32_t mothership = N2kUInt32NA;
      if (isAuxiliaryCraft(_uMmsi)) {
        mothership = (_uToBow << 21) | (_uToStern << 12) | (_uToPort << 6) | _uToStarboard;
        length = beam = posRefStbd = posRefBow = N2kDoubleNA;
      }

      // PGN129810
      SetN2kAISClassBStaticPartB(N2kMsg, _uMsgType, (tN2kAISRepeat)_repeat, _uMmsi,
                                 _uType, Vendor, CS, length, beam, posRefStbd, posRefBow,
                                 mothership, receptionChannel());

      send(N2kMsg);
    }

    virtual void onType27(unsigned int , unsigned int , unsigned int , bool , int , int , int ) override {
      //Serial.println("27");
    }

    virtual void onSentence(const AIS::StringRef &_Stc) override {
      //Serial.printf("Sentence: %s\n", _Stc);
    }

    virtual void onMessage(const AIS::StringRef &, const AIS::StringRef &, const AIS::StringRef &) override {}

    virtual void onNotDecoded(const AIS::StringRef &, int ) override {}

    virtual void onDecodeError(const AIS::StringRef &_strMessage, const std::string &_strError) override {
      logError(_strMessage, _strError);
    }

    virtual void onParseError(const AIS::StringRef &_strMessage, const std::string &_strError) override {
      logError(_strMessage, _strError);
    }

  private:
    void logError(const AIS::StringRef &_strMessage, const std::string &_strError) {
      if (!_log) return;
      std::string msg(_strMessage.data(), _strMessage.size());
      AIS::stripTrailingWhitespace(msg);
      std::string line = _strError + " [" + msg + "]";
      _log(line.c_str());
    }
};
