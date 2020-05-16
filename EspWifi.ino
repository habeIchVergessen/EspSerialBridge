#if defined(ESP8266) || defined(ESP32)

#include "EspWifi.h"

EspWiFi::EspWiFi() {
}

void EspWiFi::setup() {
  espWiFi.setupInternal();
}

void EspWiFi::setHostname(String hostname) {
#ifdef ESP8266
  WiFi.hostname(hostname);
#endif  
#ifdef ESP32
  WiFi.setHostname(hostname.c_str());
#endif  
}

void EspWiFi::setupInternal() {
  String hostname = espConfig.getValue("hostname");
  if (hostname != "")
    setHostname(hostname);

#ifdef ESP32
  // overwrite default hostname
  if (hostname == "") {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    setHostname(getDefaultHostname());
  }
#endif

  DBG_PRINT("Hostname: "); DBG_PRINTLN(getHostname());
  DBG_PRINT("MAC:      "); DBG_PRINTLN(WiFi.macAddress());

#ifdef ESP32
  // restore wifi connect data
  String ssid = "", password = "";
  
  if (preferences.begin(PrefName, true)) {
    ssid = preferences.getString(PrefSsid, "");
    password = base64Decode(preferences.getString(PrefPwd, ""));
    preferences.end();
  }  

  if (WiFi.SSID() == "" && ssid != "")
    WiFi.begin(ssid.c_str(), password.c_str());
#endif

  setupWifi();

  // init http server
  setupHttp();

#ifdef ESP32
  WiFiUdp.beginMulticast(ipMulti, portMulti);
#endif
}

void EspWiFi::loop() {
  espWiFi.loopInternal();
}

void EspWiFi::loopInternal() {
  // apply net config changes
  if (netConfigChanged) {
    // spend some time for http clients
    delay(2000);
    
    String hostname = espConfig.getValue("hostname");
    if (hostname != "")
      setHostname(hostname);
    setupWifi();
    
    yield();
    netConfigChanged = false;
  }
  
  statusWifi();

  unsigned int start = millis();
  // process http requests
  server.handleClient();
  if (httpRequestProcessed) {
    DBG_PRINTF("%d ms\n", (millis() - start));
    httpRequestProcessed = false;
  }
}

void EspWiFi::setupWifi() {
  DBG_PRINT("starting WiFi ");

  if (WiFi.getMode() != WIFI_STA)
    WiFi.mode(WIFI_STA);

  // static ip
  IPAddress ip, mask, gw, dns;
  if (ip.fromString(espConfig.getValue("address")) && ip != INADDR_NONE &&
      mask.fromString(espConfig.getValue("mask")) && mask != INADDR_NONE
  ) {
    if (!gw.fromString(espConfig.getValue("gateway")))
      gw = INADDR_NONE;
    if (!dns.fromString(espConfig.getValue("dns")))
      dns = INADDR_NONE;
    DBG_PRINTF("using static ip %s mask %s %s\n", ip.toString().c_str(), mask.toString().c_str(), String(WiFi.config(ip, gw, mask, dns) ? "ok" : "").c_str());
  } else
    DBG_PRINTF("using dhcp %s\n", String(WiFi.config(0U, 0U, 0U) ? "ok" : "").c_str());
    
  // force wait for connect
  lastWiFiStatus = !(WiFi.status() == WL_CONNECTED);
  statusWifi(true);
}

void EspWiFi::statusWifi(bool reconnect) {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected == lastWiFiStatus)
    return;
    
  if (!connected && reconnect) {
    byte retries = 20;
    while (retries > 0 && WiFi.status() != WL_CONNECTED) {
      retries--;
      delay(100);
    }
  }

  lastWiFiStatus = (WiFi.status() == WL_CONNECTED);
  if(lastWiFiStatus) {
    DBG_PRINT("Wifi connected: ");
    DBG_PRINT(WiFi.localIP());
    DBG_PRINT("/");
    DBG_PRINT(WiFi.subnetMask());
    DBG_PRINT(" ");
    DBG_PRINTLN(WiFi.gatewayIP());
    // trigger KVPUDP to reload config
    sendMultiCast("REFRESH CONFIG REQUEST");
  } else {
    DBG_PRINTLN("Wifi not connected");
  }

  setupSoftAP();
}

void EspWiFi::setupSoftAP() {
  bool run = (WiFi.status() != WL_CONNECTED);
  bool softAP = (WiFi.getMode() & WIFI_AP); //(ipString(WiFi.softAPIP()) != "0.0.0.0");

  if (run && !softAP) {
    DBG_PRINT("starting SoftAP: ");
  
    String apSsid = PROGNAME;
    apSsid += "@" + getChipID();
    WiFi.softAP(apSsid.c_str(), getChipID().c_str());
    
    DBG_PRINTLN("done (" + ipString(WiFi.softAPIP()) + ")");
  }

  if (!run && softAP) {
    DBG_PRINT("stopping SoftAP: ");
  
    WiFi.softAPdisconnect(true);
    
    DBG_PRINTLN("done");
  }
}

void EspWiFi::configWifi() {
  String ssid = server.arg("ssid");
  
  if (WiFi.SSID() != ssid && ssid == "") {
    WiFi.disconnect();  // clear ssid and psk in EEPROM
    delay(1000);
    statusWifi();
  }
  if (WiFi.SSID() != ssid && ssid != "") {
    reconfigWifi(ssid, server.arg("password"));
#ifdef ESP32
    // store wifi connect data
    if (preferences.begin(PrefName, false)) {
      preferences.putString(PrefSsid, ssid);
      preferences.putString(PrefPwd, base64::encode(server.arg("password")));
      preferences.end();
    }
#endif
  }

  httpRequestProcessed = true;
}

void EspWiFi::reconfigWifi(String ssid, String password) {
  if (ssid != "" && (WiFi.SSID() != ssid || WiFi.psk() != password)) {
    DBG_PRINT(" apply new config (" + ssid + " & " + password.length() + " bytes psk)");

    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect();
      delay(1000);
    }
    WiFi.begin(ssid.c_str(), password.c_str());
    statusWifi(true);
  }
}

void EspWiFi::configNet() {
  String hostname = server.arg("hostname"), defaultHostname = getDefaultHostname();
  if (hostname == "" || hostname == defaultHostname) {
  // reset to default
    espConfig.unsetValue("hostname");
    if (defaultHostname != getHostname())
      setHostname(defaultHostname);
  } else
    espConfig.setValue("hostname", hostname.c_str());
  
  IPAddress ip, mask, gw, dns;

  if (!ip.fromString(server.arg("address")) || !mask.fromString(server.arg("mask"))) {
    DBG_PRINT("configNet: clear config ip/mask invalid ");
    ip = mask = INADDR_NONE;
    espConfig.unsetValue("address");
    espConfig.unsetValue("mask");
    espConfig.unsetValue("gateway");
    espConfig.unsetValue("dns");
  } else {
    DBG_PRINT("configNet: apply config ");
    if (ip != INADDR_NONE) 
      espConfig.setValue("address", ip.toString());
    else
      espConfig.unsetValue("address");
    if (mask != INADDR_NONE)
      espConfig.setValue("mask", mask.toString());
    else
      espConfig.unsetValue("mask");
    if (gw.fromString(server.arg("gateway")))
      espConfig.setValue("gateway", gw.toString());
    else
      espConfig.unsetValue("gateway");
    if (dns.fromString(server.arg("dns")))
      espConfig.setValue("dns", dns.toString());
    else
      espConfig.unsetValue("dns");
  }
  
  if (espConfig.hasChanged()) {
    DBG_PRINT("saving ");
    espConfig.saveToFile();

    // apply next loop
    netConfigChanged = true;
  }

  DBG_PRINT("\n");
}

#ifdef ESP32
String EspWiFi::base64Decode(String encoded) {
  String result = "";

  if (encoded.length() > 0 && encoded.length() % 4 == 0) {
    size_t decodedLen = (float)encoded.length() * 0.75f; // * 6/8
    char decoded[decodedLen+1];

    uint32_t data = 0;
    for (int idx=0; idx<encoded.length(); idx++) {
      byte dataIdx = encoded.c_str()[idx];      
      // A-Z
      if (dataIdx >= 0x41 && dataIdx <= 0x5a)
        dataIdx -= 0x41;
      // a-z
      else if (dataIdx >= 0x61 && dataIdx <= 0x7a)
        dataIdx -= 0x61 - 0x1A;
      // 0-9
      else if (dataIdx >= 0x30 && dataIdx <= 0x39)
        dataIdx += 0x34 - 0x30;
      // +
      else if (dataIdx == 0x0b)
        dataIdx = 0x3E;
      // /
      else if (dataIdx == 0x47)
        dataIdx = 0x3F;
      else if (dataIdx == 0x3d)
        dataIdx = 0xFF;
      else
        return "";

      data <<= 6;
      if (dataIdx != 0xFF)
        data += dataIdx;

      if (idx % 4 == 3) {
        size_t didx = (idx - 3) * 0.75f;
        decoded[didx]   = (data & 0x00FF0000) >> 16;
        decoded[didx+1] = (data & 0x0000FF00) >> 8;
        decoded[didx+2] = (data & 0x000000FF);
        decoded[didx+3] = 0x00;
        data = 0;
      }
    }
    result = String(decoded);
  }
  
  return result;
}
#endif

void EspWiFi::setupHttp(bool start) {
//  if (MDNS.begin(WiFi.hostname().c_str()))
//    DBG_PRINTLN("MDNS responder started");

  if (!start) {
    if (httpStarted) {
      DBG_PRINT("stopping WebServer");
      server.stop();
      DBG_PRINTLN();
      httpStarted = false;
    }

    return;
  }

  if (httpStarted)
    return;
  
  DBG_PRINT("starting WebServer");
  server.addHandler(&mEspWiFiRequestHandler);
  server.onNotFound(std::bind(&EspWiFi::httpHandleNotFound, this));

#ifdef _ESP1WIRE_SUPPORT
  server.on("/devices", HTTP_GET, std::bind(&EspWiFi::httpHandleDevices, this));
  server.on("/schedules", HTTP_GET, std::bind(&EspWiFi::httpHandleSchedules, this));
#endif  // _ESP1WIRE_SUPPORT

  server.begin(80);
  httpStarted = true;

  DBG_PRINTLN();
}

bool EspWiFi::EspWiFiRequestHandlerImpl::canHandle(HTTPMethod method, String uri) {
  if (method == HTTP_GET && uri == "/")
    return true;
  if ((method == HTTP_GET || method == HTTP_POST) && uri == espWiFi.getConfigUri())
    return true;
  if (method == HTTP_GET && uri == espWiFi.getDevListCssUri())
    return true;
  if (method == HTTP_GET && uri == espWiFi.getDevListJsUri())
    return true;
  if (method == HTTP_POST && uri == espWiFi.getOtaUri())
    return true;

  return false;
}

bool EspWiFi::EspWiFiRequestHandlerImpl::canUpload(String uri) {
  if (uri == espWiFi.getOtaUri())
    return true;

  return false;
}

#ifdef ESP8266
bool EspWiFi::EspWiFiRequestHandlerImpl::handle(ESP8266WebServer& server, HTTPMethod method, String uri) {
#endif
#ifdef ESP32
bool EspWiFi::EspWiFiRequestHandlerImpl::handle(WebServer& server, HTTPMethod method, String uri) {
  void upload(WebServer& server, String uri, HTTPUpload& upload);
#endif
  if (method == HTTP_GET && uri == "/") {
    espWiFi.httpHandleRoot();
    return true;
  }
  if ((method == HTTP_GET || method == HTTP_POST) && uri == espWiFi.getConfigUri()) {
    espWiFi.httpHandleConfig();
    return true;
  }
  if (method == HTTP_GET && uri == espWiFi.getDevListCssUri()) {
    espWiFi.httpHandleDeviceListCss();
    return true;
  }
  if (method == HTTP_GET && uri == espWiFi.getDevListJsUri()) {
    espWiFi.httpHandleDeviceListJss();
    return true;
  }
  if (method == HTTP_POST && uri == espWiFi.getOtaUri()) {
    espWiFi.httpHandleOTA();
    return true;
  }

  return false;
}

#ifdef ESP8266
void EspWiFi::EspWiFiRequestHandlerImpl::upload(ESP8266WebServer& server, String uri, HTTPUpload& upload) {
#endif
#ifdef ESP32
void EspWiFi::EspWiFiRequestHandlerImpl::upload(WebServer& server, String uri, HTTPUpload& upload) {
#endif
  if (server.method() == HTTP_POST && uri == espWiFi.getOtaUri())
    espWiFi.httpHandleOTAData();
}

void EspWiFi::httpHandleRoot() {
  DBG_PRINT("httpHandleRoot: ");
  // menu
  String message = F("<div class=\"menu\">");
//#ifdef _ESPSERIALBRIDGE_SUPPORT
//  message += F("<a id=\"serial\" class=\"dc\">Serial</a>");
//#endif  // _ESPSERIALBRIDGE_SUPPORT
//#ifdef _ESP1WIRE_SUPPORT
//  message += F("<a href=\"/devices\" class=\"dc\">Devices</a><a href=\"/schedules\" class=\"dc\">Schedules</a>");
//#endif  // _ESP1WIRE_SUPPORT
    // for all external EspWiFiRequestHandler check menu
    EspWiFiRequestHandler *reqH = &mEspWiFiRequestHandler;
    String extMenu;

    while (reqH != NULL) {
      if (reqH->isExternalRequestHandler() && (extMenu = reqH->menuHtml()) != "")
        message += extMenu;
      reqH = reqH->getNextRequestHandler();
    }

  message += "<a id=\"ota\" class=\"dc\">OTA</a>";
  message += "<sp>" + uptime() + "</sp></div>";

  // wifi
  String netConfig = "<a id=\"net\" class=\"dc\">...</a>";
  String html = "<table><tr><td>ssid:</td><td>" + WiFi.SSID() + "</td><td><a id=\"wifi\" class=\"dc\">...</a></td></tr>";
  if (WiFi.status() == WL_CONNECTED) {
    html += F("<tr><td>Status:</td><td>connected</td><td></td></tr>");
    html += F("<tr><td>Hostname:</td><td>"); html += getHostname(); html += F(" (MAC: "); html += WiFi.macAddress();html += F(")</td><td rowspan=\"2\">");
    html += netConfig; html += F("</td></tr>");
    html += F("<tr><td>IP:</td><td>"); html += ipString(WiFi.localIP()); html += F("/"); html +=ipString(WiFi.subnetMask()); html += F(" "); html += ipString(WiFi.gatewayIP()); html += F("</td></tr>");
  } else {
    html += F("<tr><td>Status:</td><td>disconnected (MAC: ");
    html += WiFi.macAddress();
    html += F(")</td><td>"); html +=netConfig; html += F("</td></tr>");
  }
  html += F("</table>");
  message += htmlFieldSet(html, "WiFi");

  server.client().setNoDelay(true);
  server.send(200, "text/html", htmlBody(message));
  httpRequestProcessed = true;
}

void EspWiFi::httpHandleConfig() {
  DBG_PRINT("httpHandleConfig: ");
  String message = "", separator = "";

  if (server.method() == HTTP_GET) {
    for (uint8_t i=0; i<server.args(); i++) {
      if (message.length() > 0)
        separator = ",";
        
      if (server.argName(i) == "Version") {
          message += separator + "Version:";
          message += PROGNAME;
          message += ".";
          message += PROGVERS;
      }
      if (server.argName(i) == "ChipID") {
          message += separator + "ChipID:" + getChipID();
      }
      if (server.argName(i) == "Dictionary") {
          message += separator + "Dictionary:" + getDictionary();
      }
    }
  }
  
  if (server.method() == HTTP_POST) {
    // check required parameter ChipID
    if (server.args() == 0 || server.argName(0) != "ChipID" || server.arg(0) != getChipID()) {
      server.client().setNoDelay(true);
      server.send(403, "text/plain", "Forbidden");
      httpRequestProcessed = true;
      return;
    }

#ifdef _DEBUG_HTTP
    for (int i=0; i<server.args();i++) {
      DBG_PRINTLN("#" + String(i) + " '" + server.argName(i) + "' = '" + server.arg(i) + "'");
    }
#endif

#ifdef _ESP1WIRE_SUPPORT
    if (server.arg("deviceID") != "") {
      String result = "";
      uint16_t resultCode;
      if (deviceConfigCallback != NULL && (result = deviceConfigCallback(&server, &resultCode))) {
        server.client().setNoDelay(true);
        server.send(resultCode, "text/html", result);
        httpRequestProcessed = true;
        return;
      }

      server.client().setNoDelay(true);
      server.send(403, "text/plain", "Forbidden");
      httpRequestProcessed = true;
      return;
    }
    
    if (server.hasArg("schedule") && server.arg("schedule") != "") {
      uint16_t resultCode = 0;
      String result;
      
      if (scheduleConfigCallback != NULL)
        result = scheduleConfigCallback(&server, &resultCode);

      DBG_PRINT("resultCode: " + String(resultCode) + " ");
      switch (resultCode) {
        case 200:
          server.client().setNoDelay(true);
          server.send(resultCode, "text/html", result);
          break;
        case 303:
          server.client().setNoDelay(true);
          server.sendHeader("Location", "/schedules");
          server.send(303, "text/plain", "See Other");
          break;
        default:
          server.client().setNoDelay(true);
          server.send(403, "text/plain", "Forbidden");
          break;
      }

      httpRequestProcessed = true;
      return;
    }

    if (server.hasArg("resetSearch") && server.arg("resetSearch")== "") {
      esp1wire.resetSearch();
      server.client().setNoDelay(true);
      server.send(200, "text/html", "ok");
      httpRequestProcessed = true;
      return;
    }
#endif

    if (server.hasArg("ota") && server.arg("ota") == "") {
      String result = F("<h4>OTA</h4>");
      result += flashForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }

    if (server.hasArg("wifi") && server.arg("wifi") == "") {
      String result = F("<h4>WiFi</h4>");
      result += wifiForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
    if (server.arg("wifi") == "submit") {
      configWifi();
      server.client().setNoDelay(true);
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "See Other");
      return;
    }

    if (server.hasArg("net") && server.arg("net")== "") {
      String result = F("<h4>Network</h4>");
      result += netForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
    if (server.arg("net") == "submit") {
      configNet();
      server.client().setNoDelay(true);
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "See Other");
      return;
    }

    // for all external EspWiFiRequestHandler check config
    EspWiFiRequestHandler *reqH = &mEspWiFiRequestHandler;
    while (reqH != NULL) {
      if (reqH->isExternalRequestHandler() && reqH->canHandle(server) && (httpRequestProcessed = reqH->handle(server, server.method(), server.uri())))
        return;
      reqH = reqH->getNextRequestHandler();
    }

    if (server.hasArg("options") && server.arg("options") == "") {
      String result = F("<h4>Options</h4>");
      result += optionForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
    if (server.hasArg("options") && server.arg("options") == "submit") {
      for (uint8_t i=1; i<server.args(); i++) {
        if (server.argName(i) != "options")
          espConfig.setValue(server.argName(i), server.arg(i));
      }

      if (espConfig.hasChanged()) {
        DBG_PRINT("saving ");
        espConfig.saveToFile();
        optionsChanged = true;
      }
      
      server.client().setNoDelay(true);
      server.send(200, "text/html", "");
      httpRequestProcessed = true;
      return;
    }
    
    for (uint8_t i=1; i<server.args(); i++) {
      int value, value2;
      bool hasValue = false, hasValue2 = false;

      char r = server.argName(i)[0];
      String values = server.arg(i);

      hasValue=(values.length() > 0);
      if (hasValue) {
        int idx=values.indexOf(',', 0);
        value=atoi(values.substring(0, (idx == -1 ? values.length() : idx)).c_str());

        hasValue2=(idx > 0);
        if (hasValue2)
          value2=atoi(values.substring(idx + 1).c_str());
      }
      handleInput(r, hasValue, value, hasValue2, value2);
    }
  }
  
  server.client().setNoDelay(true);
  server.send(200, "text/plain", message);
  httpRequestProcessed = true;
}

#ifdef _ESP1WIRE_SUPPORT
void EspWiFi::httpHandleDevices() {
  DBG_PRINT("httpHandleDevices: ");
  String message = "", devList = "";

  if (server.method() == HTTP_GET) {
    if (deviceListCallback != NULL && (devList = deviceListCallback()) != "") {
#ifdef _DEBUG_TIMING
      unsigned long sendStart = micros();
#endif
      message = F("<div class=\"menu\"><a id=\"resetSearch\" class=\"dc\">resetSearch</a><sp><a id=\"back\" class=\"dc\">Back</a></sp></div>");
      String html = F("<table id=\"devices\"><thead><tr><th>Name</th><th>Type</th></tr></thead>");
      html += devList;
      html += F("</table>");
      String options = htmlOption("255", F("All"), true);
      options += htmlOption("18", F("Battery"));
      options += htmlOption("8", F("Counter"));
      options += htmlOption("4", F("Switch"));
      options += htmlOption("2", F("Temperature"));
      String legend = htmlSelect(F("filter"), options, F("javascript:filter();"));
      legend += F(" Devices");
      message += htmlFieldSet(html, legend);

      server.client().setNoDelay(true);
      server.send(200, "text/html", htmlBody(message));

#ifdef _DEBUG_TIMING
      DBG_PRINT("send " + elapTime(sendStart) + " ");
#endif
      httpRequestProcessed = true;
      return;
    }
  }
  
  server.client().setNoDelay(true);
  server.send(403, "text/plain", "Forbidden");
  httpRequestProcessed = true;
}

void EspWiFi::httpHandleSchedules() {
  DBG_PRINT("httpHandleSchedules: ");
  String message = "", schedList = "";

  if (server.method() == HTTP_GET) {
    if (scheduleListCallback != NULL && (schedList = scheduleListCallback()) != "") {
#ifdef _DEBUG_TIMING
      unsigned long sendStart = micros();
#endif
      // menu
      message = F("<div class=\"menu\"><a class=\"dc\" id=\"schedule#add\">Add</a><a id=\"back\" class=\"dc\" style=\"float: right;\">Back</a></div>");
      // schedules
      message += htmlFieldSet(schedList, "Schedules");

      server.client().setNoDelay(true);
      server.send(200, "text/html", htmlBody(message));

#ifdef _DEBUG_TIMING
      DBG_PRINT("send " + elapTime(sendStart) + " ");
#endif
      httpRequestProcessed = true;
      return;
    }
  }
  
  server.client().setNoDelay(true);
  server.send(403, "text/plain", "Forbidden");
  httpRequestProcessed = true;
}
#endif  // _ESP1WIRE_SUPPORT

void EspWiFi::httpHandleDeviceListCss() {
  DBG_PRINT("httpHandleDeviceListCss: ");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.client().setNoDelay(true);
  String css = F(".menu{height:1.3em;padding:5px 5px 5px 5px;margin-bottom:5px;background-color:#E0E0E0;}.menu .dc{float:left;}.menu sp{float: right;}label{width:4em;text-align:left;display:inline-block;} input[type=text]{margin-bottom:2px;} table td{padding:5px 15px 0px 0px;} table th{text-align:left;} fieldset{margin:0px 10px 10px 10px;max-height:20em;overflow:auto;} legend select{margin-right:5px;}");
  css += F(".dc{border:1px solid #A0A0A0;border-radius:5px;padding:0px 3px 0px 3px;}a.dc{color:black;text-decoration:none;margin-right:3px;outline-style:none;}.dc:hover{border:1px solid #5F5F5F;background-color:#D0D0D0;cursor:pointer;} #mD{background:rgba(0,0,0,0.5);visibility:hidden;position:absolute;top:0;left:0;width:100%;height:100%;z-index:1;} #mDC{border:1px solid #A0A0A0;border-radius:5px;background:rgba(255,255,255,1);margin-top:10%;display:inline-block;} #mDCC {margin-top: 0px;} #mDCC label{margin-left:10px;width:8em;text-align:left;display:inline-block;} #mDCC select,input{margin:5px 10px 0px 10px;width:13em;display:inline-block;} #mDCC table td input{margin:0px 0px 0px 0px;width:0.8em;} #mDCC table th {font-size:smaller} #mDCB{float:right;margin:10px 10px 10px 10px;} #mDCB a{margin:0px 2px 0px 2px;}");
  server.send(200, "text/css", css);
  httpRequestProcessed = true;
}

void EspWiFi::httpHandleDeviceListJss() {
  DBG_PRINT("httpHandleDeviceListJss: ");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.client().setNoDelay(true);
  String script = F("var mDE=true;function gE(n){return document.getElementById(n);}function cE(n){return document.createElement(n);}function cTN(d){return document.createTextNode(d);}function aC(p,c){p.appendChild(c);}function sA(o,a,v){o.setAttribute(a,v);}function aSU(u,p,n){var sN=cE('script');sA(sN,'type','text/javascript');sA(sN,'src',u);aC(p,sN);if(typeof n!=='undefined')sA(sN,'data-name',n);}function aST(t,p){var sN=cE('script');sA(sN,'type','text/javascript');aC(sN,cTN(t.replace('<script>', '').replace('</script>','')));aC(p,sN);}function hR(n,r,i){var o=gE(n);if(r.substring(0,8)=='<script>')aST(r,o);else o.innerHTML=r;}");
  script += F("function modDlgEn(e){mDE=(e==true?true:false);}");
  script += F("function windowClick(e){if(e.target.className==\"dc\"&&e.target.id){modDlg(true,false,e.target.id);}}function modDlg(open,save,id,action){if(mDE==false)return;action=((action===undefined)?'form':action);if(id=='back'){history.back();return;}document.onkeydown=(open?function(evt){evt=evt||window.event;var charCode=evt.keyCode||evt.which;if(charCode==27)modDlg(false,false);if(charCode==13)modDlg(false,true);}:null);var md=gE('mD');if(save){var form=gE('submitForm');if(form){form.submit();return;}form=gE('configForm');if(form){var aStr=form.action;var idx=aStr.indexOf('?');var url=aStr.substr(0, idx + 1);var params='';var elem;var parse;aStr=aStr.substr(idx + 1);while(1){idx=aStr.indexOf('&');if(idx>0)parse=aStr.substr(0, idx);else parse=aStr;");
  script += F("if(parse.substr(parse.length-1)!='='){params+=parse+'&';}else{elem=document.getElementsByName(parse.substr(0,parse.length-1));if(elem && elem[0])params+=parse+(elem[0].type!=\"checkbox\"?elem[0].value:(elem[0].checked?1:0))+'&';}if(idx>0) aStr=aStr.substr(idx+1); else break;}try{var xmlHttp=new XMLHttpRequest();xmlHttp.open('POST',url+params,false);xmlHttp.send(null);if(xmlHttp.status!=200){alert('Fehler: '+xmlHttp.statusText);return;}}catch(err){alert('Fehler: '+err.message);return;}}}if(open){try{var url='/config?ChipID=");
  script += getChipID();
  script += F("&action='+action;if(id.indexOf('schedule#')==0)url+='&schedule='+id.substr(9);else if(id=='wifi'||id=='net'||id=='ota'"); // ||id=='resetSearch'||id=='options'

  // for all external EspWiFiRequestHandler check menu
  EspWiFiRequestHandler *reqH = &mEspWiFiRequestHandler;
  uint8_t extMenuIDs;

  while (reqH != NULL) {
    if (reqH->isExternalRequestHandler() && (extMenuIDs = reqH->menuIdentifiers()) > 0)
      for (int i=0; i<extMenuIDs; i++)
        script += "||id=='" + reqH->menuIdentifiers(i) + "'";
    reqH = reqH->getNextRequestHandler();
  }

  script += F(")url+='&'+id+'=';else url+='&deviceID='+id;var xmlHttp=new XMLHttpRequest(); xmlHttp.open('POST',url,false);xmlHttp.send(null);if(xmlHttp.status != 200 && xmlHttp.status != 204 && xmlHttp.status != 205){alert('Fehler: '+xmlHttp.statusText);return;}if(xmlHttp.status==204){return;}if(id=='resetSearch'||xmlHttp.status==205){window.location.reload();return;}hR('mDCC',xmlHttp.responseText,id);}catch(err){alert('Fehler: '+err.message);return;}}md.style.visibility=(open?'visible':'hidden');if(!open){gE('mDCC').innerHTML='';}}");
  script += F("function filter(){var filter=document.getElementsByName('filter')[0];var table=gE('devices');if(filter&&table){var trs=document.getElementsByTagName('tr');i=1;while(trs[i]){trs[i].style.display=((filter.value&trs[i].firstChild.nodeValue)==filter.value||filter.value==255?'table-row':'none');i++;}}}");
  server.send(200, "text/javascript", script);
  httpRequestProcessed = true;
}

void EspWiFi::httpHandleNotFound() {
  DBG_PRINT("httpHandleNotFound: ");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.client().setNoDelay(true);
  server.send(404, "text/plain", message);
  httpRequestProcessed = true;
}

void EspWiFi::registerExternalRequestHandler(EspWiFiRequestHandler *externalRequestHandler) {
  EspWiFiRequestHandler *reqH = &mEspWiFiRequestHandler;

  while (reqH->getNextRequestHandler() != NULL)
    reqH = reqH->getNextRequestHandler();
  reqH->setNextRequestHandler(externalRequestHandler);
    
  DBG_PRINT("register ");
  server.addHandler(externalRequestHandler);
}

boolean EspWiFi::sendMultiCast(String msg) {
  boolean result = false;

  if (WiFi.status() != WL_CONNECTED)
    return result;

#ifndef _ESP_WIFI_UDP_MULTICAST_DISABLED
#ifdef ESP8266
  if (WiFiUdp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP()) == 1) {
    WiFiUdp.write(msg.c_str());
#endif
#ifdef ESP32
  if (WiFiUdp.beginMulticastPacket() == 1) {
    WiFiUdp.write((uint8_t*)msg.c_str(), msg.length());
#endif
    WiFiUdp.endPacket();
    yield();  // force ESP8266 background tasks (wifi); multicast requires approx. 600 µs vs. delay 1ms
    result = true;
  }
#endif

  return result;
}

String EspWiFi::ipString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

String EspWiFi::getChipID() {
  char buf[10];
#ifdef ESP8266
  sprintf(buf, "%08d", (unsigned int)ESP.getChipId());
#endif
#ifdef ESP32
  String mac = WiFi.macAddress();
  sprintf(buf, "%08d", strtol(String(mac.substring(9, 11) + mac.substring(12, 14) + mac.substring(15, 17)).c_str(), NULL, 16));
#endif
  
  return String(buf);
}

String EspWiFi::getHostname() {
#ifdef ESP8266
  return WiFi.hostname();
#endif  
#ifdef ESP32
  return WiFi.getHostname();
#endif  
}

String EspWiFi::getDefaultHostname() {
  String mac = WiFi.macAddress();

  return "ESP_" + mac.substring(9, 11) + mac.substring(12, 14) + mac.substring(15, 17);
}

void EspWiFi::printUpdateError() {
#if DBG_PRINTER != ' '
      Update.printError(DBG_PRINTER);
      DBG_FORCE_OUTPUT();
#endif
}

#ifdef _OTA_NO_SPIFFS

void EspWiFi::httpHandleOTA() {
  String message = "\n\nhttpHandleOTA: ";
  bool doReset = false;

  HTTPUpload& upload = server.upload();

  message += upload.totalSize;
  message += " Bytes received, md5: " + Update.md5String();
  if (!Update.hasError()) {
    message += "\nstarting upgrade!";
    doReset = true;
  } else {
    message += "\nupgrade failed!";
    DBG_PRINTLN(message);
    printUpdateError();
    DBG_FORCE_OUTPUT();
    Update.end(true);
  }

  DBG_PRINTLN(message);
  DBG_FORCE_OUTPUT();

  server.client().setNoDelay(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "See Other");

  if (doReset) {
    delay(1000);
#ifdef ESP8266
    ESP.reset();
#endif
#ifdef ESP32
    ESP.restart();
#endif
  }
  httpRequestProcessed = true;
}

void EspWiFi::httpHandleOTAData() {
  static HeaderBootMode1 otaHeader;
 
  // Upload 
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    DBG_PRINT("httpHandleOTAData: " + upload.filename);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      printUpdateError();
      DBG_PRINTLN("ERROR: UPLOAD_FILE_START");
      DBG_FORCE_OUTPUT();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // first block with data
    if (upload.totalSize == 0) {
      memcpy(&otaHeader, &upload.buf[0], sizeof(HeaderBootMode1));
      DBG_PRINTF(", magic: 0x%0x, size: 0x%0x, speed: 0x%0x\n", otaHeader.magic, ((otaHeader.flash_size_speed & 0xf0) >> 4), (otaHeader.flash_size_speed & 0x0f));
      DBG_FORCE_OUTPUT();

      if (otaHeader.magic != 0xe9)
        ;
    }
    
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      printUpdateError();
      DBG_PRINTLN("ERROR: UPLOAD_FILE_WRITE");
      DBG_FORCE_OUTPUT();
  }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { 
      DBG_PRINTF("Firmware size: %d", upload.totalSize);
    }
  } else {
    printUpdateError();
    DBG_PRINTLN("ERROR: UPLOAD_FILE_END");
    DBG_FORCE_OUTPUT();
  }
}

#endif  // _OTA_NO_SPIFFS

#ifndef _OTA_NO_SPIFFS

bool EspWiFi::initOtaFile(String filename, String mode) {
  SPIFFS.begin();
  otaFile = SPIFFS.open(filename, mode.c_str());

  if (otaFile)
    otaFileName = filename;

  return otaFile;
}

void EspWiFi::clearOtaFile() {
  if (otaFile)
    otaFile.close();
  if (SPIFFS.exists(otaFileName))
    SPIFFS.remove(otaFileName);
  otaFileName = "";
}

void EspWiFi::httpHandleOTA() {
  String message = "\n\nhttpHandleOTA: ";
  bool doUpdate = false;
  
  if (SPIFFS.exists(otaFileName) && initOtaFile(otaFileName, "r")) {
    message += otaFile.name();
    message += + " (";
    message += otaFile.size();
    message += " Bytes) received!";
    if ((doUpdate = Update.begin(otaFile.size()))) {
      message += "\nstarting upgrade!";
    } else
      clearOtaFile();
  }

  DBG_PRINTLN(message);

  server.client().setNoDelay(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "See Other");

  if (doUpdate) {
    DBG_PRINT("starting Update: ");
    size_t written = Update.write(otaFile);
    clearOtaFile();

    if (!Update.end() || Update.hasError()) {
      DBG_PRINTLN("failed!");
      Update.printError(Serial);
      Update.end(true);
    } else {
      DBG_PRINTLN("ok, md5 is " + Update.md5String());
      DBG_PRINTLN("restarting");
      delay(1000);
      ESP.reset();
    }
  }
  httpRequestProcessed = true;
}

void EspWiFi::httpHandleOTAData() {
  static HeaderBootMode1 otaHeader;
 
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    DBG_PRINT("httpHandleOTAData: " + upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // first block with data
    if (upload.totalSize == 0) {
      memcpy(&otaHeader, &upload.buf[0], sizeof(HeaderBootMode1));
      DBG_PRINTF(", magic: 0x%0x, size: 0x%0x, speed: 0x%0x\n", otaHeader.magic, ((otaHeader.flash_size_speed & 0xf0) >> 4), (otaHeader.flash_size_speed & 0x0f));

      if (otaHeader.magic == 0xe9)
        initOtaFile("/ota/" + getChipID() + ".bin", "w");
    }
    DBG_PRINT(".");
    if ((upload.totalSize % HTTP_UPLOAD_BUFLEN) == 20)
      DBG_PRINTLN("\n");

    if (otaFile && otaFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      DBG_PRINTLN("\nwriting file " + otaFileName + " failed!");
      clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaFile) {
      bool uploadComplete = (otaFile.size() == upload.totalSize);
      
      DBG_PRINTLN("\nend: %s (%d Bytes)\n", otaFile.name(), otaFile.size());
      otaFile.close();
      
      if (!uploadComplete)     
        clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    DBG_PRINTF("\naborted\n");
    clearOtaFile();
  }
}

#endif // _OTA_NO_SPIFFS

#endif  // ESP8266 || ESP32

