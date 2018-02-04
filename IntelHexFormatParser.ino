#include "IntelHexFormatParser.h"

IntelHexFormatParser::IntelHexFormatParser() : m_Output(NULL) {
  m_EOF = false;  
}

IntelHexFormatParser::IntelHexFormatParser(File *output) : m_Output(output) {
  m_EOF = false;
}

bool IntelHexFormatParser::cancelProcessing() {
  m_EOF = true;

  // dump
  DBG_PRINTF("\nIntelHexFormatParser: cancelProcessing 0x%04x data:\n", m_parsePos);
  for (int i=0; i<m_bufferPos; i++) {
    if (
      (m_buffer[i] >= 0x30 && m_buffer[i] <= 0x3A) || // numbers (0x30 - 0x39) including recordMark 0x3A
      (m_buffer[i] >= 0x41 && m_buffer[i] <= 0x46) ||
      (m_buffer[i] >= 0x61 && m_buffer[i] <= 0x66)
    ) {
      DBG_PRINTF("%c", m_buffer[i]);
    } else
      DBG_PRINTF(" (%02x) ", m_buffer[i]);
  }
  DBG_PRINTF("\n");
    
  // close & delete file
  if (m_Output != NULL) {
    m_Output->close();
    
    if (SPIFFS.exists(m_Output->name()))
      SPIFFS.remove(m_Output->name());
  }
    
  return false;
}

bool IntelHexFormatParser::finishProcessing() {
  m_EOF = true;

  return true;
}

bool IntelHexFormatParser::parse(const uint8_t* data, size_t size) {
  size_t dataPos = 0;

  // parse done
  if (m_EOF)
    return false;

  // processing data
  while (dataPos < size) {
    if (m_parsePos > 0) // skip leading LF & CR (disable on start of data)
      while (dataPos < size && (data[dataPos] == 0x0D || data[dataPos] == 0x0A))
        dataPos++; 

    // copy header data to buffer
    if (m_bufferPos < sizeof(HeaderIntelHex)) {
      uint8_t copy = sizeof(HeaderIntelHex) - m_bufferPos;
  
      if ((size - dataPos) < copy)
        copy = (size - dataPos);
  
      memcpy(&m_buffer[m_bufferPos], &data[dataPos], copy);
      m_bufferPos += copy;
      dataPos += copy;
  
      // incomplete header
      if (m_bufferPos < sizeof(HeaderIntelHex))
        return true;
    }
  
    // check complete header
    HeaderIntelHex *header = (HeaderIntelHex*)&m_buffer[0];
  
    if (header->recordMark != 0x3A) {
      DBG_PRINTLN("parse: recordMark error!");
      return cancelProcessing();
    }
    
    uint8_t recordLength = convertByte(header->recordLength);
    uint16_t loadOffset = convertWord(header->loadOffset);
    uint8_t recordType = convertByte(header->recordType);
  
    if ((recordType != dataType && recordType != eofType) 
      || (loadOffset != m_parsePos && recordType == dataType) 
      || (loadOffset != 0x0000 && recordType == eofType)
      || (m_bufferSize < (recordLength + 1) * 2 + sizeof(HeaderIntelHex)))
    {
      DBG_PRINTLN("parse: header check failed!");
      return cancelProcessing();
    }

    // copy data & crc to buffer
    uint16_t dataCrcLength = (recordLength + 1) * 2;

    bool truncated = false;
    // copy missing data bytes to buffer
    uint8_t copy = dataCrcLength  - (m_bufferPos > sizeof(HeaderIntelHex) ? m_bufferPos - sizeof(HeaderIntelHex) : 0);
    if (dataPos + copy > size) {
      copy = size - dataPos;
      truncated = true;
    }
    memcpy(&m_buffer[m_bufferPos], &data[dataPos], copy);
    m_bufferPos += copy;
    dataPos += copy;

    if (truncated)
      return true;

    // crc header
    uint8_t crc = recordLength + (loadOffset >> 8) + (loadOffset & 0xFF) + recordType, dataByte;

    // crc data
    for (int i=0; i<recordLength; i++) {
      dataByte = convertByte(&m_buffer[sizeof(HeaderIntelHex) + i * 2]);
      crc += dataByte;
      if (m_Output != NULL)
        m_Output->write(dataByte);
    }

    // crc check
    dataByte = convertByte(&m_buffer[sizeof(HeaderIntelHex) + recordLength * 2]);
    if (dataByte != (((crc ^ 0xFF) + 1) & 0xFF) || ((crc + dataByte) & 0xFF) != 0x00) {
      DBG_PRINTF("crc %02x calc %02x check %02x\n", dataByte, (((crc ^ 0xFF) + 1) & 0xFF), ((crc + dataByte) & 0xFF));
      return cancelProcessing();
    }

    // EOF
    if (recordType == eofType)
      return finishProcessing();
  
    // increase offset & reset buffer
    m_parsePos += recordLength;
    m_bufferPos = 0;
  }
    
  return true;
}

uint8_t IntelHexFormatParser::convertChar(const uint8_t data) {
  uint8_t result = 0xFF;
  
  if (data >= 0x30 && data <= 0x39)
    result = (data - 0x30);
  if (data >= 0x41 && data <= 0x46)
    result = (data - 0x37);
  if (data >= 0x61 && data <= 0x66)
    result = (data - 0x57);

  return result;
}

uint8_t IntelHexFormatParser::convertByte(const uint8_t *data) {
  return (convertChar(data[0]) << 4) | convertChar(data[1]);
}

uint16_t IntelHexFormatParser::convertWord(const uint8_t *data) {
  return ((uint16_t)convertByte(&data[0]) << 8) | convertByte(&data[2]);
}

