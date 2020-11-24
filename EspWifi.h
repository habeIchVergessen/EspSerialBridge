#ifndef _ESP_WIFI_H
#define _ESP_WIFI_H

#if defined(ESP8266) || defined(ESP32)

#ifdef ESP32
  #define uint8  uint8_t
  #define uint16 uint16_t
  #define uint32 uint32_t
#endif

#define _OTA_NO_SPIFFS  // don't use SPIFFS to store temporary uploaded data

#include "Arduino.h"

#ifdef ESP8266
  #include "ESP8266WebServer.h"
  #include "ESP8266WiFi.h"
#endif

#ifdef ESP32
  #include <WiFi.h>
  #include "WebServer.h"
  #include "Update.h"
  #include <Preferences.h>
  #include <base64.h>
  
  Preferences preferences;

  #define PrefName "EspWifi"
  #define PrefSsid "ssid"
  #define PrefPwd  "pwd"
#endif

#include "WiFiClient.h"

#ifdef ESP8266
  #include "ESP8266mDNS.h"
#endif

#include "WiFiUdp.h"
#include "FS.h"
#include "detail/RequestHandler.h"
#include "detail/RequestHandlersImpl.h"

#include "EspConfig.h"

#ifdef ESP8266
  extern "C" {
    #include "user_interface.h"
  }
#endif

#include "EspDebug.h"

// prototype RequestHandler
class EspWiFiRequestHandler :  public RequestHandler {
  virtual bool canHandle(HTTPMethod method, String uri) { return false; };
  virtual bool canUpload(String uri) { return false; };
#ifdef ESP8266
  virtual bool handle(ESP8266WebServer& server, HTTPMethod method, String uri) { return false; };
  virtual void upload(ESP8266WebServer& server, String uri, HTTPUpload& upload) { };
#endif
#ifdef ESP32
  virtual bool handle(WebServer& server, HTTPMethod method, String uri) { return false; };
  virtual void upload(WebServer& server, String uri, HTTPUpload& upload) { };
#endif

protected:
#ifdef ESP8266
  virtual bool canHandle(ESP8266WebServer& server) { return false; };
#endif
#ifdef ESP32
  virtual bool canHandle(WebServer& server) { return false; };
#endif
  String getConfigUri() { return "/config"; };

  virtual String menuHtml() { return ""; };
  virtual uint8_t menuIdentifiers() { return 0; };
  virtual String menuIdentifiers(uint8_t identifier) { return ""; };

  bool mExternalRequestHandler = true;
  EspWiFiRequestHandler *mNextRequestHandler = NULL;

  bool isExternalRequestHandler() { return mExternalRequestHandler; };
  EspWiFiRequestHandler *getNextRequestHandler() { return mNextRequestHandler ; }
  void setNextRequestHandler(EspWiFiRequestHandler *nextRequestHandler) { mNextRequestHandler = nextRequestHandler; };

  friend class EspWiFi; 
};

class EspWiFi {
  public:
    typedef String (*DeviceListCallback) ();
#ifdef ESP8266
    typedef String (*DeviceConfigCallback) (ESP8266WebServer *server, uint16_t *result);
#endif
#ifdef ESP32
    typedef String (*DeviceConfigCallback) (WebServer *server, uint16_t *result);
#endif
    EspWiFi();
    static void setup();
    static void loop();
    void setupHttp(bool start=true);
    boolean sendMultiCast(String msg);
    static String getChipID();
    static String getHostname();
    static String getDefaultHostname();

#ifdef _ESP1WIRE_SUPPORT
    void registerDeviceConfigCallback(DeviceConfigCallback callback) { deviceConfigCallback = callback; };
    void registerDeviceListCallback(DeviceListCallback callback) { deviceListCallback = callback; };
    void registerScheduleConfigCallback(DeviceConfigCallback callback) { scheduleConfigCallback = callback; };
    void registerScheduleListCallback(DeviceListCallback callback) { scheduleListCallback = callback; };
#endif  // _ESP1WIRE_SUPPORT

    void registerExternalRequestHandler(EspWiFiRequestHandler *externalRequestHandler);

  protected:
    class EspWiFiRequestHandlerImpl :  public EspWiFiRequestHandler {
    public:
      EspWiFiRequestHandlerImpl() { mExternalRequestHandler = false; };

      bool canHandle(HTTPMethod method, String uri);
      bool canUpload(String uri);
      
    #ifdef ESP8266
      bool handle(ESP8266WebServer& server, HTTPMethod method, String uri);
      void upload(ESP8266WebServer& server, String uri, HTTPUpload& upload);
    #endif
    #ifdef ESP32
      bool handle(WebServer& server, HTTPMethod method, String uri);
      void upload(WebServer& server, String uri, HTTPUpload& upload);
    #endif
      
    } mEspWiFiRequestHandler;

    String getConfigUri() { return mEspWiFiRequestHandler.getConfigUri(); };
    String getDevListCssUri() { return "/static/deviceList.css"; };
    String getDevListJsUri() { return "/static/deviceList.js"; };
    String getOtaUri() { return "/ota/" + getChipID() + ".bin"; };
    void setHostname(String hostname);
    String otaFileName;
    File otaFile;
    bool lastWiFiStatus = false;
    
    IPAddress ipMulti = IPAddress(239, 0, 0, 57);
    unsigned int portMulti = 12345;      // local port to listen on
    
    bool netConfigChanged = false;
    
    WiFiUDP WiFiUdp;
#ifdef ESP8266
    ESP8266WebServer server;
#endif
#ifdef ESP32
    WebServer server;
#endif

    bool httpStarted = false;

#if defined(_ESP1WIRE_SUPPORT) || defined(_ESPSERIALBRIDGE_SUPPORT)
    DeviceConfigCallback deviceConfigCallback = NULL;
#endif
    
#ifdef _ESP1WIRE_SUPPORT
    DeviceListCallback deviceListCallback = NULL;
    DeviceConfigCallback scheduleConfigCallback = NULL;
    DeviceListCallback scheduleListCallback = NULL;
#endif  // _ESP1WIRE_SUPPORT

    // source: http://esp8266-re.foogod.com/wiki/SPI_Flash_Format
    typedef struct __attribute__((packed))
    {
      uint8   magic;
      uint8   unknown;
      uint8   flash_mode;
      uint8   flash_size_speed;
      uint32  entry_addr;
    } HeaderBootMode1;

    void setupInternal();
    void loopInternal();
    void setupWifi();
    void statusWifi(bool reconnect=false);
    void setupSoftAP();
    void configWifi();
    void reconfigWifi(String ssid, String password);
    void configNet();

#ifdef ESP32
    String base64Decode(String encoded);
#endif

    String ipString(IPAddress ip);
    void printUpdateError();

    void httpHandleRoot();
    void httpHandleConfig();

    void httpHandleDeviceListCss();
    void httpHandleDeviceListJss();
    void httpHandleNotFound();

#ifdef _ESP1WIRE_SUPPORT
    void httpHandleDevices();
    void httpHandleSchedules();
#endif

#ifndef _OTA_NO_SPIFFS
    bool initOtaFile(String filename, String mode);
    void clearOtaFile();
#endif

    void httpHandleOTA();
    void httpHandleOTAData();
};

extern EspWiFi espWiFi;
extern bool optionsChanged;
extern bool httpRequestProcessed;

String getChipID() { return EspWiFi::getChipID(); };
String getHostname() { return EspWiFi::getHostname(); };
String getDefaultHostname() { return EspWiFi::getDefaultHostname(); };

#endif  // ESP8266 || ESP32

#endif	// _ESP_WIFI_H
