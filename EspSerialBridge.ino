#include <Arduino.h>

#define _DEBUG
#ifdef _DEBUG
//  #define _DEBUG_TRAFFIC
//  #define _DEBUG_HEAP
#endif
//#define _DEBUG_HTTP

// wifi options
#define _ESP_WIFI_UDP_MULTICAST_DISABLED
#define _ESPSERIALBRIDGE_SUPPORT

#ifdef _ESPSERIALBRIDGE_SUPPORT
  #define _OTA_ATMEGA328_SERIAL
//  #define _TARGET_ESP_01  // no gpio15 for serial use
#endif

#include "EspConfig.h"
#include "EspDebug.h"
#include "EspSerialBridgeImpl.h"
#include "EspWifi.h"

#define PROGNAME "EspSerialBridge"
#define PROGVERS "0.2"
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
  espDebug.registerInputCallback(handleInputStream);
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
    case 'u':
      DBG_PRINTLN("uptime: " + uptime());
      printHeapFree();
      break;
    case 'v':
      DBG_PRINTF("[%s.%s] compiled at \n", String(PROGNAME).c_str(), String(PROGVERS).c_str(), String(PROGBUILD).c_str());
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
#ifndef _TARGET_ESP_01
    options += htmlOption(F("swapped"), F("swapped (15/13)"), tx_pin == 15);
#endif
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

