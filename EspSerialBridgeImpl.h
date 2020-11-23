#ifndef _EspSerialBridgeImpl_H
#define _EspSerialBridgeImpl_H

#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>

#include "EspDebug.h"
#include "EspConfig.h"
#include "EspWifi.h"

//#define _ESPSERIALBRIDGE_TELNET_SUPPORT

#ifdef _ESPSERIALBRIDGE_TELNET_SUPPORT
  #define _DEBUG_TELNET_IAC
  //#define _DEBUG_TELNET_WILL
  //#define _DEBUG_TELNET_DO
  //#define _DEBUG_TELNET_RESPONSE
#endif

class EspSerialBridge {
  public:
    EspSerialBridge();
    ~EspSerialBridge();

    void begin(uint16_t tcpPort=23);
    void begin(unsigned long baud, SerialConfig serialConfig, uint16_t tcpPort);
    void pins(uint8_t tx, uint8_t rx);
    void loop();
    EspDeviceConfig getDeviceConfig() { return espConfig.getDeviceConfig("Serial"); };
    void readDeviceConfig();

    uint8_t getTxPin();
    unsigned long getBaud();
    SerialConfig getSerialConfig();

//    void enableReceive(bool enable=true) { m_enableReceive = enable; };
    void enableClientConnect(bool enable=true);

    void printDiag(Print& dest);

  protected:
    int available();

  private:
    static const unsigned int m_bufferSize=256;
    byte m_buffer[m_bufferSize];
    uint16_t m_inPos = 0;
//    bool m_enableReceive = true;
    bool m_enableClient = true;

    WiFiServer m_WifiServer = NULL;
    WiFiClient m_WifiClient;

    bool m_deviceConfigChanged = false;
    uint8_t m_TxPin = 1;
    unsigned long m_Baud = 9600;
    SerialConfig m_SerialConfig = SERIAL_8N1;
    uint16_t m_tcpPort;

#ifdef _ESPSERIALBRIDGE_TELNET_SUPPORT

    enum telnetCharacter : byte { // RFC854
      telnetIAC                           = 0xFF
    , telnetDONT                          = 0xFE  // negotiation
    , telnetDO                            = 0XFD  // negotiation
    , telnetWONT                          = 0xFC  // negotiation
    , telnetWILL                          = 0xFB  // negotiation
    , telnetSB                            = 0xFA  // subnegotiation begin
    , telnetGoAhead                       = 0xF9
    , telnetEraseLine                     = 0xF8
    , telnetEraseChar                     = 0xF7
    , telnetAreYouThere                   = 0xF6
    , telnetAbortOutput                   = 0xF5
    , telnetInterruptProc                 = 0xF4
    , telnetBreak                         = 0xF3
    , telnetDataMark                      = 0xF2
    , telnetNOP                           = 0xF1
    , telnetSE                            = 0xF0  // subnegotiation end
    , telnetEndOfRecord                   = 0xEF
    , telnetAbortProcess                  = 0xEE
    , telnetSuspendProcess                = 0xED
    , telnetEndOfFile                     = 0xEC
    , telnetComPortOpt                    = 0x2C  // COM port options
    };

    enum telnetComPortOptions : byte {  // RFC 2217
      telnetComPortSetBaud                = 0x01   // Set baud rate
    , telnetComPortSetDataSize            = 0x02   // Set data size
    , telnetComPortSetParity              = 0x03   // Set parity
    , telnetComPortSetStopSize            = 0x04   // Set stop size
    , telnetComPortSetControl             = 0x05   // Set control lines
    , telnetComPortPurgeData              = 0x0C   // Flush FIFO buffer(s)
    };

    enum telnetComPortControlCommands : byte {  // RFC 2217
      telnetComPortControlCommandPurgeTX  = 0x02
    , telnetComPortControlCommandBrkReq   = 0x04 // request current BREAK state
    , telnetComPortControlCommandBrkOn    = 0x05  // set BREAK (TX-line to LOW)
    , telnetComPortControlCommandBrkOff   = 0x06  // reset BREAK
    , telnetComPortControlCommandDTRReq   = 0x07  // used here to reset microcontroller
    , telnetComPortControlCommandDTROn    = 0x08  // used here to reset microcontroller
    , telnetComPortControlCommandDTROff   = 0x09
    , telnetComPortControlCommandRTSReq   = 0x0A  // used here to signal ISP (in-system-programming) to uC
    , telnetComPortControlCommandRTSOn    = 0x0B  // used here to signal ISP (in-system-programming) to uC
    , telnetComPortControlCommandRTSOff   = 0x0C
    };

    enum telnetState : byte {
      telnetStateNone                     = 0x00
    , telnetStateNormal                   = 0x01
    , telnetStateIac                      = 0x10
    , telnetStateWill                     = 0x11
    , telnetStateDo                       = 0x12
    , telnetStateStart                    = 0x13
    , telnetStateEnd                      = 0x14
    , telnetStateComPort                  = 0x20
    , telnetStateSetBaud                  = 0x21
    , telnetStateSetDataSize              = 0x22
    , telnetStateSetParity                = 0x23
    , telnetStateSetStopSize              = 0x24
    , telnetStateSetControl               = 0x25
    , telnetStatePurgeData                = 0x26
    };

    typedef struct __attribute__((packed)) TelnetSession {
      telnetState sessionState;
      uint8_t     baudCnt;
      uint32_t    baud;
    };

    void enableSessionDetection(bool enable=true); { m_sessionDetection = enable; if (enable) m_telnetSession.sessionState = telnetStateNone; }
    bool telnetProtocolParse(uint8_t byte);
    void telnetResponse(uint8_t *response, size_t responseSize);

    bool m_sessionDetection = false;
    TelnetSession m_telnetSession;
#endif  // _ESPSERIALBRIDGE_TELNET_SUPPORT
};

#endif
