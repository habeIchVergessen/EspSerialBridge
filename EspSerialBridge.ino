#include <Arduino.h>

#define _DEBUG
#ifdef _DEBUG
//  #define _DEBUG_TRAFFIC
//  #define _DEBUG_HEAP
//  #define _DEBUG_WIFI_SETTINGS  // enable WiFi.setAutoConnect and .printDiag on debug console
//  #define _DEBUG_ESP            // enable ESP.reset on debug console
#endif
//#define _DEBUG_HTTP

// wifi options
#define _ESP_WIFI_UDP_MULTICAST_DISABLED
#define _ESPSERIALBRIDGE_SUPPORT

#ifdef _ESPSERIALBRIDGE_SUPPORT
//  #define _OTA_ATMEGA328_SERIAL // enable atmega328 OTA
//  #define _TARGET_ESP_01  // no gpio15 for serial use
#endif

#ifdef _OTA_ATMEGA328_SERIAL
  #include "IntelHexFormatParser.h"
  #include "FlashATMega328Serial.h"

  IntelHexFormatParser *intelHexFormatParser = NULL;
#endif

#include "EspConfig.h"
#include "EspDebug.h"
#include "EspSerialBridgeImpl.h"
#include "EspWifi.h"
#include "HelperHTML.h"

#define PROGNAME "EspSerialBridge"
#define PROGVERS "0.3"
#define PROGBUILD String(__DATE__) + " " + String(__TIME__)

bool httpRequestProcessed     = false;
bool optionsChanged           = false;

EspConfig espConfig(PROGNAME);
EspWiFi espWiFi;

EspSerialBridge espSerialBridge;
EspDebug espDebug;

// prototypes
// **********************************************
// * class EspSerialBridgeRequestHandler
// **********************************************
class EspSerialBridgeRequestHandler : public EspWiFiRequestHandler {
  public:
    bool canHandle(HTTPMethod method, String uri) override;
    bool canUpload(String uri);
#ifdef ESP8266
    bool handle(ESP8266WebServer& server, HTTPMethod method, String uri) override;
    void upload(ESP8266WebServer& server, String uri, HTTPUpload& upload);
#endif
#ifdef ESP32
    bool handle(WebServer& server, HTTPMethod method, String uri) override;
    void upload(WebServer& server, String uri, HTTPUpload& upload);
#endif

  protected:
#ifdef ESP8266
    bool canHandle(ESP8266WebServer& server) override;
#endif
#ifdef ESP32
    bool canHandle(WebServer& server) override;
#endif
    String menuHtml() override;
    uint8_t menuIdentifiers() override;
    String menuIdentifiers(uint8_t identifier) override;
    String menuIdentifierSerial() { return "serial"; };
    String menuIdentifierOtaAddon() { return "ota-addon"; };
        
    String getDevicesUri() { return "/devices"; };
    String getOtaAtMegaUri() { return "/ota/atmega328.bin"; };

    String handleDeviceList();
#ifdef ESP8266
    String handleDeviceConfig(ESP8266WebServer& server, uint16_t *resultCode);
#endif
#ifdef ESP32
    String handleDeviceConfig(WebServer& server, uint16_t *resultCode);
#endif

#ifdef _OTA_ATMEGA328_SERIAL
    void clearParser();
#ifdef ESP8266
    void httpHandleOTAatmega328(ESP8266WebServer& server);
    void httpHandleOTAatmega328Data(ESP8266WebServer& server);
#endif
#ifdef ESP32
    void httpHandleOTAatmega328(WebServer& server);
    void httpHandleOTAatmega328Data(WebServer& server);
#endif

    String otaFileName;
    File otaFile;

    bool initOtaFile(String filename, String mode);
    void clearOtaFile();
#endif  // _OTA_ATMEGA328_SERIAL

} espSerialBridgeRequestHandler;

void setup() {
  setupEspTools();

  espConfig.setup();
  espWiFi.setup();

  espWiFi.registerExternalRequestHandler(&espSerialBridgeRequestHandler);

  espSerialBridge.begin();

  espDebug.begin();
  espDebug.registerInputCallback(handleInputStream);
}

void loop(void) {
  // handle wifi
  espWiFi.loop();

  // tools
  loopEspTools();

  espSerialBridge.loop();

  // send debug data
  espDebug.loop();
}

// required EspWifi
String getDictionary() {
  return "";
}

// other
void printHeapFree() {
#ifdef _DEBUG_HEAP
  DBG_PRINTLN((String)F("heap: ") + (String)(ESP.getFreeHeap()));
#endif
}

void handleInput(char r, bool hasValue, unsigned long value, bool hasValue2, unsigned long value2) {
  switch (r) {
#ifdef _DEBUG_WIFI_SETTINGS
    case 'a':
      WiFi.setAutoConnect(true);
      break;
    case 'd':
      WiFi.printDiag(espDebug);
      break;
#endif
#ifdef _DEBUG_ESP
    case 'R':
      ESP.reset();
      break;
#endif
    case 'u':
      DBG_PRINTLN("uptime: " + uptime());
      printHeapFree();
      break;
    case 'v':
      DBG_PRINTF("[%s.%s] compiled at %s\n", String(PROGNAME).c_str(), String(PROGVERS).c_str(), String(PROGBUILD).c_str());
      break;
    case ' ':
    case '\n':
    case '\r':
      break;
    default:
      break;
    }
}

void handleInputStream(Stream *input) {
  if (input->available() <= 0)
    return;

  static long value, value2;
  bool hasValue, hasValue2;
  char r = input->read();

  // reset variables
  value = 0; hasValue = false;
  value2 = 0; hasValue2 = false;
  
  byte sign = 0;
  // char is a number
  if ((r >= '0' && r <= '9') || r == '-'){
    byte delays = 2;
    while ((r >= '0' && r <= '9') || r == ',' || r == '-') {
      if (r == '-') {
        sign = 1;
      } else {
        // check value separator
        if (r == ',') {
          if (!hasValue || hasValue2) {
            print_warning(2, "format");
            return;
          }
          
          hasValue2 = true;
          if (sign == 0) {
            value = value * -1;
            sign = 0;
          }
        } else {
          if (!hasValue || !hasValue2) {
            value = value * 10 + (r - '0');
            hasValue = true;
          } else {
            value2 = value2 * 10 + (r - '0');
            hasValue2 = true;
          }
        }
      }
            
      // wait a little bit for more input
      while (input->available() <= 0 && delays > 0) {
        delay(20);
        delays--;
      }

      // more input available
      if (delays == 0 && input->available() <= 0) {
        return;
      }

      r = input->read();
    }
  }

  // Vorzeichen
  if (sign == 1) {
    if (hasValue && !hasValue2)
      value = value * -1;
    if (hasValue && hasValue2)
      value2 = value2 * -1;
  }

  handleInput(r, hasValue, value, hasValue2, value2);
}

// helper
void print_config() {
  String blank = F(" ");
  
  DBG_PRINT(F("config:"));
  DBG_PRINTLN();
}

void print_warning(byte type, String msg) {
  return;
  DBG_PRINT(F("\nwarning: "));
  if (type == 1)
    DBG_PRINT(F("skipped incomplete command "));
  if (type == 2)
    DBG_PRINT(F("wrong parameter "));
  if (type == 3)
    DBG_PRINT(F("failed: "));
  DBG_PRINTLN(msg);
}


// **********************************************
// * class EspSerialBridgeRequestHandler
// **********************************************
bool EspSerialBridgeRequestHandler::canHandle(HTTPMethod method, String uri) {
  if (method == HTTP_POST && canUpload(uri))
    return true;

  return false;
}

bool EspSerialBridgeRequestHandler::canUpload(String uri) {
  return uri == getOtaAtMegaUri();
}

#ifdef ESP8266
bool EspSerialBridgeRequestHandler::handle(ESP8266WebServer& server, HTTPMethod method, String uri) {
#endif
#ifdef ESP32
bool EspSerialBridgeRequestHandler::handle(WebServer& server, HTTPMethod method, String uri) {
#endif

  if (method == HTTP_POST && uri == getConfigUri() && server.hasArg(menuIdentifierSerial())) {
    uint16_t resultCode;
    String html = handleDeviceConfig(server, &resultCode);

    if (html != "") {
      server.client().setNoDelay(true);
      server.send(resultCode, "text/plain", html);

      return (httpRequestProcessed = true);
    }
  }

#ifdef _OTA_ATMEGA328_SERIAL
  if (method == HTTP_POST && uri == getConfigUri() && server.hasArg(menuIdentifierOtaAddon())) {
    String action = getOtaAtMegaUri();
    String html = F("<h4>OTA-AddOn</h4>");
    html += htmlInput("file", "file", "", 0) + htmlNewLine();
  
    server.client().setNoDelay(true);
    server.send(200, "text/plain", htmlForm(html, action, "post", "submitForm", "multipart/form-data"));

    return (httpRequestProcessed = true);
  }

  if (method == HTTP_POST && uri == getOtaAtMegaUri()) {
    httpHandleOTAatmega328(server);
    return httpRequestProcessed;
  }
#endif  // _OTA_ATMEGA328_SERIAL
  
  return false;
}

#ifdef ESP8266
void EspSerialBridgeRequestHandler::upload(ESP8266WebServer& server, String uri, HTTPUpload& upload) {
#endif
#ifdef ESP32
void EspSerialBridgeRequestHandler::upload(WebServer& server, String uri, HTTPUpload& upload) {
#endif
#ifdef _OTA_ATMEGA328_SERIAL
  httpHandleOTAatmega328Data(server);
#endif  // _OTA_ATMEGA328_SERIAL
}
    
#ifdef ESP8266
bool EspSerialBridgeRequestHandler::canHandle(ESP8266WebServer& server) {
#endif
#ifdef ESP32
bool EspSerialBridgeRequestHandler::canHandle(WebServer& server) {
#endif
  if (canHandle(server.method(), server.uri()))
    return true;

  if (server.method() == HTTP_POST && server.uri() == getConfigUri()) {
    if (server.hasArg(menuIdentifierSerial()) && (server.arg(menuIdentifierSerial()) == "" || server.arg(menuIdentifierSerial()) == "config"))
      return true;
    if (server.hasArg(menuIdentifierOtaAddon()) && server.arg(menuIdentifierOtaAddon()) == "")
      return true;
  }

  return false;
}

String EspSerialBridgeRequestHandler::menuHtml() {
  return htmlMenuItem(menuIdentifierSerial(), "Serial");
}

uint8_t EspSerialBridgeRequestHandler::menuIdentifiers() {
  return 2;
}

String EspSerialBridgeRequestHandler::menuIdentifiers(uint8_t identifier) {
  switch(identifier) {
    case 0: return menuIdentifierSerial();break;
    case 1: return menuIdentifierOtaAddon();break;
  }

  return "";
}

#ifdef ESP8266
String EspSerialBridgeRequestHandler::handleDeviceConfig(ESP8266WebServer& server, uint16_t *resultCode) {
#endif
#ifdef ESP32
String EspSerialBridgeRequestHandler::handleDeviceConfig(WebServer& server, uint16_t *resultCode) {
#endif
  String result = "";
  String reqAction = server.arg(F("action"));
 
  if (reqAction != F("form") && reqAction != F("submit"))
    return result;

  if (reqAction == F("form")) {
    String action = F("/config?ChipID=");
    action += getChipID();
    action += F("&serial=config");
    action += F("&action=submit");

    String html = "", options = "";

    uint32_t baud = espSerialBridge.getBaud();
    html += htmlLabel(F("baud"), F("Baud: "));
    options = htmlOption(F("9600"), F("9600"), baud == 9600);
    options += htmlOption(F("19200"), F("19200"), baud == 19200);
    options += htmlOption(F("38400"), F("38400"), baud == 38400);
    options += htmlOption(F("57600"), F("57600"), baud == 57600);
    options += htmlOption(F("74880"), F("74880"), baud == 74880);
    options += htmlOption(F("115200"), F("115200"), baud == 115200);
    html += htmlSelect(F("baud"), options, "") + htmlNewLine();
    action += F("&baud=");

    SerialConfig curr = espSerialBridge.getSerialConfig();
    
    html += htmlLabel(F("data"), F("Data: "));
    options = htmlOption(String(UART_NB_BIT_8), F("8"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_8);
    options += htmlOption(String(UART_NB_BIT_7), F("7"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_7);
    options += htmlOption(String(UART_NB_BIT_6), F("6"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_6);
    options += htmlOption(String(UART_NB_BIT_5), F("5"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_5);
    html += htmlSelect(String(F("data")), options, "") + htmlNewLine();
    action += F("&data=");

    html += htmlLabel(F("parity"), F("Parity: "));
    options = htmlOption(String(UART_PARITY_NONE), F("None"), (curr & UART_PARITY_MASK) == UART_PARITY_NONE);
    options += htmlOption(String(UART_PARITY_EVEN), F("Even"), (curr & UART_PARITY_MASK) == UART_PARITY_EVEN);
    options += htmlOption(String(UART_PARITY_ODD), F("Odd"), (curr & UART_PARITY_MASK) == UART_PARITY_ODD);
    html += htmlSelect(String(F("parity")), options, "") + htmlNewLine();
    action += F("&parity=");

    html += htmlLabel(F("stop"), F("Stop: "));
    options = htmlOption(String(UART_NB_STOP_BIT_1), F("1"), (curr & UART_NB_STOP_BIT_MASK) == UART_NB_STOP_BIT_1);
    options += htmlOption(String(UART_NB_STOP_BIT_2), F("2"), (curr & UART_NB_STOP_BIT_MASK) == UART_NB_STOP_BIT_2);
    html += htmlSelect(String(F("stop")), options, "") + htmlNewLine();
    action += F("&stop=");

    uint8_t tx_pin = espSerialBridge.getTxPin();
    html += htmlLabel("pins", F("TX/RX: "));
    options = htmlOption(F("normal"), F("normal (1/3)"), tx_pin == 1);
#ifndef _TARGET_ESP_01
    options += htmlOption(F("swapped"), F("swapped (15/13)"), tx_pin == 15);
#endif
    html += htmlSelect(String(F("pins")), options, "") + htmlNewLine();
    action += F("&pins=");

#ifdef _OTA_ATMEGA328_SERIAL
    html = htmlFieldSet(html, htmlMenuItem(menuIdentifierOtaAddon(), "OTA"));
#else
    html = htmlFieldSet(html, F("Settings"));
#endif

    if (html != "") {
      *resultCode = 200;
      html = "<h4>Serial</h4>" + html;
      result = htmlForm(html, action, F("post"), F("configForm"), "", "");
    }
  }

  if (reqAction == F("submit")) {
    EspDeviceConfig deviceConfig = espSerialBridge.getDeviceConfig();
    
    deviceConfig.setValue("baud", server.arg("baud"));
    deviceConfig.setValue("tx", String(server.arg("pins") == "normal" ? 1 : 15));
    uint8_t dps = 0;
    dps |= (server.arg("data").toInt() & UART_NB_BIT_MASK);
    dps |= (server.arg("parity").toInt() & UART_PARITY_MASK);
    dps |= (server.arg("stop").toInt() & UART_NB_STOP_BIT_MASK);
    deviceConfig.setValue("dps", String(dps));

    if (deviceConfig.hasChanged()) {
      deviceConfig.saveToFile();
      espSerialBridge.readDeviceConfig();
    }
    
    *resultCode = 200;
    result = F("ok");
  }

  return result;
}

#ifdef _OTA_ATMEGA328_SERIAL

#ifdef ESP8266
void EspSerialBridgeRequestHandler::httpHandleOTAatmega328(ESP8266WebServer& server) {
#endif
#ifdef ESP32
void EspSerialBridgeRequestHandler::httpHandleOTAatmega328(WebServer& server) {
#endif
  String message = "\n\nhttpHandleOTAatmega328: ";
  bool doUpdate = false;
  
  if (SPIFFS.exists(otaFileName) && initOtaFile(otaFileName, "r")) {
    message += otaFile.name();
    message += + " (";
    message += otaFile.size();
    message += " Bytes) received!";
    doUpdate = true;
  } else
    message += "file doesn't exists (maybe wrong IntelHEX format parsed!)";

  DBG_PRINTLN(message);

  if (doUpdate) {
    DBG_PRINT("starting Update: ");
    DBG_FORCE_OUTPUT();

    uint8_t txPin = 1;
#ifdef _ESPSERIALBRIDGE_SUPPORT
    espSerialBridge.enableClientConnect(false);
    txPin = espSerialBridge.getTxPin();
#endif

    FlashATmega328 flashATmega328(2, txPin);

    flashATmega328.flashFile(&otaFile);

#ifdef _ESPSERIALBRIDGE_SUPPORT
    espSerialBridge.enableClientConnect();
#endif

    clearOtaFile();
  }

  server.client().setNoDelay(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "See Other");
  httpRequestProcessed = true;
}

#ifdef ESP8266
void EspSerialBridgeRequestHandler::httpHandleOTAatmega328Data(ESP8266WebServer& server) {
#endif
#ifdef ESP32
void EspSerialBridgeRequestHandler::httpHandleOTAatmega328Data(WebServer& server) {
#endif
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    DBG_PRINT("httpHandleOTAatmega328Data: " + upload.filename);
    DBG_FORCE_OUTPUT();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // first block with data
    if (upload.totalSize == 0) {
        initOtaFile("/ota/atmega328.bin", "w");
        intelHexFormatParser = new IntelHexFormatParser(&otaFile);
    }

    if (intelHexFormatParser == NULL)
      return;

    DBG_PRINT(".");
    if ((upload.totalSize % HTTP_UPLOAD_BUFLEN) == 20)
      DBG_PRINTLN("\n");

    if (!intelHexFormatParser->parse(upload.buf, upload.currentSize)) {
      DBG_PRINTLN("\nwriting file " + otaFileName + " failed!");
      DBG_FORCE_OUTPUT();

      clearParser();
      clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaFile) {
      bool uploadComplete = (otaFile.size() == intelHexFormatParser->sizeBinaryData() && intelHexFormatParser->isEOF());
      
      DBG_PRINTF("\nend: %s (%d Bytes)\n", otaFile.name(), otaFile.size());
      DBG_FORCE_OUTPUT();
      otaFile.close();
      
      clearParser();
      if (!uploadComplete)     
        clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    DBG_PRINTF("\naborted\n");
    DBG_FORCE_OUTPUT();

    clearParser();
    clearOtaFile();
  }
}

bool EspSerialBridgeRequestHandler::initOtaFile(String filename, String mode) {
  SPIFFS.begin();
  otaFile = SPIFFS.open(filename, mode.c_str());

  if (otaFile)
    otaFileName = filename;

  return otaFile;
}

void EspSerialBridgeRequestHandler::clearOtaFile() {
  if (otaFile)
    otaFile.close();
  if (SPIFFS.exists(otaFileName))
    SPIFFS.remove(otaFileName);
  otaFileName = "";
}

void EspSerialBridgeRequestHandler::clearParser() {
  if (intelHexFormatParser != NULL) {
    free(intelHexFormatParser);
    intelHexFormatParser = NULL;
  }
}

#endif  // _OTA_ATMEGA328_SERIAL

