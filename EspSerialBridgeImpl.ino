#include "EspSerialBridgeImpl.h"

bool wifiClientConnected = false;

EspSerialBridge::EspSerialBridge() {
}

EspSerialBridge::~EspSerialBridge() {
  m_WifiServer = NULL;
}

void EspSerialBridge::begin(uint16_t tcpPort) {
  readDeviceConfig();
  begin(m_Baud, m_SerialConfig, tcpPort);
}
  
void EspSerialBridge::begin(unsigned long baud, SerialConfig serialConfig, uint16_t tcpPort) {
  m_Baud = baud;
  m_SerialConfig = serialConfig;
  m_tcpPort = tcpPort;
  
  m_deviceConfigChanged = false;

  Serial.begin(m_Baud, m_SerialConfig);
  if (m_TxPin != 1)
    pins(15, 13);
  
  if (m_WifiServer.status() != CLOSED)
    m_WifiServer.stop();
    
  m_WifiServer = WiFiServer(tcpPort);
  m_WifiServer.begin();
  m_WifiServer.setNoDelay(true);
}

void EspSerialBridge::pins(uint8_t tx, uint8_t rx) {
  m_TxPin = tx;
  Serial.pins(tx, rx);
}

int EspSerialBridge::available() {
  return m_inPos;
}

void EspSerialBridge::loop() {
  // apply config changes
  if (m_deviceConfigChanged) {
    m_deviceConfigChanged = false;

    // disconnect client & cleanup buffer
    enableClientConnect(false);
    begin(m_Baud, m_SerialConfig, m_tcpPort);
    enableClientConnect();
    return;
  }
  
  // copy serial input to buffer
  while (m_enableReceive && Serial.available()) {
    int data = Serial.read();
    
    if (data >= 0) {
      m_buffer[m_inPos] = data;
      m_inPos++;
    } else
      break;
  }

  // output to network
  int dataSend;
  if ((dataSend = available()) > 0) {
      int socketSend = m_WifiClient.write(&m_buffer[0], dataSend);

#ifdef _DEBUG_TRAFFIC
      String msg = "send: " + String(dataSend) + " bytes";
      for (int i=0; i<dataSend; i++)
        msg += " " + String(m_buffer[i], HEX);
      espDebug.println(msg);
#endif
      // move buffer
      if (socketSend < dataSend) {
        memcpy(&m_buffer[0], &m_buffer[socketSend], (m_inPos - socketSend));
        m_inPos -= socketSend;
      } else
        m_inPos = 0;
  }

  // input from network
  int recv;
  while (m_enableClient && (recv = m_WifiClient.available()) > 0) {
    byte data[128];      
    int dataRead = m_WifiClient.read(data, (recv >= sizeof(data) ? sizeof(data) : recv));
    
    if (m_sessionDetection) {
      if (recv >= 2 && data[0] == telnetIAC && (data[1] == telnetDO || data[1] == telnetWILL)) {
        DBG_PRINTLN("telnet connection detected!");
        m_telnetSession.sessionState = telnetStateNormal;
      }
      enableSessionDetection(false);
    }
    
    for (int i=0; i<dataRead; i++)
      if (!telnetProtocolParse(data[i] & 0xff))
        Serial.write((data[i] & 0xff));

#ifdef _DEBUG_TRAFFIC
    String msg = "recv: " + String(dataRead) + " bytes";
    for (int i=0; i<dataRead; i++)
      msg += " " + String((byte)(data[i] & 0xff), HEX);
    espDebug.println(msg);
#endif
  }
  
  // probe new client
  if (m_WifiServer.hasClient()) {
    WiFiClient wifiClient = m_WifiServer.available();

    // discarding connection attempts if client is connected/not enabled
    if (!m_enableClient || m_WifiClient.status() != CLOSED) {
      wifiClient.stop();
    } else {
    // accept new connection
      m_WifiClient = wifiClient;
      m_WifiClient.setNoDelay(true);
      enableSessionDetection();
    }
  }
}

bool EspSerialBridge::telnetProtocolParse(uint8_t byte) {
  if (m_telnetSession.sessionState == telnetStateNone)
    return false;

  if (m_telnetSession.sessionState == telnetStateNormal)
    switch (byte) {
      case telnetIAC:
        m_telnetSession.sessionState = telnetStateIac;
        return true;
        break;
      default:
        return false;
        break;
    }

  if (m_telnetSession.sessionState == telnetStateIac)
    switch (byte) {
      case telnetIAC:
        m_telnetSession.sessionState = telnetStateNormal;
        return false;
        break;
      case telnetDO:
        m_telnetSession.sessionState = telnetStateDo;
        return true;
        break;
      case telnetWILL:
        m_telnetSession.sessionState = telnetStateWill;
        return true;
        break;
      case telnetSB:
        m_telnetSession.sessionState = telnetStateStart;
        return true;
        break;
      case telnetSE:
        m_telnetSession.sessionState = telnetStateNormal;
        return true;
        break;
      default:
#ifdef _DEBUG_TELNET_IAC
        DBG_PRINTF("iac: unknown %02x ", byte);
#endif
        m_telnetSession.sessionState = telnetStateNormal;
        return true;
        break;
    }

  if (m_telnetSession.sessionState == telnetStateWill) {
    uint8_t resp[3] = { telnetIAC, telnetDONT, byte };

    if (byte == telnetComPortOpt)
      resp[1] = telnetDO;
#ifdef _DEBUG_TELNET_WILL
    else
      DBG_PRINTF("telnet: will ignore %02x\n", byte);
#endif
    telnetResponse(resp, sizeof(resp));
    
    m_telnetSession.sessionState = telnetStateNormal;
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateDo) {
//    uint8_t resp[3] = { telnetIAC, telnetWONT, byte };
//
//    if (byte == telnetComPortOpt)
//      resp[1] = telnetDO;
//    else
//      DBG_PRINTF("do: ignore %02x ", byte);
//    telnetResponse(resp, sizeof(resp));
#ifdef _DEBUG_TELNET_DO
    DBG_PRINTF("telnet: do %02x", byte);
#endif
    m_telnetSession.sessionState = telnetStateNormal;
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateStart) {
    m_telnetSession.sessionState = (byte == telnetComPortOpt ? telnetStateComPort : telnetStateEnd);
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateEnd) {
    if (byte == telnetIAC)
      m_telnetSession.sessionState = telnetStateIac;
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateComPort) {
    switch (byte) {
      case telnetComPortSetBaud: 
        m_telnetSession.sessionState = telnetStateSetBaud; 
        m_telnetSession.baudCnt = 0; 
        m_telnetSession.baud = 0; 
        break;
      case telnetComPortSetDataSize:
        m_telnetSession.sessionState = telnetStateSetDataSize;
        break;
      case telnetComPortSetParity: 
        m_telnetSession.sessionState = telnetStateSetParity;
        break;
      case telnetComPortSetStopSize:
        m_telnetSession.sessionState = telnetStateSetStopSize;
        break;
      case telnetComPortSetControl:
        m_telnetSession.sessionState = telnetStateSetControl;
        break;
      case telnetComPortPurgeData: 
        m_telnetSession.sessionState = telnetStatePurgeData; 
        break;
      default:
        m_telnetSession.sessionState = telnetStateEnd; 
        break;
    }
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateSetControl) {
    switch (byte) {
      case telnetComPortControlCommandPurgeTX:
        break;
      case telnetComPortControlCommandBrkReq:
        break;
      case telnetComPortControlCommandBrkOn:
        break;
      case telnetComPortControlCommandBrkOff:
        break;
      case telnetComPortControlCommandDTROn:
        break;
      case telnetComPortControlCommandDTROff:
        break;
      case telnetComPortControlCommandRTSOn:
        break;
      case telnetComPortControlCommandRTSOff:
        break;
    }

    m_telnetSession.sessionState = telnetStateEnd; 
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateSetBaud) {
    m_telnetSession.baud |= byte;
    m_telnetSession.baudCnt++;

    if (m_telnetSession.baudCnt < 4) {
      m_telnetSession.baud <<= 8;
      return true;
    }

    if (m_telnetSession.baud == 0) {
        unsigned long baud = getBaud();
        uint8_t resp[10] = { telnetIAC, telnetSB, telnetComPortOpt, telnetComPortSetBaud,
         baud>>24, baud>>16, baud>>8, baud, telnetIAC, telnetSE };
        telnetResponse(resp, sizeof(resp));
    } else if (m_telnetSession.baud >= 300 && m_telnetSession.baud <= 115200) {
        EspDeviceConfig deviceConfig = getDeviceConfig();
        deviceConfig.setValue("baud", String(m_telnetSession.baud));
        if (deviceConfig.hasChanged()) {
          deviceConfig.saveToFile();
          readDeviceConfig();
      }
    }
    
    m_telnetSession.sessionState = telnetStateEnd; 
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateSetDataSize) {
    if (byte == 0) {
        uint8_t resp[7] = { telnetIAC, telnetSB, telnetComPortOpt, telnetComPortSetDataSize, 
          ((getSerialConfig() & UART_NB_BIT_MASK) >> 2) + 5, telnetIAC, telnetSE };
        telnetResponse(resp, sizeof(resp));
    } else if (byte >= 5 && byte <= 8) {
      uint8_t dataSize[4] = { UART_NB_BIT_5, UART_NB_BIT_6, UART_NB_BIT_7, UART_NB_BIT_8 };
      uint8_t serialConfig = (getSerialConfig() & ~UART_NB_BIT_MASK) & dataSize[byte - 5];

      EspDeviceConfig deviceConfig = getDeviceConfig();
      deviceConfig.setValue("dps", String(serialConfig));
      if (deviceConfig.hasChanged()) {
        deviceConfig.saveToFile();
        readDeviceConfig();
      }
    }

    m_telnetSession.sessionState = telnetStateEnd; 
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateSetParity) {
    if (byte == 0) {
      uint8_t resp[7] = { telnetIAC, telnetSB, telnetComPortOpt, telnetComPortSetParity,
        ((getSerialConfig() & UART_PARITY_MASK)) + 1, telnetIAC, telnetSE };
      telnetResponse(resp, sizeof(resp));
    } else if (byte >= 1 && byte <= 3) {
      uint8_t parity[3] = { UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD };
      uint8_t serialConfig = (getSerialConfig() & ~UART_PARITY_MASK) & parity[byte - 1];

      EspDeviceConfig deviceConfig = getDeviceConfig();
      deviceConfig.setValue("dps", String(serialConfig));
      if (deviceConfig.hasChanged()) {
        deviceConfig.saveToFile();
        readDeviceConfig();
      }
    }

    m_telnetSession.sessionState = telnetStateEnd; 
    return true;
  }

  if (m_telnetSession.sessionState == telnetStateSetStopSize) {
    if (byte == 0) {
      uint8_t stopBits;
      switch((getSerialConfig() & UART_NB_STOP_BIT_MASK)) {
        case UART_NB_STOP_BIT_1:
          stopBits = 1;
          break;
        case UART_NB_STOP_BIT_15:
          stopBits = 3;
          break;
        case UART_NB_STOP_BIT_2:
          stopBits = 2;
          break;
      }
      uint8_t resp[7] = { telnetIAC, telnetSB, telnetComPortOpt, telnetComPortSetStopSize, stopBits, telnetIAC, telnetSE };
      telnetResponse(resp, sizeof(resp));
    } else if (byte >= 1 && byte <= 3) {
      uint8_t stopBits[3] = { UART_NB_STOP_BIT_1, UART_NB_STOP_BIT_2, UART_NB_STOP_BIT_15 };
      uint8_t serialConfig = (getSerialConfig() & ~UART_NB_STOP_BIT_MASK) & stopBits[byte - 1];

      EspDeviceConfig deviceConfig = getDeviceConfig();
      deviceConfig.setValue("dps", String(serialConfig));
      if (deviceConfig.hasChanged()) {
        deviceConfig.saveToFile();
        readDeviceConfig();
      }
    }

    DBG_PRINTF("stopSize: %02x ", byte);
    
    m_telnetSession.sessionState = telnetStateEnd; 
    return true;
  }

  if (m_telnetSession.sessionState == telnetStatePurgeData) {
    if (byte == telnetComPortControlCommandPurgeTX)
      ;
    m_telnetSession.sessionState = telnetStateEnd; 
    return true;
  }

   DBG_PRINTF("\n\nunknown telnet: state %02x byte %02x\n", m_telnetSession.sessionState, byte);
   m_telnetSession.sessionState = telnetStateNormal;
}

void EspSerialBridge::telnetResponse(uint8_t *response, size_t responseSize) {
  int socketSend = m_WifiClient.write((unsigned char*)response, responseSize);    
#ifdef _DEBUG_TELNET_WILL_RESPONSE
  DBG_PRINT("telnetResponse: "); for (size_t i=0; i<responseSize; i++) DBG_PRINTF("%02x ", response[i]);
#endif
}

uint8_t EspSerialBridge::getTxPin() {
  return m_TxPin;  
}

unsigned long EspSerialBridge::getBaud() {
  return m_Baud;
}

SerialConfig EspSerialBridge::getSerialConfig() {
  return m_SerialConfig;
}

void EspSerialBridge::enableClientConnect(bool enable) {
  if (m_enableClient == enable)
    return;
    
  m_enableClient = enable;

  if (!enable && m_WifiClient.status() != CLOSED) {
    // send buffered data
    loop();
    m_WifiClient.stop();

    m_inPos = 0;
  }
}

void EspSerialBridge::readDeviceConfig() {
  EspDeviceConfig deviceConfig = getDeviceConfig();
  
  // baud
  String value = deviceConfig.getValue("baud");
  if (value != "") {
    int newVal = value.toInt();
    if (!m_deviceConfigChanged && m_Baud != newVal)
      m_deviceConfigChanged = true;
    m_Baud = newVal;
  }
    
  // tx-pin
  value = deviceConfig.getValue("tx");
  if (value != "") {
    int newVal = (value == "1" ? 1 : 15);
    if (!m_deviceConfigChanged && m_TxPin != newVal)
      m_deviceConfigChanged = true;
    m_TxPin = newVal;
  }

  // data/parity/stop
  value = deviceConfig.getValue("dps");
  if (value != "") {
    SerialConfig newVal = (SerialConfig)(value.toInt() & (UART_NB_BIT_MASK | UART_PARITY_MASK | UART_NB_STOP_BIT_MASK));
    if (!m_deviceConfigChanged && m_SerialConfig != newVal)
      m_deviceConfigChanged = true;
    m_SerialConfig = newVal;
  }
}

