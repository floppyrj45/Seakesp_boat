#include "gps_skytraq.h"
#include "web_server.h"


static HardwareSerial* gpsSerial = nullptr;
static String rawBuffer;
static GpsFix lastFix;
static int gpsCurRx = -1, gpsCurTx = -1;
static bool mockEnabled = false;
static GpsFix mockFix;
static bool echoRaw = false;

static uint8_t nmeaChecksum(const String& s) {
  uint8_t c = 0;
  for (size_t i = 0; i < s.length(); ++i) c ^= (uint8_t)s[i];
  return c;
}

static double parseCoord(const String& token, const String& hemi) {
  if (token.length() < 4) return NAN;
  int dot = token.indexOf('.');
  if (dot < 0) return NAN;
  // Format NMEA: latitude ddmm.mmmm, longitude dddmm.mmmm
  int degLen = (dot > 3) ? (dot - 2) : 2; // safe: assumes at least mm.mmmm
  if (degLen < 2) degLen = 2; // fallback for malformed lat
  if (degLen > 3) degLen = 3; // cap for lon
  double deg = token.substring(0, degLen).toDouble();
  double min = token.substring(degLen).toDouble();
  double val = deg + (min / 60.0);
  if (hemi == "S" || hemi == "W") val = -val;
  return val;
}

static void parseGGA(const String* t, int n) {
  // $GxGGA,time,lat,N,lon,E,fix,sats,hdop,alt,M,geoid,M,age,ref*CS
  if (n < 15) return;
  lastFix.satellites = (uint16_t)t[7].toInt();
  lastFix.hdop = t[8].toFloat();
  lastFix.altitudeM = t[9].toFloat();
  lastFix.geoidM = t[11].toFloat();
  if (t[2].length()) lastFix.latitude = parseCoord(t[2], t[3]);
  if (t[4].length()) lastFix.longitude = parseCoord(t[4], t[5]);
  lastFix.fixQuality = (uint8_t)t[6].toInt();
  lastFix.valid = (lastFix.fixQuality != 0);
}

static void parseRMC(const String* t, int n) {
  // $GxRMC,time,status,lat,N,lon,E,sog,cog,date,magvar,dir,mode,...*CS
  if (n < 10) return;
  lastFix.valid = (t[2] == "A");
  // lat/lon à indices 3..6
  if (t[3].length() && t[4].length()) lastFix.latitude = parseCoord(t[3], t[4]);
  if (t[5].length() && t[6].length()) lastFix.longitude = parseCoord(t[5], t[6]);
  // vitesse/route fond
  if (t[7].length()) lastFix.speedKnots = t[7].toFloat();
  if (t[8].length()) {
    float newHdg = t[8].toFloat();
    lastFix.headingDeg = newHdg;
    Serial.printf("[GPS] RMC COG: %.1f° (speed: %.1fkn)\n", newHdg, lastFix.speedKnots);
  }
  // date: ddmmyy
  if (t[9].length() >= 6) {
    lastFix.day = (uint8_t)t[9].substring(0,2).toInt();
    lastFix.month = (uint8_t)t[9].substring(2,4).toInt();
    lastFix.year = (uint16_t)(2000 + t[9].substring(4,6).toInt());
  }
}

static void parseVTG(const String* t, int n) {
  // $..VTG,cog,T,,M,sog,N,,K*CS
  if (n < 10) return;
  if (t[1].length()) {
    float newHdg = t[1].toFloat();
    lastFix.headingDeg = newHdg;
    Serial.printf("[GPS] VTG COG: %.1f°\n", newHdg);
  }
  if (t[5].length()) lastFix.speedKnots = t[5].toFloat();
}

static void parseHDT(const String* t, int n) {
  // $..HDT,heading,T*CS
  if (n < 4) return;
  if (t[1].length()) {
    float newTrueHdg = t[1].toFloat();
    lastFix.trueHeadingDeg = newTrueHdg;
    Serial.printf("[GPS] HDT True Heading: %.1f°\n", newTrueHdg);
  }
}

static void parsePASHR(const String* t, int n) {
  // $PASHR,hhmmss.sss,heading,T,pitch,roll,heave,course,spd,lat,lon,alt,ins,hdop,vn,*CS (variant)
  if (n >= 4 && t[2].length()) {
    float newTrueHdg = t[2].toFloat();
    lastFix.trueHeadingDeg = newTrueHdg;
    Serial.printf("[GPS] PASHR True Heading: %.1f°\n", newTrueHdg);
  }
}

static void parsePSTI036(const String* t, int n) {
  // $PSTI,036,hhmmss.ss,x,x,heading,pitch,roll,*CS (SkyTraq proprietary)
  if (n >= 7 && t[4].length()) {
    float newTrueHdg = t[4].toFloat();
    lastFix.trueHeadingDeg = newTrueHdg;
    Serial.printf("[GPS] PSTI036 Heading: %.1f° (pitch: %s, roll: %s)\n", 
                 newTrueHdg, 
                 (t[5].length() ? t[5].c_str() : "nan"),
                 (t[6].length() ? t[6].c_str() : "nan"));
  }
}

static void parseLine(const String& line) {
  if (!line.startsWith("$")) return;
  int star = line.indexOf('*');
  if (star < 0) return;
  String payload = line.substring(1, star);
  String cks = line.substring(star + 1);
  cks.trim();
  uint8_t want = (uint8_t)strtol(cks.c_str(), nullptr, 16);
  if (nmeaChecksum(payload) != want) return;

  // split by comma
  const int MAXT = 32;
  String tokens[MAXT];
  int idx = 0;
  int start = 0;
  for (int i = 0; i <= payload.length() && idx < MAXT; ++i) {
    if (i == payload.length() || payload[i] == ',') {
      tokens[idx++] = payload.substring(start, i);
      start = i + 1;
    }
  }
  if (idx == 0) return;

  String type = tokens[0];
  // Support pour GPS (GP) et GNSS multi-constellation (GN)
  if (type.endsWith("GGA")) parseGGA(tokens, idx);
  else if (type.endsWith("RMC")) parseRMC(tokens, idx);
  else if (type.endsWith("VTG")) parseVTG(tokens, idx);
  else if (type.endsWith("HDT")) parseHDT(tokens, idx);
  else if (type.endsWith("THS")) parseHDT(tokens, idx); // THS = True Heading same format as HDT
  else if (type.startsWith("PASHR")) parsePASHR(tokens, idx);
  else if (type.startsWith("PSTI") && tokens[1] == "036") parsePSTI036(tokens, idx);
  
  // Debug pour voir le fixQuality reçu (supprimé pour éviter flood)
  // if (type.endsWith("GGA") && idx > 6) {
  //   Serial.printf("[GPS] GGA fixQuality=%s (raw field 6)\n", tokens[6].c_str());
  // }
}

void gpsBegin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin) {
  gpsSerial = &serial;
  gpsCurRx = rxPin; gpsCurTx = txPin;
  serial.begin(baud, SERIAL_8N1, rxPin, txPin);
  // Timeout plus long pour laisser le temps à une ligne NMEA complète d'arriver (ex: 9600 bps)
  serial.setTimeout(200);
  rawBuffer.reserve(4096);
}

bool gpsAutoDetectBaud(uint32_t& selectedBaud) {
  if (!gpsSerial) return false;
  // Essayer d'abord les vitesses les plus courantes (115200, 9600), puis autres, puis 921600 en dernier
  const uint32_t candidates[] = {115200, 9600, 57600, 38400, 19200, 921600};
  for (uint32_t b : candidates) {
    gpsSerial->end();
    gpsSerial->begin(b, SERIAL_8N1, gpsCurRx, gpsCurTx);
    unsigned long t0 = millis();
    int hits = 0;
    while (millis() - t0 < 1200) { // Fenêtre plus large pour laisser passer au moins 1-2 trames à 9600 bps
      if (gpsSerial->available()) {
        String line = gpsSerial->readStringUntil('\n');
        line.trim();
        if (line.startsWith("$") && line.indexOf('*') > 0) {
          hits++;
          if (hits >= 1) { selectedBaud = b; return true; }
        }
      }
      delay(5);
    }
  }
  return false;
}

void gpsPoll() {
  if (!gpsSerial) return;
  if (mockEnabled) {
    // synthèse minimale NMEA pour RAW et mise à jour lastFix
    lastFix = mockFix;
    char latStr[16] = ""; char lonStr[16] = ""; char ns='N', ew='E';
    double alat=lastFix.latitude, alon=lastFix.longitude;
    if (alat<0){ns='S'; alat=-alat;} if(alon<0){ew='W'; alon=-alon;}
    int latd=(int)alat; double latm=(alat-latd)*60.0; snprintf(latStr,sizeof(latStr),"%02d%07.4f",latd,latm);
    int lond=(int)alon; double lonm=(alon-lond)*60.0; snprintf(lonStr,sizeof(lonStr),"%03d%07.4f",lond,lonm);
    char rmc[160]; snprintf(rmc,sizeof(rmc),"GPRMC,000000,A,%s,%c,%s,%c,%.1f,%.1f,010100,,",latStr,ns,lonStr,ew,(double)lastFix.speedKnots,(double)lastFix.headingDeg);
    uint8_t cks=0; for(size_t i=0;i<strlen(rmc);++i) cks^=(uint8_t)rmc[i];
    char frm[180]; snprintf(frm,sizeof(frm),"$%s*%02X\n",rmc,cks);
    if (rawBuffer.length() > 8192) rawBuffer.remove(0, rawBuffer.length() - 4096);
    rawBuffer += frm;
    return;
  }
  // Lecture non bloquante, accumulation jusqu'à fin de ligne (\r ou \n)
  static String lineBuf;
  while (gpsSerial->available()) {
    char ch = (char)gpsSerial->read();
    if (ch == '\r' || ch == '\n') {
      if (lineBuf.length()) {
        String line = lineBuf; line.trim(); lineBuf = "";
        if (!line.isEmpty()) {
          if (rawBuffer.length() > 8192) rawBuffer.remove(0, rawBuffer.length() - 4096);
          rawBuffer += line + "\n";
          // Echo optionnel des trames NMEA sur le moniteur série et via WebSocket (debug)
          if (echoRaw) {
            Serial.println(line);
            String esc = line; esc.replace("\\", "\\\\"); esc.replace("\"", "\\\"");
            String js = String("{\"nmea\":\"") + esc + "\"}";
            wsBroadcastJson(js);
          }
          parseLine(line);
        }
      }
    } else {
      lineBuf += ch;
      if (lineBuf.length() > 256) lineBuf.remove(0, lineBuf.length() - 256);
    }
  }
}

GpsFix gpsGetFix() {
  return lastFix;
}

String gpsGetRaw(int maxLines) {
  if (maxLines <= 0) return String();
  // return last N lines from buffer
  int count = 0;
  int pos = rawBuffer.length() - 1;
  while (pos >= 0 && count < maxLines) {
    if (rawBuffer[pos] == '\n') count++;
    pos--;
  }
  int start = max(0, pos + 1);
  return rawBuffer.substring(start);
}

void gpsFeedRtcm(const uint8_t* data, size_t len) {
  if (!gpsSerial || !data || len == 0) return;
  gpsSerial->write(data, len);
}

void gpsSwapPins() {
  if (!gpsSerial) return;
  int newRx = gpsCurTx;
  int newTx = gpsCurRx;
  // Re-open UART with swapped pins, keep same baud
  uint32_t baud = 115200; // default
  // There is no direct getter; assume 115200 as used at begin.
  gpsSerial->end();
  gpsSerial->begin(baud, SERIAL_8N1, newRx, newTx);
  gpsCurRx = newRx; gpsCurTx = newTx;
}

void gpsSetMockEnabled(bool enabled) { mockEnabled = enabled; }
void gpsSetMockFix(const GpsFix& fix) {
  mockFix = fix;
  if (!isfinite(mockFix.trueHeadingDeg)) mockFix.trueHeadingDeg = mockFix.headingDeg;
  mockFix.valid = true;
}

void gpsSetEchoRaw(bool enabled) { echoRaw = enabled; }
bool gpsGetEchoRaw() { return echoRaw; }


