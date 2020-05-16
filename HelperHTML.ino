#if defined(ESP8266) || defined(ESP32)

#include "HelperHTML.h"

#ifdef _MQTT_SUPPORT
  #include "EspConfig.h"
#endif

String htmlBody(String html) {
  String doc = F("<!DOCTYPE html><html lang=\"de\">");
  doc += F("<head>\n");
  doc += htmlStyle("/static/deviceList.css");
  doc += htmlScript("/static/deviceList.js");
  doc += F("</head><body onclick=\"javascript:windowClick(event)\"><center><div style=\"width: 30em;\">");
  doc += "<h1>"; doc += PROGNAME; doc += " v"; doc += PROGVERS; doc += "@" + getChipID() + "</h1>";
  html.replace("\n", "<br>");
  doc += html;
  // dialog crap
  doc += F("<div id=\"mD\"><center><div id=\"mDC\"><p id=\"mDCC\"></p><p id=\"mDCB\"><a class=\"dc\" onclick=\"javascript:modDlg(false, true)\">Ok</a><a class=\"dc\" onclick=\"javascript:modDlg(false)\">Cancel</a></p></div></center></div>");
  doc += F("</div></center></body></html>");

  return doc;
}

String wifiForm() {
#ifdef ESP8266
  struct station_config current_conf;
#endif
#ifdef ESP32
  wifi_sta_config_t current_conf;
#endif
  
  String action = F("/config?ChipID=");
  action += getChipID();
  action += F("&wifi=submit&ssid=&password=");

  String html = htmlLabel("ssid", "ssid: ");
  html += htmlInput("ssid", "",  WiFi.SSID(), sizeof(current_conf.ssid)) + htmlNewLine();
  html += htmlLabel("password", "psk: ");
  html += htmlInput("password", "",  "", sizeof(current_conf.password)) + htmlNewLine();

  return htmlForm(html, action, "post", "configForm");
}

String netForm() {
  String action = F("/config?ChipID=");
  action += getChipID();
  action += F("&net=submit&hostname=&address=&mask=&gateway=&dns=");

  String html = htmlLabel("hostname", "hostname: ");
  String hostname = getHostname(), defaultHostname = getDefaultHostname();
  html += htmlInput("hostname", "",  (hostname == defaultHostname ? "" : hostname), 32, "", "", defaultHostname) + htmlNewLine();
  html += htmlLabel("address", "ip: ");
  html += htmlInput("address", ipAddress,  espConfig.getValue("address"), 15) + htmlNewLine();
  html += htmlLabel("mask", "mask: ");
  html += htmlInput("mask", ipAddress,  espConfig.getValue("mask"), 15) + htmlNewLine();
  html += htmlLabel("gateway", "gateway: ");
  html += htmlInput("gateway", ipAddress,  espConfig.getValue("gateway"), 15) + htmlNewLine();
  html += htmlLabel("dns", "dns: ");
  html += htmlInput("dns", ipAddress,  espConfig.getValue("dns"), 15) + htmlNewLine();

  return htmlForm(html, action, "post", "configForm");
}

String toCheckboxValue(String value) {
  return (value == NULL || value == "1" ? "1" : "0");
}

String optionForm() {
  String action = F("/config?ChipID=");
  action += getChipID();
  action += F("&options=submit&http=&kvpudp=&mqtt=&debug=");

  String html = htmlLabel("http", "http: ");
  html += htmlInput("http", checkBox, toCheckboxValue(espConfig.getValue("http"))) + htmlNewLine();
  html += htmlLabel("kvpudp", "kvpudp: ");
  html += htmlInput("kvpudp", checkBox, toCheckboxValue(espConfig.getValue("kvpudp"))) + htmlNewLine();
  html += htmlLabel("mqtt", "mqtt: ");
  html += htmlInput("mqtt", checkBox, toCheckboxValue(espConfig.getValue("mqtt"))) + htmlNewLine();
  html += htmlLabel("debug", "debug: ");
  html += htmlInput("debug", checkBox, toCheckboxValue(espConfig.getValue("debug"))) + htmlNewLine();

  return htmlForm(html, action, "post", "configForm");
}

#ifdef _MQTT_SUPPORT
String mqttForm() {
  String action = F("/config?ChipID=");
  action += getChipID();
  action += F("&mqtt=submit&server=&port=&user=&password=");

  String html = htmlLabel("server", "server: ");
  html += htmlInput("server", "", espConfig.getValue("mqttServer"), 40) + htmlNewLine();
  html += htmlLabel("port", "port: ");
  html += htmlInput("port", "number", espConfig.getValue("mqttPort"), 0, "1", "65535") + htmlNewLine();
  html += htmlLabel("user", "user: ");
  html += htmlInput("user", "", espConfig.getValue("mqttUser"), 40) + htmlNewLine();
  html += htmlLabel("password", "password: ");
  html += htmlInput("password", "", "", 40) + htmlNewLine();

  return htmlForm(html, action, "post", "configForm");
}
#endif  // _MQTT_SUPPORT

String flashForm() {
  String action = F("/ota/");
  action += getChipID();
  action += F(".bin");

  String html = htmlInput("file", "file", "", 0) + htmlNewLine();

  return htmlForm(html, action, "post", "submitForm", "multipart/form-data");
}

String htmlForm(String html, String pAction, String pMethod, String pID, String pEnctype, String pLegend) {
  String result = F("<form");
  if (pID != "") {
    result += idField;
    result += textMark;
    result += pID;
    result += textMark;
  }
  result += actionField;
  result += textMark;
  result += pAction;
  result += textMark;
  result += methodField;
  result += textMark;
  result += pMethod;
  result += textMark;
  if (pEnctype != "") {
    result += enctypeField;
    result += textMark;
    result += pEnctype;
    result += textMark;
  }
  result += F(">");
  result += htmlFieldSet(html, pLegend);
  result += F("</form>");
  
  return result;
}

String htmlFieldSet(String pHtml, String pLegend) {
  if (pLegend == "")
    return pHtml;
    
  String result = F("<fieldset>");
  if (pLegend != "") {
    result += F("<legend>");
    result += pLegend;
    result += F("</legend>");
  }
  result += pHtml;
  result += F("</fieldset>");

  return result;
}

String htmlNewLine() {
  return F("<br>");
}

String htmlLabel(String pFor, String pText) {
  String result = F("<label for=");
  result += textMark;
  result += pFor;
  result += textMark;
  result += ">" + pText;
  result += F("</label>");

  return result;
}

String htmlInput(String pName, String pType, String pValue, int pMaxLength, String pMinNumber, String pMaxNumber, String pPlaceHolder) {
  String result = F("<input ");
  result += nameField;
  result += textMark;
  result += pName;
  result += textMark;
  result += typeField;
  result += textMark;
  result += (pType != "" && pType != ipAddress ? pType : "text");
  result += textMark;
  
  if (pValue != "") {
    result += valueField;
    result += textMark;
    result += pValue;
    result += textMark;
  }
  if (pMaxLength > 0) {
    result += maxLengthField;
    result += textMark;
    result += String(pMaxLength);
    result += textMark;
  }
  if (pType == "number" && pMinNumber != "") {
    result += minField;
    result += textMark;
    result += pMinNumber;
    result += textMark;
  }
  if (pType == "number" && pMaxNumber != "") {
    result += maxField;
    result += textMark;
    result += pMaxNumber;
    result += textMark;
  }
  if (pValue == "1" && pType == checkBox)
    result += F(" checked");
  if (pType == ipAddress)
    result += " placeholder=\"0.0.0.0\" pattern=\"^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$\"";
  if (pPlaceHolder != "")
    result += " placeholder=\"" + pPlaceHolder + "\"";
  result += F(">");
  
  return result;
}

String htmlButton(String pType, String pName, String pValue, String pText) {
  String result = F("<button type=");
  result += textMark;
  result += pType;
  result += textMark;
  result += nameField;
  result += textMark;
  result += pName;
  result += textMark;
  result += valueField;
  result += textMark;
  result += pValue;
  result += textMark;
  result += F(">");
  result += pText;
  result += F("</button>");

  return result;
}

String htmlStyle(String hRef) {
  String result = F("<link rel=\"stylesheet\" type=\"text/css\" href=\"");
  result += hRef;
  result += F("\">\n");

  return result;
}

String htmlScript(String src) {
  String result = F("<script type=\"text/javascript\" src=\"");
  result += src;
  result += F("\"></script>\n");

  return result;
}

String htmlAnker(String pId, String pClass, String pText, String href) {
  String result = F("<a ");

  if (pId != "") {
    result += idField;
    result += textMark;
    result += pId;
    result += textMark;
  }

  if (pClass != "") {
    result += classField;
    result += textMark;
    result += pClass;
    result += textMark;
  }
  
  if (href != "") {
    result += hrefField;
    result += textMark;
    result += href;
    result += textMark;
  }

  result += F(">");
  result += pText;
  result += F("</a>");
  
  return result;  
}

String htmlMenuItem(String pId, String pText) {
  return htmlAnker(pId, "dc", pText);
}

String htmlOption(String pValue, String pText) {
  return htmlOption(pValue, pText, false);
}

String htmlOption(String pValue, String pText, bool pSelected) {
  String result = F("<option");
  result += valueField;
  result += textMark;
  result += pValue;
  result += textMark;
  if (pSelected)
    result += F(" selected");
  result += F(">");
  result += pText;
  result += F("</option>");

  return result;
}

String htmlSelect(String pName, String pOptions) {
  return htmlSelect(pName, pOptions, "");
}

String htmlSelect(String pName, String pOptions, String pOnChange) {
  String result = F("<select");
  result += nameField;
  result += textMark;
  result += pName;
  result += textMark;
  if (pOnChange != "") {
    result += onChangeField;
    result += textMark;
    result += pOnChange;
    result += textMark;
  }
  result += F(">");
  result += pOptions;
  result += F("</select>");

  return result;
}

#endif  // ESP8266 || ESP32

