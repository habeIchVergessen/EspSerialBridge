#include <Arduino.h>

#define _DEBUG
#ifdef _DEBUG
//  #define _DEBUG_TRAFFIC
#endif
//#define _DEBUG_HTTP

// wifi options
#define _OTA_NO_SPIFFS
#define _ESP_WIFI_UDP_MULTICAST_DISABLED
#define _ESPSERIALBRIDGE_SUPPORT

#ifdef _ESPSERIALBRIDGE_SUPPORT
  #define _OTA_ATMEGA328_SERIAL
#endif

#include "EspConfig.h"
#include "EspDebug.h"
#include "EspSerialBridgeImpl.h"
#include "EspWifi.h"

#define PROGNAME "EspSerialBridge"
#define PROGVERS "0.1b"
#define PROGBUILD String(__DATE__) + " " + String(__TIME__)

bool httpRequestProcessed     = false;

EspConfig espConfig(PROGNAME);

EspSerialBridge espSerialBridge;
EspDebug espDebug;

void setup() {
  setupEspTools();
  setupEspWifi();

  espSerialBridge.begin();

  registerDeviceConfigCallback(handleDeviceConfig);

  espDebug.begin();
}

void loop(void) {
  // handle wifi
  loopEspWifi();

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
    case ' ':
    case '\n':
    case '\r':
      break;
    default:
      break;
    }
}


String handleDeviceConfig(ESP8266WebServer *server, uint16_t *resultCode) {
  String result = "";
  String reqAction = server->arg(F("action"));
  
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
    html += htmlSelect(F("baud"), options) + htmlNewLine();
    action += F("&baud=");

    SerialConfig curr = espSerialBridge.getSerialConfig();
    
    html += htmlLabel(F("data"), F("Data: "));
    options = htmlOption(String(UART_NB_BIT_8), F("8"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_8);
    options += htmlOption(String(UART_NB_BIT_7), F("7"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_7);
    options += htmlOption(String(UART_NB_BIT_6), F("6"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_6);
    options += htmlOption(String(UART_NB_BIT_5), F("5"), (curr & UART_NB_BIT_MASK) == UART_NB_BIT_5);
    html += htmlSelect(F("data"), options) + htmlNewLine();
    action += F("&data=");

    html += htmlLabel(F("parity"), F("Parity: "));
    options = htmlOption(String(UART_PARITY_NONE), F("None"), (curr & UART_PARITY_MASK) == UART_PARITY_NONE);
    options += htmlOption(String(UART_PARITY_EVEN), F("Even"), (curr & UART_PARITY_MASK) == UART_PARITY_EVEN);
    options += htmlOption(String(UART_PARITY_ODD), F("Odd"), (curr & UART_PARITY_MASK) == UART_PARITY_ODD);
    html += htmlSelect(F("parity"), options) + htmlNewLine();
    action += F("&parity=");

    html += htmlLabel(F("stop"), F("Stop: "));
    options = htmlOption(String(UART_NB_STOP_BIT_1), F("1"), (curr & UART_NB_STOP_BIT_MASK) == UART_NB_STOP_BIT_1);
    options += htmlOption(String(UART_NB_STOP_BIT_2), F("2"), (curr & UART_NB_STOP_BIT_MASK) == UART_NB_STOP_BIT_2);
    html += htmlSelect(F("stop"), options) + htmlNewLine();
    action += F("&stop=");

    uint8_t tx_pin = espSerialBridge.getTxPin();
    html += htmlLabel(F("pins"), F("TX/RX: "));
    options = htmlOption(F("normal"), F("normal (1/3)"), tx_pin == 1);
    options += htmlOption(F("swapped"), F("swapped (15/13)"), tx_pin == 15);
    html += htmlSelect(F("pins"), options) + htmlNewLine();
    action += F("&pins=");

#ifdef _OTA_ATMEGA328_SERIAL
    html = htmlFieldSet(html, F("  <a id=\"ota-addon\" class=\"dc\">OTA</a>"));
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
    
    deviceConfig.setValue("baud", server->arg("baud"));
    deviceConfig.setValue("tx", String(server->arg("pins") == "normal" ? 1 : 15));
    uint8_t dps = 0;
    dps |= (server->arg("data").toInt() & UART_NB_BIT_MASK);
    dps |= (server->arg("parity").toInt() & UART_PARITY_MASK);
    dps |= (server->arg("stop").toInt() & UART_NB_STOP_BIT_MASK);
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

