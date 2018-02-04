#include "FlashATmega328Serial.h";

FlashATmega328::FlashATmega328(uint8_t dtrPort) {
  m_dtrPort = dtrPort;
  pinMode(m_dtrPort, OUTPUT);
  digitalWrite(m_dtrPort, HIGH);
}

FlashATmega328::~FlashATmega328() {
  pinMode(m_dtrPort, INPUT);

  if (m_BaudRateAtReset != -1 && Serial.baudRate() != m_BaudRateAtReset) {
    setupSerial(m_BaudRateAtReset);
    readData();
  }
}

void FlashATmega328::setupSerial(int baud) {
  Serial.begin(baud, SERIAL_8N1);
  Serial.pins(15, 13);

  yield();
  
  Serial.flush();
  readData();
}

bool FlashATmega328::reset() {
  digitalWrite(m_dtrPort, LOW);
  delay(1000);
  digitalWrite(m_dtrPort, HIGH);

  return true;
}

bool FlashATmega328::initFlash() {
  m_BaudRateAtReset = Serial.baudRate();

  setupSerial(57600);

  // send sync
  if (!stkGetSync())
    return false;
  DBG_PRINT("sync ");
  
  if (!stkEnableProgMode())
    return false;
  DBG_PRINT("prog ");

  return true;
}

bool FlashATmega328::finishFlash() {
  stkEnableProgMode(false);

  return true;
}

int FlashATmega328::readData(bool dump) {
  int result = 0, bytes = 0;

#ifdef _DEBUG
  if (dump)
    DBG_PRINTF("FlashATmega328: readData ");
#endif

  uint8_t data[8];
  while ((bytes = readData(data, sizeof(data))) > 0) {
#ifdef _DEBUG
    if (dump)
      for (int i=0; i<bytes; i++)
        DBG_PRINTF("%0x ", data[i]);
#endif
    result += bytes;
  }

#ifdef _DEBUG
  if (dump)
    DBG_PRINTF("\n");
#endif

  return result;
}

int FlashATmega328::readData(uint8_t *data, size_t dataSize) {
  int dataRead = -1;
  uint8_t pos = 0;

  while (Serial.available()) {
    dataRead = Serial.read();
    
    if (dataRead == -1)
      return (pos > 0 ? pos : -1);
    
    data[pos] = (uint8_t)(dataRead & 0xFF);
    pos++;
    if (pos == dataSize)
      break;
  }

  return (pos > 0 ? pos : -1);
}

int FlashATmega328::writeData(uint8_t *data, size_t dataSize) {
  int result = 0, writeData;

  for (int i=0; i<dataSize; i++) {
    if ((writeData = Serial.write(data[i])) != 1)
      break;
    result++;
  }
  Serial.flush();
  
  return result;
}

bool FlashATmega328::stkGetSync() {
  unsigned long currMillis = millis();
  uint8_t sync[2] = { stkRequestGetSync, stkRequestCrcEOP }, retries = 10;
  bool result = false;
  
  while (retries > 0 && !result) {
    // initialize serial line
    Serial.flush();
    readData();

    //
    int writeSync = writeData(sync, sizeof(sync));
    delay(50);

    uint8_t response[2] = { 0x00, 0x00 };
    uint8_t pos = stkPollResponse(response, sizeof(response), 2);
    
    if (pos < sizeof(response) || response[0] != stkResponseInSync || response[1] != stkResponseOk) {
      retries--;
      continue;
    }

    result = true;
  }

  return result;
}

bool FlashATmega328::stkGetParameter(uint8_t parameter, uint8_t *value) {
  // get parameter
  uint8_t cmd[3] = { stkRequestGetParameter, parameter, stkRequestCrcEOP };

  int wp = writeData(cmd, sizeof(cmd));
  delay(50);

  uint8_t response[3] = { 0x00, 0x00, 0x00 };
  uint8_t pos = stkPollResponse(response, sizeof(response));

  if (pos < sizeof(response) || response[0] != stkResponseInSync || response[2] != stkResponseOk)
    return false;

  *value = response[1];

  return true;
}

bool FlashATmega328::stkReadSignature(uint8_t signature[3]) {
  uint8_t cmd[2] = { stkRequestReadSignature, stkRequestCrcEOP };

  int writeSign = writeData(cmd, sizeof(cmd));
  delay(50);

  uint8_t response[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
  uint8_t pos = stkPollResponse(response, sizeof(response));

  if (pos < sizeof(response) || response[0] != stkResponseInSync || response[4] != stkResponseOk)
    return false;

  memcpy(signature, &response[1], 3);
  
  return true;
}

bool FlashATmega328::stkEnableProgMode(bool enable) {
  // program mode
  uint8_t cmd[2] = { (enable ? stkRequestEnterProgMode : stkRequestLeaveProgMode), stkRequestCrcEOP }, response[2], retries = 20;

  bool sendCmd = true;
  int wPr;

  while (retries > 0) {
    if (sendCmd) {
      wPr = writeData(cmd, sizeof(cmd));
      delay(50);
      sendCmd = false;
    }
    
    if (readData(response, 1) < 1) {
      delay(25);
      retries--;
      continue;
    }

    switch(response[0]) {
      case stkResponseNoSync:
        DBG_PRINT(" noSync ");
        if (stkGetSync() == -1)
          return false;
        sendCmd = true;
        retries--;
        continue;
        break;
      case stkResponseInSync:
        if (readData(&response[1], 1) < 1)
          return false;
        break;
      default:
        DBG_PRINTF("FlashATmega328: stkEnableProgMode %d unkonwn response[0] %0x ", (enable ? 1 : 0), response[0]);
        return false;
        break;
    }
    
    switch(response[1]) {
      case stkResponseOk:
        return true;
        break;
      case stkResponseNoDevice:
      case stkResponseFailed:
        return false;
        break;
      default:
        DBG_PRINTF("FlashATmega328: stkEnableProgMode %d unkonwn response[1] %0x ", (enable ? 1 : 0), response[1]);
        return false;
        break;
    }

    break;
  }

  return false;
}

bool FlashATmega328::stkLoadAddress(uint16_t address, bool mapTo16BitAddress) {
  // mapping 8-bit memory address to 16-bit device address
  uint16_t mAddress = address;

  if (mapTo16BitAddress) {
    if (address & 0x01)
      return false;

    mAddress >>= 1;
  }
  
  uint8_t cmd[4] = { stkRequestLoadAddress, (mAddress & 0xFF), (mAddress >> 8), stkRequestCrcEOP };
  
  int writeSign = writeData(cmd, sizeof(cmd));
  delay(50);

  uint8_t response[2] = { 0x00, 0x00 };
  uint8_t pos = stkPollResponse(response, sizeof(response));

  if (pos < sizeof(response) || response[0] != stkResponseInSync || response[1] != stkResponseOk)
    return false;

  return true;
}

bool FlashATmega328::stkProgPage(StkPageMemType pageMemType, uint8_t *data, uint8_t dataSize) {
  uint8_t cmd[5] = { stkRequestProgPage, 0x00, dataSize, pageMemType, stkRequestCrcEOP }, pos = 0;
  
  int writeProgPage = writeData(cmd, 4);
  while ((writeProgPage = writeData(&data[pos], dataSize - pos)) > 0)
    pos += writeProgPage;
  writeProgPage = writeData(&cmd[4], 1);
  delay(50);

  uint8_t response[2] = { 0x00, 0x00 };
  pos = stkPollResponse(response, sizeof(response));

  if (pos < sizeof(response) || response[0] != stkResponseInSync || response[1] != stkResponseOk)
    return false;

  return true;
}

bool FlashATmega328::stkReadPage(StkPageMemType pageMemType, uint8_t *data, uint8_t dataSize) {
  uint8_t cmd[5] = { stkRequestReadPage, 0x00, dataSize, pageMemType, stkRequestCrcEOP }, pos = 0;
  
  int cmdW = writeData(cmd, 5);
  delay(50);

  uint8_t response[2] = { 0x00, 0x00 };
  pos = stkPollResponse(response, 1);

  if (pos < 1 || response[0] != stkResponseInSync)
    return false;

  pos = stkPollResponse(data, dataSize, 10);
  if (pos < dataSize)
    return false;

  pos = stkPollResponse(&response[1], 1);
  if (pos < 1 || response[1] != stkResponseOk)
    return false;

  return true;
}

size_t FlashATmega328::stkPollResponse(uint8_t *response, size_t responseSize, uint8_t retries) {
  uint8_t pos = 0, cnt = retries;
  int rd;

  while (cnt > 0 && pos < responseSize) {
    if ((rd = readData(&response[pos], responseSize - pos)) > 0)
      pos += rd;
      
    if (pos < responseSize) {
      delay(25);
      cnt--;
    }
  }

  return pos;
}

void FlashATmega328::flashFile(File *input) {
  test();
}

void FlashATmega328::test() {
    DBG_PRINT("FlashATmega328:\nreset: ");
    if (reset()) {
      DBG_PRINT("ok\ninitFlash: ");
      bool init = initFlash();

      if (init) {
        DBG_PRINT("ok\n");
        uint8_t signature[3];
        if (stkReadSignature(signature))
          DBG_PRINTF("signature %02x %02x %02x ", signature[0], signature[1], signature[2]);
      
        uint8_t parameter;
        if (stkGetParameter(stkRequestSWMajor, &parameter))
          DBG_PRINTF("major %d ", parameter);
        if (stkGetParameter(stkRequestSWMinor, &parameter))
          DBG_PRINTF("minor %d ", parameter);

        DBG_PRINT("loadAddress 0x0000 ");
        if (stkLoadAddress(0x0000)) {
          DBG_PRINT("ok readPage ");
          DBG_FORCE_OUTPUT();
          uint8_t data[64];
          for (uint8_t p=0; p<2; p++) {
            if (p > 0)
              stkLoadAddress(p * sizeof(data));
            if (stkReadPage(stkPageMemTypeFlash, data, sizeof(data))) {
              if (p == 0)
                DBG_PRINT("ok\n")
              for (int i=0; i<sizeof(data); i++) {
                if (i % 16 == 0)
                  DBG_PRINTF(":10%04X00", (p*sizeof(data) + i));
                DBG_PRINTF("%02X", data[i]);
                if (i % 16 == 15)
                  DBG_PRINT("  \n");
              }
            } else
              DBG_PRINT("failed");
          }
        } else
          DBG_PRINT("failed ");
          
        DBG_PRINT("\nfinishFlash: ");
        finishFlash();
        DBG_PRINT("ok\n");
      }
    }
    DBG_PRINT("\n");
}

