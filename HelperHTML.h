#if defined(ESP8266) || defined(ESP32)

#ifndef _HTML_HELPER_H
#define _HTML_HELPER_H

#define textMark         F("\"")
#define actionField      F(" action=")
#define enctypeField     F(" enctype=")
#define maxLengthField   F(" maxlength=")
#define minField         F(" min=")
#define maxField         F(" max=")
#define methodField      F(" method=")
#define nameField        F(" name=")
#define typeField        F(" type=")
#define valueField       F(" value=")
#define idField          F(" id=")
#define classField       F(" class=")
#define titleField       F(" title=")
#define hrefField        F(" href=")
#define targetField      F(" target=")
#define onChangeField    F(" onchange=");
#define checkBox         "checkbox"
#define ipAddress        "ipAddress"

// prototypes
String htmlForm(String html, String pAction, String pMethod, String pID="", String pEnctype="", String pLegend="");
String htmlInput(String pName, String pType, String pValue, int pMaxLength=0, String pMinNumber="", String pMaxNumber="", String pPlaceHolder="");
String htmlFieldSet(String pHtml, String pLegend="");
String htmlOption(String pValue, String pText, bool pSelected=false);
String htmlSelect(String pName, String pOptions, String pOnChange="");
String htmlAnker(String pId, String pClass, String pText, String href="");

#endif	// _HTML_HELPER_H

#endif	// defined(ESP8266) || defined(ESP32)
