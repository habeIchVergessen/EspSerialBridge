#ifndef _FLASHATMEGA328_H
#define _FLASHATMEGA328_H

#include <Arduino.h>
#include "EspDebug.h"

class FlashATmega328 {
  public:
    FlashATmega328(uint8_t dtrPort, uint8_t txPin=1);
    ~FlashATmega328();

    void flashFile(File *input);
    void test();
    
  protected:
    enum StkRequest : byte {
      stkRequestCrcEOP        = 0x20
    , stkRequestGetSync       = 0x30
    , stkRequestGetSignOn     = 0x31
    , stkRequestSetParameter  = 0x40
    , stkRequestGetParameter  = 0x41
    , stkRequestSetDevice     = 0x42
    , stkRequestSetDeviceExt  = 0x45
    , stkRequestEnterProgMode = 0x50
    , stkRequestLeaveProgMode = 0x51
    , stkRequestChipErase     = 0x52
    , stkRequestCheckAutonic  = 0x53
    , stkRequestLoadAddress   = 0x55
    , stkRequestProgFlash     = 0x60
    , stkRequestProgPage      = 0x64
    , stkRequestReadFlash     = 0x70
    , stkRequestReadPage      = 0x74
    , stkRequestReadSignature = 0x75
    , stkRequestHW            = 0x80
    , stkRequestSWMajor       = 0x81
    , stkRequestSWMinor       = 0x82
    };
    
    enum StkResponse : byte {
      stkResponseOk           = 0x10
    , stkResponseFailed       = 0x11
    , stkResponseUnknown      = 0x12
    , stkResponseNoDevice     = 0x13
    , stkResponseInSync       = 0x14
    , stkResponseNoSync       = 0x15
    };

    enum StkPageMemType : byte {
      stkPageMemTypeFlash     = 0x46
    , stkPageMemTypeEEPROM    = 0x45
    };
    
    bool reset();
    bool initFlash();
    bool finishFlash();

    int readData(bool dump=false);
    int readData(uint8_t *data, size_t dataSize);
    int writeData(uint8_t *data, size_t dataSize);
    void setupSerial(int baud);
    
  private:
    unsigned long m_resetMillis;
    uint8_t m_dtrPort;
    int m_BaudRateAtReset = -1;
    int m_SerialTxPin = 1;

    bool stkGetSync();
    bool stkGetParameter(uint8_t parameter, uint8_t *value);
    bool stkReadSignature(uint8_t signature[3]);
    bool stkEnableProgMode(bool enable=true);
    // mapping 8-bit memory address to 16-bit device address
    bool stkLoadAddress(uint16_t address, bool mapTo16BitAddress=true);
    bool stkProgPage(StkPageMemType pageMemType, uint8_t *data, uint8_t dataSize);
    bool stkReadPage(StkPageMemType pageMemType, uint8_t *data, uint8_t dataSize);
    size_t stkPollResponse(uint8_t *response, size_t responseSize, uint8_t retries=20);
};

#endif

