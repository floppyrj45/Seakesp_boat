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
  // 1) $DTPING,<TOF>,<unk>,<angle_deg>,<distance_dm>,...*CS
  // 2) $DTPING,<angle_deg>,<distance_m>,...*CS (legacy)
  float angleDeg = NAN;
  float distanceM = NAN;
  if (n >= 5 && t[3].length() && t[4].length()) {
    angleDeg = t[3].toFloat();
    // Distance exprimée en décimètres -> convertir en mètres
    distanceM = t[4].toFloat() / 10.0f;
  } else if (n >= 3 && t[1].length() && t[2].length()) {
    angleDeg = t[1].toFloat();
    distanceM = t[2].toFloat();
  }
  if (isfinite(angleDeg)) gSeaker.lastAngle = angleDeg;
  if (isfinite(distanceM)) gSeaker.lastDistance = distanceM;
  if (isfinite(angleDeg) || isfinite(distanceM)) {
    gSeaker.pingCounter++;
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
          if (echoSeaker) Serial.println(line);
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


