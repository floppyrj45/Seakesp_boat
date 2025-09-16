#include "kogger.h"
#include "console_broadcast.h"

static HardwareSerial* koggerSerial = nullptr;
KoggerState gKogger;
static bool echoKogger = false;

static uint8_t nmeaChecksum(const String& s) {
  uint8_t c = 0; for (size_t i=0;i<s.length();++i) c ^= (uint8_t)s[i]; return c;
}

static bool validateNmea(const String& line, String& payloadOut){
  if (!line.startsWith("$")) return false;
  int star = line.indexOf('*'); if (star < 0) return false;
  String payload = line.substring(1, star);
  String cks = line.substring(star + 1); cks.trim();
  uint8_t want = (uint8_t)strtol(cks.c_str(), nullptr, 16);
  if (nmeaChecksum(payload) != want) return false;
  payloadOut = payload; return true;
}

static void parseDPT(const String* t, int n){
  // $--DPT,depthMeters,offset[,range]*CS
  if (n >= 2 && t[1].length()) gKogger.depthM = t[1].toFloat();
  if (n >= 3 && t[2].length()) gKogger.offsetM = t[2].toFloat();
  gKogger.lastMs = millis();
}

static void parseDBT(const String* t, int n){
  // $--DBT,depthFeet,f,depthMeters,M,depthFathoms,F*CS
  if (n >= 5 && t[3].length()) gKogger.depthM = t[3].toFloat();
  gKogger.lastMs = millis();
}

static void parseMTW(const String* t, int n){
  // $--MTW,temperature,C*CS
  if (n >= 3 && t[1].length()) gKogger.tempC = t[1].toFloat();
  gKogger.lastMs = millis();
}

static void parseLine(const String& payload){
  const int MAXT = 16; String tok[MAXT];
  int idx=0, start=0; for (int i=0;i<=payload.length() && idx<MAXT;i++){ if (i==payload.length() || payload[i]==','){ tok[idx++] = payload.substring(start,i); start=i+1; }}
  if (idx==0) return;
  const String& type = tok[0];
  if (type.endsWith("DPT")) parseDPT(tok, idx);
  else if (type.endsWith("DBT")) parseDBT(tok, idx);
  else if (type.endsWith("MTW") || type.endsWith("MTW,SD")) parseMTW(tok, idx);
}

void startKOGGER(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin){
  koggerSerial = &serial;
  serial.begin(baud, SERIAL_8N1, rxPin, txPin);
  serial.setTimeout(2);
}

void pollKOGGER(){
  if (!koggerSerial) return;
  static String lineBuf;
  int guard = 0;
  while (koggerSerial->available() && guard++ < 256){
    char ch = (char)koggerSerial->read();
    if (ch=='\r' || ch=='\n'){
      if (lineBuf.length()){
        String raw = lineBuf; lineBuf = ""; raw.trim();
        if (!raw.isEmpty()){
          if (echoKogger) Serial.println(raw);
          // Diffuser seulement les trames sonar connues côté console TCP
          if (raw.startsWith("$SDDPT") || raw.startsWith("$SDDBT") || raw.startsWith("$DPT") || raw.startsWith("$DBT") || raw.startsWith("$SDMTW") || raw.startsWith("$MTW")){
            consoleBroadcastLine(raw);
          }
          String payload; if (validateNmea(raw, payload)) parseLine(payload);
        }
      }
    } else {
      lineBuf += ch; if (lineBuf.length()>256) lineBuf.remove(0, lineBuf.length()-256);
    }
  }
}

void koggerSetEchoRaw(bool enable){ echoKogger = enable; }
bool koggerGetEchoRaw(){ return echoKogger; }


