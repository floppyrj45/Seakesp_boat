#include <Arduino.h>
#include <stdarg.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "config.h"
#include "gps_skytraq.h"
#include "seaker.h"
#include "runtime_config.h"
#include "utm.h"
#include "console_broadcast.h"
#include "target_filter.h"
#include "web_server.h"
#include "telemetry_state.h"
#include "power.h"

// 🏷️ Version firmware
const char* FIRMWARE_VERSION = "2.1.0";
const char* BUILD_DATE = __DATE__ " " __TIME__;

// Forward decls (helpers defined later in file)
static float estimateGpsPosStd(const GpsFix& f);
static float estimateSeakerPosStd(float distanceM);
static void printTargetFilteredFrame(double tgtLat, double tgtLon, float posStd);
static TargetFilterState gTf; static unsigned long gTfLastMs = 0;

static HardwareSerial& GPS = Serial1; // UART1
static HardwareSerial& SEAKER = Serial2; // UART2
static Adafruit_INA219 gIna219;
static bool gInaReady = false;

// Forward declaration pour la fonction WiFi Manager
static void wifiManagerStep();
static void setupWiFiServices();

enum LogLevel { LOG_ERROR=0, LOG_WARN=1, LOG_LOW=2, LOG_INFO=3, LOG_DEBUG=4 };
volatile LogLevel gLogLevel = LOG_LOW; // réduit par défaut
static void logMsg(LogLevel lvl, const char* tag, const char* fmt, ...) {
  if (lvl > gLogLevel) return;
  char buf[192];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  const char* lvls = (lvl==LOG_ERROR?"E": (lvl==LOG_WARN?"W": (lvl==LOG_INFO?"I":"D")));
  Serial.printf("[%s][%s] %s\n", lvls, tag, buf);
}

// Tentative de recovery I2C si SDA reste bloquée à LOW (esclave figé)
static void tryI2CBusRecovery(int sdaPin, int sclPin) {
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
  delay(2);
  if (digitalRead(sdaPin) == LOW) {
    // Générer des pulses sur SCL pour libérer l'esclave
    for (int i = 0; i < 16 && digitalRead(sdaPin) == LOW; ++i) {
      pinMode(sclPin, OUTPUT);
      digitalWrite(sclPin, LOW);
      delayMicroseconds(6);
      pinMode(sclPin, INPUT_PULLUP); // relâche SCL (haut via pullup)
      delayMicroseconds(6);
    }
    // Tentative d'un STOP: SDA monte alors que SCL est haut
    pinMode(sclPin, INPUT_PULLUP);
    delayMicroseconds(6);
    pinMode(sdaPin, OUTPUT);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(6);
    pinMode(sdaPin, INPUT_PULLUP);
    delayMicroseconds(6);
  }
}

// WiFi credentials (dev): try STA from prefs/env later; fallback AP
// Runtime config provided in runtime_config.cpp
extern String gWifiSsid;
extern String gWifiPass;

// GPS Forward TCP - pour envoyer les messages NMEA GPS au ROV
static WiFiServer gpsForwardServer(10111); // Port 10111 pour le forward GPS
static WiFiClient gpsForwardClient;
volatile bool gGpsForwardEnabled = false;

// WiFi Manager - Gestion automatique connexion/reconnexion/fallback AP
enum WifiState { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED, WIFI_AP_MODE };
volatile WifiState gWifiState = WIFI_DISCONNECTED;
volatile bool gWifiApFallback = false;
static unsigned long gLastWifiCheck = 0;
static unsigned long gWifiConnectStart = 0;
static int gWifiRetryCount = 0;
static const char* kApSsid = "SeakerESP-Config";
static const char* kApPassword = "seaker123";

// Configuration de correction de distance SEAKER
enum SeakerMode { SEAKER_NORMAL, SEAKER_OFFSET, SEAKER_TRANSPONDER };
SeakerMode gSeakerMode = SEAKER_NORMAL;
float gSeakerDistOffset = 0.0f; // Offset en mètres
float gSeakerTransponderDelay = 0.0f; // Délai transpondeur en millisecondes

// NTRIP client minimal (TCP) pour flux RTCM -> UART GPS
static TaskHandle_t ntripTaskHandle = nullptr;
static void ntripTask(void* arg) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) { vTaskDelay(pdMS_TO_TICKS(2000)); continue; }
    if (!gNtripEnabled) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    WiFiClient cli;
    if (!cli.connect(gNtripHost.c_str(), gNtripPort)) { vTaskDelay(pdMS_TO_TICKS(5000)); continue; }
    String auth = "";
    if (gNtripUser.length()) {
      String up = gNtripUser + ":" + gNtripPass;
      size_t outLen = (up.length()+2)/3*4; String b; b.reserve(outLen);
      const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      int val=0, valb=-6; for (uint8_t c: up) { val=(val<<8)+c; valb+=8; while(valb>=0){ b+=tbl[(val>>valb)&0x3F]; valb-=6; } }
      while (b.length()%4) b+='='; auth = b;
    }
    String req = String("GET /") + gNtripMount + " HTTP/1.0\r\n";
    req += String("Host: ") + gNtripHost + "\r\n";
    req += String("User-Agent: ESP32-Seaker/") + kFirmwareVersion + "\r\n";
    if (auth.length()) req += String("Authorization: Basic ") + auth + "\r\n";
    req += "Connection: keep-alive\r\n\r\n";
    cli.print(req);

    unsigned long lastGga = 0;
    while (cli.connected()) {
      while (cli.available()) {
        uint8_t buf[256]; int n=cli.read(buf, sizeof(buf)); if (n>0) {
          gRtcmRxBytes += n; gRtcmLastMs = millis();
          gpsFeedRtcm(buf, n); gRtcmFwdBytes += n;
        }
      }
      if (gNtripReconnectRequested) { break; }
      if (millis() - lastGga > 10000) {
        GpsFix f = gpsGetFix();
        char latStr[16] = ""; char lonStr[16] = ""; char ns='N', ew='E';
        double alat=f.latitude, alon=f.longitude;
        if (alat<0){ns='S'; alat=-alat;} if(alon<0){ew='W'; alon=-alon;}
        int latd = (int)alat; double latm=(alat-latd)*60.0; snprintf(latStr,sizeof(latStr),"%02d%07.4f",latd,latm);
        int lond = (int)alon; double lonm=(alon-lond)*60.0; snprintf(lonStr,sizeof(lonStr),"%03d%07.4f",lond,lonm);
        char gga[128]; snprintf(gga,sizeof(gga),"GPGGA,000000,%s,%c,%s,%c,%d,%d,%.1f,%.1f,M,0.0,M,,",latStr,ns,lonStr,ew,f.valid?1:0,f.satellites,(double)f.hdop,(double)f.altitudeM);
        uint8_t cks=0; for (size_t i=0;i<strlen(gga);++i) cks^=(uint8_t)gga[i];
        char frame[160]; snprintf(frame,sizeof(frame),"$%s*%02X\r\n",gga,cks);
        cli.print(frame);
        lastGga = millis();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    cli.stop();
    gNtripReconnectRequested = false;
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// --- Dev/CLI Série ---
static void printFixSummary() {}

bool readPower(float& voltageV, float& current_mA){
  if (!gInaReady) return false;
  float busV = gIna219.getBusVoltage_V();
  float shuntV = gIna219.getShuntVoltage_mV() / 1000.0f;
  current_mA = gIna219.getCurrent_mA();
  voltageV = busV + shuntV;
  return true;
}

// Fonction pour calculer le checksum NMEA
static uint8_t nmeaChecksum(const String& s) {
  uint8_t c = 0; for (size_t i=0;i<s.length();++i) c ^= (uint8_t)s[i]; return c;
}

// Créer une trame GPGGA avec la position TARGET
static void sendTargetAsGGA() {
  if (!gGpsForwardEnabled || !gpsForwardClient || !gpsForwardClient.connected()) return;
  if (isnan(gTargetFLat) || isnan(gTargetFLon)) return;
  
  // Format GPGGA: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
  // Exemple: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
  
  // Utiliser millis pour créer un timestamp simple
  unsigned long ms = millis();
  int hours = (ms / 3600000) % 24;
  int minutes = (ms / 60000) % 60;
  int seconds = (ms / 1000) % 60;
  char timeStr[10];
  sprintf(timeStr, "%02d%02d%02d.00", hours, minutes, seconds);
  
  // Convertir lat/lon en format NMEA (ddmm.mmmm)
  double latAbs = fabs(gTargetFLat);
  int latDeg = (int)latAbs;
  double latMin = (latAbs - latDeg) * 60.0;
  char latHemi = gTargetFLat >= 0 ? 'N' : 'S';
  
  double lonAbs = fabs(gTargetFLon);
  int lonDeg = (int)lonAbs;
  double lonMin = (lonAbs - lonDeg) * 60.0;
  char lonHemi = gTargetFLon >= 0 ? 'E' : 'W';
  
  // Construire la trame GPGGA
  char gga[200];
  snprintf(gga, sizeof(gga), "GPGGA,%s,%02d%08.5f,%c,%03d%08.5f,%c,1,08,1.0,0.0,M,0.0,M,,",
    timeStr,
    latDeg, latMin, latHemi,
    lonDeg, lonMin, lonHemi
  );
  
  // Calculer et ajouter le checksum
  uint8_t cks = nmeaChecksum(String(gga));
  char fullMsg[220];
  snprintf(fullMsg, sizeof(fullMsg), "$%s*%02X\r\n", gga, cks);
  
  // Envoyer via TCP
  gpsForwardClient.print(fullMsg);
  
  // Log pour debug
  Serial.printf("[GPS Forward] GGA sent: lat=%.6f lon=%.6f (R95=%.1fm)\n", gTargetFLat, gTargetFLon, 
                isfinite(gTargetFR95) ? gTargetFR95 : 0.0f);
}

// Diffuse une trame NMEA-like sur Serial + TCP + WS
static void broadcastNmea(const String& payload) {
  uint8_t cks = nmeaChecksum(payload);
  char cksHex[3]; snprintf(cksHex, sizeof(cksHex), "%02X", cks);
  Serial.print("$"); Serial.print(payload); Serial.print("*"); Serial.print(cksHex); Serial.print("\r\n");
  String line = String("$") + payload + String("*") + String(cksHex);
  consoleBroadcastLine(line);
  // WS
  String js = String("{\"nmea\":\"") + line + "\"}";
  wsBroadcastJson(js);
  
  // Ne pas forward les messages GPS normaux quand GPS Forward est activé
  // On va créer nos propres messages avec la position TARGET
}

static void printHelp() {
  Serial.println("=== CLI Seaker (Serial) ===");
  Serial.println("h: help");
  Serial.println("r: dump RAW NMEA (50 lignes)");
  Serial.println("p: print fix résumé");
  Serial.println("e: toggle echo RAW -> Serial");
  Serial.println("s: toggle echo SEAKER RAW -> Serial");
  Serial.println("b: auto-détection baud GPS");
  Serial.println("V: augmenter verbosité (DEBUG max)");
  Serial.println("v: diminuer verbosité (ERROR min)");
  Serial.println("(Niveaux: ERROR < WARN < LOW < INFO < DEBUG)");
  Serial.println("M: définir le mountpoint NTRIP (terminer par \r\n)");
  Serial.println("L: lister mountpoints (caster sourcé, si dispo)");
  Serial.println("I: inverser angle SEAKER (toggle)");
  Serial.println("O: définir offset angle SEAKER en degrés (CR pour valider)");
  Serial.println("A: définir sigma angulaire SEAKER en degrés (ex: 3)");
  Serial.println("R: définir erreur relative distance SEAKER (ex: 0.005)");
  Serial.println("K: définir bruit process Kalman aStd (m/s^2)");
  Serial.println("G: définir seuil gating Kalman (sigma)");
}

static void printSysFrame() {
  unsigned long now = millis();
  unsigned long ageRtcm = (gRtcmLastMs==0)? 0 : (now - gRtcmLastMs);
  String payload = "SYS,";
  payload += "uptime=" + String(now/1000);
  payload += ",rtcm_rx=" + String((unsigned long)gRtcmRxBytes);
  payload += ",rtcm_fwd=" + String((unsigned long)gRtcmFwdBytes);
  payload += ",rtcm_age_ms=" + String((unsigned long)ageRtcm);
  
  // État WiFi selon le WiFi Manager
  String wifiStatus;
  String ipAddress;
  int rssi = 0;
  
  switch(gWifiState) {
    case WIFI_CONNECTED:
      wifiStatus = "STA";
      ipAddress = WiFi.localIP().toString();
      rssi = WiFi.RSSI();
      break;
    case WIFI_AP_MODE:
      wifiStatus = gWifiApFallback ? "AP-FALLBACK" : "AP";
      ipAddress = WiFi.softAPIP().toString();
      rssi = 0;
      break;
    case WIFI_CONNECTING:
      wifiStatus = "CONNECTING";
      ipAddress = "0.0.0.0";
      rssi = 0;
      break;
    default:
      wifiStatus = "DISCONNECTED";
      ipAddress = "0.0.0.0";
      rssi = 0;
      break;
  }
  
  payload += ",wifi=" + wifiStatus;
  payload += ",ip=" + ipAddress;
  payload += ",rssi=" + String(rssi) + "dBm";
  broadcastNmea(payload);
}

static void printNtripFrame() {
  unsigned long now = millis();
  unsigned long ageRtcm = (gRtcmLastMs==0)? 0 : (now - gRtcmLastMs);
  bool streaming = (gRtcmLastMs != 0) && (now - gRtcmLastMs < 5000);
  String payload = "NTRIP,";
  payload += "enabled=" + String(gNtripEnabled ? 1 : 0);
  payload += ",host=" + gNtripHost;
  payload += ",port=" + String((unsigned)gNtripPort);
  payload += ",mount=" + gNtripMount;
  payload += ",streaming=" + String(streaming ? 1 : 0);
  payload += ",rtcm_rx=" + String((unsigned long)gRtcmRxBytes);
  payload += ",rtcm_fwd=" + String((unsigned long)gRtcmFwdBytes);
  payload += ",rtcm_age_ms=" + String((unsigned long)ageRtcm);
  broadcastNmea(payload);
}

static void printGpsSummaryFrame() {
  GpsFix f = gpsGetFix();
  float hdg = isfinite(f.trueHeadingDeg) ? f.trueHeadingDeg : f.headingDeg;
  
  // DEBUG: Afficher les valeurs brutes de heading
  Serial.printf("[DEBUG] Raw heading values - trueHeadingDeg: %.1f, headingDeg: %.1f, final hdg: %.1f\n", 
                f.trueHeadingDeg, f.headingDeg, hdg);
  
  if (isfinite(hdg)) {
    while (hdg < 0) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;
    Serial.printf("[DEBUG] Normalized heading: %.1f°\n", hdg);
  } else {
    Serial.printf("[DEBUG] Heading is NOT finite!\n");
  }
  
  // Déterminer le statut GPS
  String gpsStatus = "NAT"; // Natural
  if (f.fixQuality == 4) gpsStatus = "FIX"; // RTK Fix
  else if (f.fixQuality == 5) gpsStatus = "FLT"; // RTK Float
  else if (f.fixQuality == 2) gpsStatus = "DGP"; // DGPS
  
  String payload = "GPS,";
  payload += "valid=" + String(f.valid ? 1 : 0);
  payload += ",status=" + gpsStatus;
  payload += ",sats=" + String((unsigned)f.satellites);
  payload += ",hdop=" + String(isfinite(f.hdop) ? String(f.hdop,1) : String("nan"));
  payload += ",lat=" + String(isfinite(f.latitude) ? String(f.latitude,6) : String("nan"));
  payload += ",lon=" + String(isfinite(f.longitude) ? String(f.longitude,6) : String("nan"));
  payload += ",alt=" + String(isfinite(f.altitudeM) ? String(f.altitudeM,1) : String("nan"));
  payload += ",hdg=" + String(isfinite(hdg) ? String(hdg,1) : String("nan"));
  payload += ",kn=" + String(isfinite(f.speedKnots) ? String(f.speedKnots,1) : String("nan"));
  broadcastNmea(payload);
}

// --- SEAKER contrôle & TARGET ---
static void seakerSendGoStart() { sendSEAKERCommand("GOSEAK,4,1,1500,1"); }
static void seakerSendGoStop()  { sendSEAKERCommand("GOSEAK,4,1,1500,0"); }

static int parseSeakerStatus() {
  // gSeaker.lastStatus peut contenir "0/1/2/3" ou texte
  const String& s = gSeaker.lastStatus;
  if (!s.length()) return -1;
  if (s == "MOCK") return 2;
  char c = s[0]; if (c>='0' && c<='9') return (int)(c-'0');
  return -1;
}

// Fonction pour corriger la distance selon le mode configuré
static float correctSeakerDistance(float rawDistance) {
  switch(gSeakerMode) {
    case SEAKER_OFFSET:
      // Mode offset simple : soustraire une distance fixe
      return rawDistance - gSeakerDistOffset;
      
    case SEAKER_TRANSPONDER: {
      // Mode transpondeur : 
      // 1. Convertir le délai ms en distance (vitesse du son = 1500 m/s)
      float delayDistance = (gSeakerTransponderDelay / 1000.0f) * 1500.0f;
      // 2. Soustraire cette distance
      float remainingDistance = rawDistance - delayDistance;
      // 3. Diviser par 2 (aller-retour)
      return remainingDistance / 2.0f;
    }
    
    case SEAKER_NORMAL:
    default:
      return rawDistance;
  }
}

static void printTargetFrame() {
  GpsFix fix = gpsGetFix();
  if (!fix.valid || !isfinite(gSeaker.lastAngle) || !isfinite(gSeaker.lastDistance)) return;
  double platformAz = isfinite(fix.trueHeadingDeg) ? fix.trueHeadingDeg : fix.headingDeg;
  if (!isfinite(platformAz)) return;
  // Appliquer inversion/offset SEAKER
  double rel = gSeaker.lastAngle;
  if (gSeakerInvertAngle) rel = -rel;
  rel += (double)gSeakerAngleOffsetDeg;
  while (rel < 0) rel += 360.0; while (rel >= 360.0) rel -= 360.0;
  double az = platformAz + rel;
  while (az < 0) az += 360.0; while (az >= 360.0) az -= 360.0;
  // Appliquer la correction de distance selon le mode
  double d = correctSeakerDistance(gSeaker.lastDistance); // mètres corrigés

  // Calcul en UTM
  int zone; bool north; double e0,n0;
  if (!wgs84ToUtm(fix.latitude, fix.longitude, zone, north, e0, n0)) return;
  double brg = az * (M_PI/180.0);
  double de = d * sin(brg);
  double dn = d * cos(brg);
  double e1 = e0 + de;
  double n1 = n0 + dn;
  double tgtLat, tgtLon;
  if (!utmToWgs84(zone, north, e1, n1, tgtLat, tgtLon)) return;
  // Estimation d'incertitude (écart-type) et r95
  float gpsStd = estimateGpsPosStd(fix);
  float seakerStd = estimateSeakerPosStd((float)d);
  float measStd = sqrtf(gpsStd*gpsStd + seakerStd*seakerStd);
  String payload = "TARGET,";
  payload += String(tgtLat, 7) + "," + String(tgtLon, 7);
  payload += ",az=" + String(az,1) + ",dist_m=" + String(d,1);
  payload += ",r95_m=" + String(measStd * 2.45f, 2);
  broadcastNmea(payload);

  // Toujours envoyer la position TARGET brute calculée via WebSocket
  {
    // Mise à jour globale pour l'API et WebSocket
    telemetrySetTargetF(tgtLat, tgtLon, measStd * 2.45f);
    String js = String("{\"targetf\":{\"lat\":") + String(tgtLat,7) + ",\"lon\":" + String(tgtLon,7) + ",\"r95_m\":" + String(measStd*2.45f,2) + "}}";
    wsBroadcastJson(js);
    
    // Position TARGET mise à jour - sera envoyée par le timer à 1Hz
    
    // Debug: log chaque mise à jour target
    Serial.printf("[TARGET] Raw: lat=%.7f lon=%.7f az=%.1f dist=%.1f r95=%.2f\n", 
                  tgtLat, tgtLon, az, d, measStd * 2.45f);
  }
  
  // Kalman 2D avec gating et sortie TARGETF filtré
  static int tfZone = 0; static bool tfNorth = true;
  unsigned long nowMs = millis(); float dt = (gTfLastMs==0)? 0.0f : (nowMs - gTfLastMs) / 1000.0f; gTfLastMs = nowMs;
  if (!gTf.initialized) { tfZone = zone; tfNorth = north; }
  if (dt > 0.0f) targetFilterPredict(gTf, dt, gKalmanAccelStd);
  float innov = targetFilterUpdate(gTf, (float)e1, (float)n1, measStd);
  if (innov < gKalmanGate) {
    double fLat, fLon;
    if (utmToWgs84(tfZone, tfNorth, gTf.x, gTf.y, fLat, fLon)) {
      float posStdF = sqrtf(max(0.0f, (gTf.Pxx + gTf.Pyy) * 0.5f));
      printTargetFilteredFrame(fLat, fLon, posStdF);
      // Mettre à jour avec la version filtrée si acceptée
      telemetrySetTargetF(fLat, fLon, posStdF * 2.45f);
      // Push la version filtrée
      String js = String("{\"targetf\":{\"lat\":") + String(fLat,7) + ",\"lon\":" + String(fLon,7) + ",\"r95_m\":" + String(posStdF*2.45f,2) + ",\"filtered\":true}}";
      wsBroadcastJson(js);
    }
  }
}

// --- Cible filtrée (Kalman) ---
// moved to top-level forward declaration
static float estimateGpsPosStd(const GpsFix& f){
  // Estimation grossière en mètres selon fixQuality/hdop
  if (f.fixQuality == 4) return 0.03f;      // RTK Fix ~3cm
  if (f.fixQuality == 5) return 0.1f;       // RTK Float ~10cm
  if (isfinite(f.hdop)) return max(1.0f, f.hdop * 1.5f);
  return 3.0f; // fallback
}
static float estimateSeakerPosStd(float distanceM){
  // Erreur angulaire configurable (deg)
  const float angRadStd = gSeakerAngleSigmaDeg * (M_PI/180.0f);
  float lateral = fabs(distanceM) * angRadStd;
  // Erreur relative de distance configurable
  float rangeErr = max(0.1f, gSeakerRangeRel * fabs(distanceM));
  return sqrtf(lateral*lateral + rangeErr*rangeErr);
}
static void printTargetFilteredFrame(double tgtLat, double tgtLon, float posStd){
  String payload = "TARGETF,";
  payload += String(tgtLat, 7) + "," + String(tgtLon, 7);
  payload += ",r95_m=" + String(posStd * 2.45f, 2); // ~2.45*std pour r95 2D approximé
  broadcastNmea(payload);
}

static void seakerControllerStep() {
  enum { ST_INIT=0, ST_WAIT, ST_OK, ST_RECOVER };
  static int st = ST_INIT;
  static unsigned long t0 = 0;
  int stv = parseSeakerStatus();
  switch (st) {
    case ST_INIT:
      seakerSendGoStart();
      Serial.println("$SEAK,GOSEAK,start*00");
      t0 = millis(); st = ST_WAIT;
      break;
    case ST_WAIT:
      if (stv == 2) { Serial.println("$SEAK,STATUS,2*00"); st = ST_OK; }
      else if (stv == 0 || stv == 3 || (millis()-t0 > 15000)) { // échec, arrêt ou timeout
        seakerSendGoStop(); Serial.println("$SEAK,GOSEAK,stop*00");
        t0 = millis(); st = ST_RECOVER;
      }
      break;
    case ST_RECOVER:
      if (millis() - t0 > 2000) { st = ST_INIT; }
      break;
    case ST_OK:
      // rien; surveiller si retombe en 3
      if (stv == 0 || stv == 3) { // statut perdu/arrêt → relancer
        seakerSendGoStop(); Serial.println("$SEAK,GOSEAK,stop*00");
        t0 = millis(); st = ST_RECOVER;
      }
      break;
  }
}

// --- Tâche SEAKER dédiée (core 1) ---
static void seakerTask(void* arg){
  (void)arg;
  for(;;){
    pollSEAKER();
    seakerControllerStep();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --- WiFi Manager - Gestion automatique des connexions ---
static void setupWiFiServices() {
  // mDNS (seakesp.local)
  if (MDNS.begin("seakesp")) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("nmea-0183", "tcp", 10110);
    MDNS.addService("gps-forward", "tcp", 10111);
    Serial.println("[WiFi] mDNS: seakesp.local actif");
  } else {
    Serial.println("[WiFi] mDNS: échec d'initialisation");
  }
  
  // Démarrer les serveurs TCP
  gpsForwardServer.begin();
  gpsForwardServer.setNoDelay(true);
  Serial.println("[WiFi] GPS Forward: Server started on port 10111");
  
  consoleBegin();
  webSetup();
  Serial.println("[WiFi] Services web et TCP activés");
}

static void wifiManagerStep() {
  unsigned long now = millis();
  
  // Vérification toutes les 5 secondes
  if (now - gLastWifiCheck < 5000) return;
  gLastWifiCheck = now;
  
  switch (gWifiState) {
    case WIFI_DISCONNECTED:
      // Si trop d'échecs → mode AP direct
      if (gWifiRetryCount >= 3) {
        Serial.printf("[WiFi] 💥 %d échecs → Basculement FORCÉ en mode AP\n", gWifiRetryCount);
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(kApSsid, kApPassword);
        Serial.printf("[WiFi] 📡 Mode AP FORCÉ actif: SSID='%s', IP=%s\n", 
                      kApSsid, WiFi.softAPIP().toString().c_str());
        gWifiState = WIFI_AP_MODE;
        gWifiApFallback = true;
        setupWiFiServices();
        break;
      }
      
      // Tentative de connexion STA (si pas trop d'échecs)
      if (gWifiSsid.length() > 0) {
        Serial.printf("[WiFi] 🔄 Tentative connexion STA à '%s' (essai %d/3)...\n", gWifiSsid.c_str(), gWifiRetryCount + 1);
        Serial.printf("[WiFi] Debug: SSID='%s', Pass='%s' (len=%d)\n", gWifiSsid.c_str(), gWifiPass.c_str(), gWifiPass.length());
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.setHostname("seakesp");
        WiFi.begin(gWifiSsid.c_str(), gWifiPass.c_str());
        gWifiState = WIFI_CONNECTING;
        gWifiConnectStart = now;
      } else {
        // Pas de SSID configuré → mode AP direct
        Serial.println("[WiFi] ⚠️ Pas de SSID configuré, démarrage mode AP");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(kApSsid, kApPassword);
        Serial.printf("[WiFi] 📡 Mode AP par défaut: SSID='%s', IP=%s\n", 
                      kApSsid, WiFi.softAPIP().toString().c_str());
        gWifiState = WIFI_AP_MODE;
        gWifiApFallback = false;
        setupWiFiServices();
      }
      break;
      
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] ✅ Connecté STA, IP=%s\n", WiFi.localIP().toString().c_str());
        Serial.printf("🌐 Seaker ESP32 prêt - Interface web: http://%s\n", WiFi.localIP().toString().c_str());
        gWifiState = WIFI_CONNECTED;
        gWifiRetryCount = 0;
        setupWiFiServices();
      } else if (now - gWifiConnectStart > 15000) {
        // Timeout de 15s → essayer mode AP
        gWifiRetryCount++;
        Serial.printf("[WiFi] ❌ Échec connexion STA (tentative %d), Status=%d\n", gWifiRetryCount, WiFi.status());
        
        if (gWifiRetryCount >= 3) {
          // Après 3 échecs → mode AP permanent
          Serial.printf("[WiFi] 🔴 ÉCHEC: %d tentatives STA échouées → Basculement mode AP FALLBACK\n", gWifiRetryCount);
          WiFi.disconnect();
          WiFi.mode(WIFI_AP);
          WiFi.softAP(kApSsid, kApPassword);
        Serial.printf("[WiFi] 📡 Mode AP FALLBACK actif: SSID='%s', IP=%s (connexion externe possible)\n", 
                      kApSsid, WiFi.softAPIP().toString().c_str());
        Serial.printf("🌐 Seaker ESP32 en mode AP - Interface web: http://%s\n", WiFi.softAPIP().toString().c_str());
          gWifiState = WIFI_AP_MODE;
          gWifiApFallback = true;
          setupWiFiServices();
        } else {
          // Retry après 5s
          Serial.printf("[WiFi] ⏳ Nouvelle tentative STA dans 5s... (%d/%d)\n", gWifiRetryCount, 3);
          gWifiState = WIFI_DISCONNECTED;
        }
      }
      break;
      
    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] ❌ Connexion perdue, reconnexion...");
        gWifiState = WIFI_DISCONNECTED;
        gWifiRetryCount++; // ⚠️ IMPORTANT: Compter les échecs de reconnexion
        Serial.printf("[WiFi] Échec de reconnexion #%d\n", gWifiRetryCount);
      }
      break;
      
    case WIFI_AP_MODE:
      // En mode AP, vérifier si on peut revenir en STA
      if (!gWifiApFallback && gWifiSsid.length() > 0) {
        // Tenter de revenir en STA toutes les 2 minutes
        static unsigned long lastStaAttempt = 0;
        if (now - lastStaAttempt > 120000) {
          Serial.println("[WiFi] Tentative retour mode STA...");
          WiFi.softAPdisconnect();
          WiFi.mode(WIFI_STA);
          gWifiState = WIFI_DISCONNECTED;
          gWifiRetryCount = 0;
          lastStaAttempt = now;
        }
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Booting Seaker ESP32");
  Serial.print("🚀 Firmware v"); Serial.println(FIRMWARE_VERSION);
  Serial.print("📅 Build: "); Serial.println(BUILD_DATE);
  Serial.println("Appuyez 'h' pour l'aide CLI.");

  // I2C (INA219) sur SDA=GPIO21, SCL=GPIO22, avec timeout et recovery si bus bloqué
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  #ifdef ARDUINO
  Wire.setTimeOut(200); // limite l'attente I2C pour éviter un blocage au boot
  #endif
  bool inaOk = gIna219.begin();
  if (!inaOk) {
    logMsg(LOG_WARN, "INA219", "init échouée, tentative recovery I2C...");
    tryI2CBusRecovery(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    #ifdef ARDUINO
    Wire.setTimeOut(200);
    #endif
    inaOk = gIna219.begin();
  }
  if (inaOk) {
    gInaReady = true;
    logMsg(LOG_INFO, "INA219", "capteur détecté");
  } else {
    gInaReady = false;
    logMsg(LOG_WARN, "INA219", "non détecté");
  }

  // Charger log level depuis NVS
  {
    uint8_t lv = loadLogLevelFromPrefs();
    if (lv != 255 && lv <= LOG_DEBUG) gLogLevel = (LogLevel)lv;
  }

  // UARTs
  startSEAKER(SEAKER, 115200, SONAR_RX_PIN, SONAR_TX_PIN);
  gpsBegin(GPS, 921600, GPS_RX_PIN, GPS_TX_PIN);
  // Forcer l'écho NMEA GPS et tenter une auto-détection du baud au boot
  gpsSetEchoRaw(true);
  {
    uint32_t sel = 0;
    if (gpsAutoDetectBaud(sel)) {
      Serial.printf("GPS auto-baud OK: %lu\n", (unsigned long)sel);
    } else {
      Serial.println("GPS auto-baud: échec (921600 conservé)");
    }
  }

  // Charger config WiFi et NTRIP depuis NVS
  loadWifiFromPrefs();
  loadNtripFromPrefs();
  loadSeakerCalibFromPrefs();
  loadSeakerConfigsFromPrefs(); // Charger les profils CONFIG SEAKER

  // Démarrer le WiFi Manager (gestion automatique STA/AP)
  Serial.println("[WiFi] Démarrage WiFi Manager...");
  gWifiState = WIFI_DISCONNECTED;
  gLastWifiCheck = 0; // Force la vérification immédiate
  wifiManagerStep(); // Premier appel pour initier la connexion
  // Lancer la tâche NTRIP sur core 0 (réseau), pour libérer le core 1 (loop)
  xTaskCreatePinnedToCore(ntripTask, "ntrip", 8192, nullptr, 1, &ntripTaskHandle, 0);
  // Lancer la tâche SEAKER sur core 1
  static TaskHandle_t seakerTaskHandle = nullptr;
  xTaskCreatePinnedToCore(seakerTask, "seaker", 4096, nullptr, 1, &seakerTaskHandle, 1);

  // Émettre immédiatement des trames de boot pour vérif moniteur série
  Serial.println("$BOOT,ok*00");
  // Diffusion TCP immédiate d'un BOOT aussi (sans checksum exactement égal mais cohérent via helper)
  broadcastNmea("BOOT,ok");
  printSysFrame();
  printNtripFrame();
  printGpsSummaryFrame();
}

void loop() {
  gpsPoll();
  // SEAKER handled in its own task
  // Mise à jour immédiate sur nouvel écho SEAKER (sans attendre la fenêtre 2s)
  {
    static unsigned long lastPingSeenLive = 0;
    static unsigned long lastTargetPushMs = 0;
    if (gSeaker.pingCounter != lastPingSeenLive) {
      unsigned long nowEv = millis();
      if (nowEv - lastTargetPushMs > 250) { // limiter le débit
        printTargetFrame();
        lastTargetPushMs = nowEv;
      }
      lastPingSeenLive = gSeaker.pingCounter;
    }
  }
  
  // WiFi Manager - surveillance continue et reconnexion automatique
  wifiManagerStep();
  
  consoleLoop();
  webLoop();
  
  // Gérer les connexions GPS Forward TCP
  if (gGpsForwardEnabled) {
    // Accepter les nouvelles connexions
    if (gpsForwardServer.hasClient()) {
      if (!gpsForwardClient || !gpsForwardClient.connected()) {
        gpsForwardClient = gpsForwardServer.available();
        gpsForwardClient.setNoDelay(true);
        Serial.println("[GPS Forward] Client connected");
      } else {
        // Refuser la connexion si déjà un client
        WiFiClient tempClient = gpsForwardServer.available();
        tempClient.stop();
      }
    }
    
    // Vérifier si le client est toujours connecté
    if (gpsForwardClient && !gpsForwardClient.connected()) {
      gpsForwardClient.stop();
      Serial.println("[GPS Forward] Client disconnected");
    }
  }
  
// #ifndef MINIMAL_SERIAL
//   handleTCPClient();
// #endif

  static unsigned long lastPush = 0;
  static unsigned long lastGpsWsMs = 0;
  static unsigned long lastRssiPushMs = 0;
  
  // Push RSSI toutes les 2 secondes via WebSocket
  unsigned long nowRssi = millis();
  if (nowRssi - lastRssiPushMs > 2000) {
    int rssi = WiFi.RSSI();
    String rssiJson = "{\"rssi\":" + String(rssi) + "}";
    wsBroadcastJson(rssiJson);
    // Suppression log RSSI périodique pour éviter flood série
    // Serial.printf("[RSSI] WiFi signal: %d dBm\n", rssi);
    lastRssiPushMs = nowRssi;
  }
  
  // GPS Forward : Envoi à débit constant (1 Hz) pour ROV/Mavlink
  static unsigned long lastGpsForwardMs = 0;
  if (gGpsForwardEnabled && (nowRssi - lastGpsForwardMs > 1000)) {
    sendTargetAsGGA(); // Envoie la dernière position connue même si inchangée
    lastGpsForwardMs = nowRssi;
  }
  
  // if (millis()-lastPush > 200) {
  //   
  //   #ifndef MINIMAL_SERIAL
  //   String js = getTelemetryJson();
  //   wsBroadcastJson(js);
  //   tcpPushTelemetry(js);
  //   #endif
  //   lastPush = millis();
  // }

  // CLI Série simplifié (1-char commands)
  if (Serial.available()) {
    static String lineInput;
    int c = Serial.read();
    switch (c) {
      case 'h':
      case 'H':
        printHelp();
        break;
      case 'p':
      case 'P':
        printFixSummary();
        break;
      case 'r': {
        String raw = gpsGetRaw(50);
        Serial.println("--- RAW ---");
        Serial.print(raw);
        Serial.println("--- END ---");
        break;
      }
      case 'e': {
        bool en = !gpsGetEchoRaw();
        gpsSetEchoRaw(en);
        Serial.printf("Echo RAW: %s\n", en ? "ON" : "OFF");
        break;
      }
      case 's': {
        bool en = !seakerGetEchoRaw();
        seakerSetEchoRaw(en);
        Serial.printf("Echo SEAKER: %s\n", en ? "ON" : "OFF");
        break;
      }
      case '1': {
        sendSEAKERCommand("CONFIG,5,1,1500,0,0,0,0,0,2,1,5,1,0,0,0,0,0,0,0");
        Serial.println("$SEAK,CONFIG,profile1*00");
        break;
      }
      case '2': {
        sendSEAKERCommand("CONFIG,2,1,1500,0,0,0,0,0,2,1,6,1,0,0,0,0,0,0,0");
        Serial.println("$SEAK,CONFIG,profile2*00");
        break;
      }
      case 'b': {
        uint32_t sel = 0;
        if (gpsAutoDetectBaud(sel)) {
          Serial.printf("Auto-baud OK: %lu\n", (unsigned long)sel);
        } else {
          Serial.println("Auto-baud échec");
        }
        break;
      }
      
      case 'v': {
        if (gLogLevel>LOG_ERROR) gLogLevel = (LogLevel)((int)gLogLevel - 1);
        Serial.printf("LogLevel=%d\n", (int)gLogLevel);
        break;
      }
      case 'V': {
        if (gLogLevel<LOG_DEBUG) gLogLevel = (LogLevel)((int)gLogLevel + 1);
        Serial.printf("LogLevel=%d\n", (int)gLogLevel);
        break;
      }
      case 'M': {
        // Lecture d'une ligne jusqu'à CR/LF pour le mountpoint
        Serial.println("Entrez mountpoint NTRIP et validez (CR):");
        unsigned long tstart = millis();
        String mp;
        while (millis()-tstart < 5000) {
          if (Serial.available()) {
            char ch = (char)Serial.read();
            if (ch=='\r' || ch=='\n') { if (mp.length()) break; else continue; }
            mp += ch;
          }
          delay(1);
        }
        mp.trim();
        if (mp.length()) {
          gNtripMount = mp;
          saveNtripToPrefs();
          gNtripReconnectRequested = true;
          Serial.printf("Mountpoint défini: %s\n", gNtripMount.c_str());
          printNtripFrame();
        } else {
          Serial.println("Mountpoint inchangé.");
        }
        break;
      }
      case 'L': {
        // Tentative simple: récupérer la source-table du caster
        if (WiFi.status()!=WL_CONNECTED) { Serial.println("WiFi non connecté"); break; }
        WiFiClient c;
        if (!c.connect(gNtripHost.c_str(), gNtripPort)) { Serial.println("Caster inaccessible"); break; }
        String req = String("GET / HTTP/1.0\r\nHost: ") + gNtripHost + "\r\nUser-Agent: ESP32-Seaker/" + kFirmwareVersion + "\r\n\r\n";
        c.print(req);
        unsigned long t0 = millis(); String resp;
        while (millis()-t0 < 4000) { while (c.available()) resp += (char)c.read(); if (!c.connected()) break; delay(1);} c.stop();
        // Afficher lignes commençant par STR; simplifié
        int pos = 0; int matches = 0;
        while (true) {
          int nl = resp.indexOf('\n', pos);
          if (nl < 0) break;
          String line = resp.substring(pos, nl); line.trim(); pos = nl+1;
          if (line.startsWith("STR")) { Serial.println(line); matches++; }
        }
        if (!matches) Serial.println("Aucune ligne STR trouvée (auth requise ou réponse absente)");
        break;
      }
      case 'I': {
        gSeakerInvertAngle = !gSeakerInvertAngle;
        saveSeakerCalibToPrefs();
        Serial.printf("SEAKER invert=%s\n", gSeakerInvertAngle?"ON":"OFF");
        break;
      }
      case 'O': {
        Serial.println("Entrez offset angle (deg) et validez (CR):");
        unsigned long tstart = millis();
        String s;
        while (millis()-tstart < 5000) {
          if (Serial.available()) {
            char ch = (char)Serial.read();
            if (ch=='\r' || ch=='\n') { if (s.length()) break; else continue; }
            s += ch;
          }
          delay(1);
        }
        s.trim();
        if (s.length()) {
          gSeakerAngleOffsetDeg = s.toFloat();
          while (gSeakerAngleOffsetDeg < 0) gSeakerAngleOffsetDeg += 360.0f;
          while (gSeakerAngleOffsetDeg >= 360.0f) gSeakerAngleOffsetDeg -= 360.0f;
          saveSeakerCalibToPrefs();
          Serial.printf("SEAKER offset=%.1f deg\n", (double)gSeakerAngleOffsetDeg);
        } else {
          Serial.println("Offset inchangé.");
        }
        break;
      }
      case 'A': {
        Serial.println("Entrez sigma angulaire SEAKER (deg) et validez (CR):");
        unsigned long tstart = millis(); String s;
        while (millis()-tstart < 5000) { if (Serial.available()) { char ch=(char)Serial.read(); if (ch=='\r'||ch=='\n'){ if(s.length()) break; else continue;} s+=ch;} delay(1);} s.trim();
        if (s.length()) { gSeakerAngleSigmaDeg = s.toFloat(); saveFilterPrefs(); Serial.printf("SEAKER sigma=%.2f deg\n", (double)gSeakerAngleSigmaDeg);} else { Serial.println("Inchangé."); }
        break;
      }
      case 'R': {
        Serial.println("Entrez erreur relative distance SEAKER (ex: 0.005) et validez (CR):");
        unsigned long tstart = millis(); String s;
        while (millis()-tstart < 5000) { if (Serial.available()) { char ch=(char)Serial.read(); if (ch=='\r'||ch=='\n'){ if(s.length()) break; else continue;} s+=ch;} delay(1);} s.trim();
        if (s.length()) { gSeakerRangeRel = s.toFloat(); saveFilterPrefs(); Serial.printf("SEAKER rangeRel=%.4f\n", (double)gSeakerRangeRel);} else { Serial.println("Inchangé."); }
        break;
      }
      case 'K': {
        Serial.println("Entrez bruit process Kalman aStd (m/s^2) et validez (CR):");
        unsigned long tstart = millis(); String s;
        while (millis()-tstart < 5000) { if (Serial.available()) { char ch=(char)Serial.read(); if (ch=='\r'||ch=='\n'){ if(s.length()) break; else continue;} s+=ch;} delay(1);} s.trim();
        if (s.length()) { gKalmanAccelStd = s.toFloat(); saveFilterPrefs(); Serial.printf("Kalman aStd=%.2f m/s^2\n", (double)gKalmanAccelStd);} else { Serial.println("Inchangé."); }
        break;
      }
      case 'G': {
        Serial.println("Entrez seuil gating Kalman (sigma) et validez (CR):");
        unsigned long tstart = millis(); String s;
        while (millis()-tstart < 5000) { if (Serial.available()) { char ch=(char)Serial.read(); if (ch=='\r'||ch=='\n'){ if(s.length()) break; else continue;} s+=ch;} delay(1);} s.trim();
        if (s.length()) { gKalmanGate = s.toFloat(); saveFilterPrefs(); Serial.printf("Kalman gate=%.1f sigma\n", (double)gKalmanGate);} else { Serial.println("Inchangé."); }
        break;
      }
      default:
        break;
    }
  }

  static unsigned long lastFixOut = 0;
  static unsigned long lastPingSeen = 0;
  if (millis() - lastFixOut > 2000) {
    // Trame système NMEA-like: $SYS,...*CS\r\n
    printSysFrame();
    // Trame NTRIP/RTCM: $NTRIP,...*CS\r\n
    printNtripFrame();
    // Trame GPS synthétique: $GPS,...*CS\r\n
    printGpsSummaryFrame();
    // Trame TARGET uniquement si un nouveau DTPING a été reçu
    if (gSeaker.pingCounter != lastPingSeen) {
      printTargetFrame();
      lastPingSeen = gSeaker.pingCounter;
    }
    if (gInaReady) {
      float busV = gIna219.getBusVoltage_V();
      float shuntV = gIna219.getShuntVoltage_mV() / 1000.0f;
      float current = gIna219.getCurrent_mA();
      float loadV = busV + shuntV;
      String payload = "PWR,";
      payload += String(loadV,2) + "," + String(current,1) + ",busV=" + String(busV,2);
      uint8_t cks = nmeaChecksum(payload);
      char buf[8]; snprintf(buf, sizeof(buf), "*%02X\r\n", cks);
      Serial.print("$"); Serial.print(payload); Serial.print(buf);
      // Suppression log INA219 périodique pour éviter flood série
      // logMsg(LOG_DEBUG, "INA219", "V=%.2fV I=%.1fmA", (double)loadV, (double)current);
    }
  // Push GPS et TargetF via WS avec throttle
  {
    unsigned long nowMs = millis();
    if (nowMs - lastGpsWsMs >= 120) {
      GpsFix f = gpsGetFix();
      static double prevLat = 0, prevLon = 0; static float prevHdg = -999; static bool have=false;
      float currentHdg = isfinite(f.trueHeadingDeg) ? f.trueHeadingDeg : f.headingDeg;
      
      // Push GPS si position OU heading a changé
      if (!have || f.latitude!=prevLat || f.longitude!=prevLon || fabs(currentHdg - prevHdg) > 0.1) {
        String js = String("{\"gps\":{\"valid\":") + String(f.valid?1:0) + 
                   ",\"lat\":" + String(f.latitude,7) + 
                   ",\"lon\":" + String(f.longitude,7) +
                   ",\"hdg\":" + String(isfinite(currentHdg) ? currentHdg : 0.0, 1) + "}}";
        wsBroadcastJson(js);
        prevLat=f.latitude; prevLon=f.longitude; prevHdg=currentHdg; have=true;
        lastGpsWsMs = nowMs;
        Serial.printf("[GPS WS] Heading: %.1f° (true: %.1f, cog: %.1f)\n", currentHdg, 
                     isfinite(f.trueHeadingDeg) ? f.trueHeadingDeg : -999.0f, 
                     isfinite(f.headingDeg) ? f.headingDeg : -999.0f);
      }
      
      // Push aussi le dernier targetF connu périodiquement
      if (!isnan(gTargetFLat) && !isnan(gTargetFLon) && !isnan(gTargetFR95)) {
        static unsigned long lastTargetFWsMs = 0;
        if (nowMs - lastTargetFWsMs >= 250) { // Plus fréquent: toutes les 250ms
          String js = String("{\"targetf\":{\"lat\":") + String(gTargetFLat,7) + ",\"lon\":" + String(gTargetFLon,7) + ",\"r95_m\":" + String(gTargetFR95,2) + "}}";
          wsBroadcastJson(js);
          lastTargetFWsMs = nowMs;
        }
      }
    }
  }
    // résumé [GPS] supprimé (doublon avec $GPS)
    lastFixOut = millis();
  }
}


