#ifndef _EspSerialBridgeImpl_H
#define _EspSerialBridgeImpl_H

#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>

#include "EspDebug.h"

class EspSerialBridge {
  public:
    EspSerialBridge();
    ~EspSerialBridge();

    void begin(unsigned long baud=9600, SerialConfig serialConfig=SERIAL_8N1, uint16_t tcpPort=23);
    void pins(uint8_t tx, uint8_t rx);
    void loop();

    uint8_t getTxPin();
    unsigned long getBaud();
    void setBaud(unsigned long baud);
    SerialConfig getSerialConfig();
    void setSerialConfig(SerialConfig serialConfig);

    void enableReceive(bool enable=true) { m_enableReceive = enable; };
    void enableClientConnect(bool enable=true);

  protected:
    enum telnetCharacter : byte {
      telnetIAC         = 255
    , telnetDONT        = 254  // negotiation
    , telnetDO          = 253  // negotiation
    , telnetWILL        = 251  // negotiation
    , telnetSB          = 250  // subnegotiation begin
    , telnetSE          = 240  // subnegotiation end
    , telnetComPortOpt  =  44  // COM port options
    , telnetSetBaud     =   1  // Set baud rate
    , telnetSetDataSize =   2  // Set data size
    , telnetSetParity   =   3  // Set parity
    , telnetSetControl  =   5  // Set control lines
    , telnetPurgeData   =  12  // Flush FIFO buffer(s)
    , telnetPurgeTX     =   2
    , telnetBrkReq      =   4  // request current BREAK state
    , telnetBrkOn       =   5  // set BREAK (TX-line to LOW)
    , telnetBrkOff      =   6  // reset BREAK
    , telnetDTROn       =   8  // used here to reset microcontroller
    , telnetDTROff      =   9
    , telnetRTSOn       =  11  // used here to signal ISP (in-system-programming) to uC
    , telnetRTSOff      =  12
    };
    
    int available();
    void enableSessionDetection(bool enable=true) { m_sessionDetection = enable; }
  
  private:
    static const unsigned int m_bufferSize=256;
    byte m_buffer[m_bufferSize];
    uint16_t m_inPos = 0;
    bool m_enableReceive = true;
    bool m_enableClient = true;
    bool m_sessionDetection = false;

    WiFiServer m_WifiServer = NULL;
    WiFiClient m_WifiClient;

    uint8_t m_TxPin = 1;
    unsigned long m_Baud = 9600;
    SerialConfig m_SerialConfig = SERIAL_8N1;
};

#endif
