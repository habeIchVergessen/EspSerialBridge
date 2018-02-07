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
      }
      enableSessionDetection(false);
    }

    for (int i=0; i<dataRead; i++)
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

