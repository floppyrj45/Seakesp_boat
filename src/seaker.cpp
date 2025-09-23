#include "seaker.h"
#include "config.h"
#include "runtime_config.h"
#include "console_broadcast.h"

static HardwareSerial* seakerSerial = nullptr;
SeakerState gSeaker;
static bool mockOn = false;
static float mockBaseAngle = 0.0f, mockBaseDist = 10.0f, mockNoiseAng = 3.0f, mockNoiseDist = 1.5f;
static bool mockSweep = false; static float mockSweepRate = 0.0f; static unsigned long mockLastMs = 0;
static bool echoSeaker = false;

static uint8_t nmeaChecksum(const String& s) {
  uint8_t c = 0;
  for (size_t i = 0; i < s.length(); ++i) c ^= (uint8_t)s[i];
  return c;
}

void startSEAKER(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin) {
  seakerSerial = &serial;
  serial.begin(baud, SERIAL_8N1, rxPin, txPin);
  serial.setTimeout(2); // réduire pour limiter les blocages
}

bool sendSEAKERCommand(const String& payload) {
  if (!seakerSerial) return false;
  String frame = "$" + payload;
  uint8_t cks = nmeaChecksum(payload);
  char buf[8];
  snprintf(buf, sizeof(buf), "*%02X\r\n", cks);
  frame += buf;
  size_t n = seakerSerial->print(frame);
  return n == frame.length();
}

static void parseSTATUS(const String* t, int n) {
  // $STATUS,<status>,freq,snr,etx,erx,...*CS (format adaptatif)
  if (n >= 2) gSeaker.lastStatus = t[1];
  if (n >= 3 && t[2].length()) gSeaker.rxFrequency = t[2].toFloat();
  if (n >= 4 && t[3].length()) gSeaker.snr = t[3].toFloat();
  if (n >= 5 && t[4].length()) gSeaker.energyTx = t[4].toFloat();
  if (n >= 6 && t[5].length()) gSeaker.energyRx = t[5].toFloat();
}

static void parseDTPING(const String* t, int n) {
  // Formats observés:
  // 1) $DTPING,<TOF>,<TAT_ms>,<angle_deg>,<distance_dm>,...*CS
  // 2) $DTPING,<angle_deg>,<distance_m>,...*CS (legacy)
  float angleDeg = NAN;
  float distanceM = NAN;
  int tatMs = -1;
  if (n >= 5 && t[3].length() && t[4].length()) {
    // Nouveau format avec TAT en millisecondes
    if (t[2].length()) tatMs = t[2].toInt();
    angleDeg = t[3].toFloat();
    // Distance exprimée en décimètres -> convertir en mètres
    distanceM = t[4].toFloat() / 10.0f;
  } else if (n >= 3 && t[1].length() && t[2].length()) {
    // Format legacy sans TAT
    angleDeg = t[1].toFloat();
    distanceM = t[2].toFloat();
  }

  bool hasUseful = (isfinite(angleDeg) || isfinite(distanceM));
  bool accept = hasUseful;
  // Validation TAT: accepter uniquement si TAT est proche d'un multiple de 2000 ms
  // Tolérance par défaut: ±100 ms
  if (accept && tatMs >= 0 && gTatFilterEnabled) {
    const int expectedMs = 2000;
    const int tolMs = 100;
    int r = tatMs % expectedMs;
    if (!(r <= tolMs || r >= expectedMs - tolMs)) {
      accept = false;
      gSeaker.rejectedTat++;
    }
  }
  if (accept) {
    if (isfinite(angleDeg)) gSeaker.lastAngle = angleDeg;
    if (isfinite(distanceM)) gSeaker.lastDistance = distanceM;
    gSeaker.pingCounter++;
    gSeaker.acceptedPings++;
  }

  // Rapport périodique (toutes les ~2s) des acceptés/rejetés
  static unsigned long lastReportMs = 0;
  static unsigned long lastAcc = 0;
  static unsigned long lastRej = 0;
  unsigned long now = millis();
  if (now - lastReportMs >= 2000) {
    unsigned long acc2s = gSeaker.acceptedPings - lastAcc;
    unsigned long rej2s = gSeaker.rejectedTat - lastRej;
    gSeaker.tatAcc2s = acc2s;
    gSeaker.tatRej2s = rej2s;
    lastAcc = gSeaker.acceptedPings;
    lastRej = gSeaker.rejectedTat;
    lastReportMs = now;
    // Construire une trame $SEAK,TATSTAT,acc2s=..,rej2s=..
    String payload = String("SEAK,TATSTAT,acc2s=") + String(acc2s) + ",rej2s=" + String(rej2s);
    uint8_t cks = nmeaChecksum(payload);
    char buf[8]; snprintf(buf, sizeof(buf), "*%02X", cks);
    String line = String("$") + payload + String(buf);
    Serial.println(line);
    consoleBroadcastLine(line);
  }
}

void parseSeakerNMEA(const String& line) {
  if (!line.startsWith("$")) return;
  int star = line.indexOf('*');
  if (star < 0) return;
  String payload = line.substring(1, star);
  String cks = line.substring(star + 1);
  cks.trim();
  uint8_t want = (uint8_t)strtol(cks.c_str(), nullptr, 16);
  uint8_t got = 0; for (size_t i=0;i<payload.length();++i) got ^= (uint8_t)payload[i];
  if (got != want) return;

  const int MAXT = 32;
  String tokens[MAXT];
  int idx = 0, start = 0;
  for (int i = 0; i <= payload.length() && idx < MAXT; ++i) {
    if (i == payload.length() || payload[i] == ',') {
      tokens[idx++] = payload.substring(start, i);
      start = i + 1;
    }
  }
  if (idx == 0) return;
  if (tokens[0] == "STATUS") { parseSTATUS(tokens, idx); }
  else if (tokens[0] == "DTPING") { parseDTPING(tokens, idx); }
}

void pollSEAKER() {
  if (mockOn) {
    unsigned long now = millis(); double dt = (mockLastMs==0)? 0.0 : (now-mockLastMs)/1000.0; mockLastMs = now;
    float ang = mockBaseAngle + (mockSweep? (mockSweepRate*dt* (float)(random(0,2)?1:-1)) : 0.0f) + ((float)random(-100,101)/100.0f)*mockNoiseAng;
    float dist = max(0.0f, mockBaseDist + ((float)random(-100,101)/100.0f)*mockNoiseDist);
    gSeaker.lastAngle = ang; gSeaker.lastDistance = dist; gSeaker.lastStatus = "MOCK";
    return;
  }
  if (!seakerSerial) return;
  static String lineBuf;
  int guard = 0; // éviter de monopoliser trop longtemps
  while (seakerSerial->available() && guard++ < 256) {
    char ch = (char)seakerSerial->read();
    if (ch == '\r' || ch == '\n') {
      if (lineBuf.length()) {
        String line = lineBuf; line.trim(); lineBuf = "";
        if (!line.isEmpty()) {
          // Suppression echo SEAKER pour éviter flood série
          // if (echoSeaker) Serial.println(line);
          consoleBroadcastLine(line);
          parseSeakerNMEA(line);
        }
      }
    } else {
      lineBuf += ch;
      if (lineBuf.length() > 256) lineBuf.remove(0, lineBuf.length() - 256);
    }
  }
}

void seakerSetMock(bool enable, float baseAngleDeg, float baseDistanceM, float noiseAngleDeg, float noiseDistanceM, bool sweep, float sweepDegPerSec){
  mockOn = enable; mockBaseAngle=baseAngleDeg; mockBaseDist=baseDistanceM; mockNoiseAng=noiseAngleDeg; mockNoiseDist=noiseDistanceM; mockSweep=sweep; mockSweepRate=sweepDegPerSec; mockLastMs=0;
}

void seakerSetEchoRaw(bool enable) { echoSeaker = enable; }
bool seakerGetEchoRaw() { return echoSeaker; }


