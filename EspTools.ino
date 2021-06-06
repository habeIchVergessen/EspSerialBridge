#ifndef LED_BUILTIN
  #if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO)
    #define LED_BUILTIN 9
  #endif
  #ifdef ESP32
    #define LED_BUILTIN 2
  #endif
#endif

void setupEspTools() {
  pinMode(LED_BUILTIN, OUTPUT);
  setLed(false);
}

void setLed(bool on) {
  digitalWrite(LED_BUILTIN, !on);
}

void blinkLed() {
  blinkLed(1);
}

void blinkLed(byte cnt) {
  blinkLed(cnt, 100);
}

void blinkLed(byte cnt, byte microseconds) {
  for (int i=cnt * 2 - 1; i>=0; i--) {
    setLed((i % 2));
    delayMicroseconds(microseconds);
  }
}

uint16_t  espToolsUptimeDays = 0;
unsigned long espToolsLastMillis = 0;

void loopEspTools() {
  unsigned long done, currMillis = millis();
  if (currMillis < espToolsLastMillis)
    done = 0xFFFFFFFF - espToolsLastMillis + currMillis;
  else
    done = currMillis - espToolsLastMillis;

  if (done >= 86400000) {
    espToolsUptimeDays++;
    espToolsLastMillis += 86400000;
  }
}

String uptime() {
  String result = "";

  unsigned long uptime = (millis() / 1000);

  if (espToolsUptimeDays > 0)
    result += String(espToolsUptimeDays) + "d, ";
  uptime %= 86400;
  uint8_t hours = uptime / 3600;
  result += (hours < 10 ? "0" + String(hours) : String(hours)) + ":";
  uptime %= 3600;
  uint8_t minutes = uptime / 60;
  result += (minutes < 10 ? "0" + String(minutes) : String(minutes)) + ".";
  uptime %= 60;
  result += (uptime < 10 ? "0" + String(uptime) : String(uptime));

  return result;
}
