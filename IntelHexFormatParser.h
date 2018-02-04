#ifndef _INTELHEXFORMATPARSER_H
#define _INTELHEXFORMATPARSER_H

#include <FS.h>

typedef struct __attribute__((packed))
{
  uint8_t   recordMark;
  uint8_t   recordLength[2];
  uint8_t   loadOffset[4];
  uint8_t   recordType[2];
} HeaderIntelHex;

class IntelHexFormatParser {
  public:
    IntelHexFormatParser();
    IntelHexFormatParser(File *output);

    bool parse(const uint8_t* data, size_t size);
    inline bool isEOF() { return m_EOF; }
    inline unsigned long sizeBinaryData() { return m_parsePos; }

  protected:
    const char recordMark = 0x3A;
    
    enum RecordType : byte {
      dataType                = 0x00
    , eofType                 = 0x01
    , extendedSegmentAddress  = 0x02
    , startSegmentAddress     = 0x03
    , extendedLinearAddress   = 0x04
    , startLinearAddress      = 0x05
    };
    
    static const uint8_t m_bufferSize = 64;
    uint8_t m_buffer[m_bufferSize];
    uint8_t m_bufferPos = 0;
    
    unsigned long m_parsePos = 0;
    bool m_EOF = false;

    File *m_Output;

    bool cancelProcessing();
    bool finishProcessing();

  private:
    uint8_t convertChar(const uint8_t data);
    uint8_t convertByte(const uint8_t *data);
    uint16_t convertWord(const uint8_t *data);
};

#endif
