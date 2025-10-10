#include "web_server.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <ElegantOTA.h>
#include "gps_skytraq.h"
#include "seaker.h"
#include "telemetry_state.h"
#include "power.h"
#include "utm.h"
#include "config.h"
#include "log_iface.h"
#include "runtime_config.h"
#include "demo_sim.h"

// Types from main.cpp
enum SeakerMode { SEAKER_NORMAL, SEAKER_OFFSET, SEAKER_TRANSPONDER };
extern String gNtripHost; extern uint16_t gNtripPort; extern String gNtripMount; extern volatile bool gNtripEnabled; extern volatile unsigned long gRtcmLastMs; 

static WebServer server(80);
static WebSocketsServer ws(81);

static bool gFsReady = false;

static void handleRoot(){
  if (!gFsReady) { server.send(500, "text/plain", "FS not mounted"); return; }
  File f = LittleFS.open("/index.html", "r");
  if (!f) { server.send(404, "text/plain", "index.html not found"); return; }
  server.streamFile(f, "text/html"); f.close();
}

static void handleMap(){
  if (!gFsReady) { server.send(500, "text/plain", "FS not mounted"); return; }
  File f = LittleFS.open("/map.html", "r");
  if (!f) { server.send(404, "text/plain", "map.html not found"); return; }
  server.streamFile(f, "text/html"); f.close();
}

static void handleConsole(){
  if (!gFsReady) { server.send(500, "text/plain", "FS not mounted"); return; }
  File f = LittleFS.open("/console.html", "r");
  if (!f) { server.send(404, "text/plain", "console.html not found"); return; }
  server.streamFile(f, "text/html"); f.close();
}

static void handleAbout(){
  if (!gFsReady) { server.send(500, "text/plain", "FS not mounted"); return; }
  File f = LittleFS.open("/about.html", "r");
  if (!f) { server.send(404, "text/plain", "about.html not found"); return; }
  server.streamFile(f, "text/html"); f.close();
}

extern const char* FIRMWARE_VERSION;
extern const char* BUILD_DATE;

static void handleApiTelemetry(){
  GpsFix f = gpsGetFix();
  String json = "{";
  
  // Version firmware
  json += "\"firmware\":{\"version\":\"" + String(FIRMWARE_VERSION) + "\",\"build\":\"" + String(BUILD_DATE) + "\"},";
  
  json += "\"gps\":{\"valid\":" + String(f.valid?1:0) + ",\"lat\":" + String(f.latitude,7) + ",\"lon\":" + String(f.longitude,7) + ",\"hdg\":" + String(isfinite(f.trueHeadingDeg)?f.trueHeadingDeg:f.headingDeg,1) + ",\"sats\":" + String((unsigned)f.satellites) + ",\"hdop\":" + String(isfinite(f.hdop)?f.hdop:0,1);
  // Ajouter la vitesse en noeuds si disponible
  if (isfinite(f.speedKnots)) { json += ",\"speed_kn\":" + String(f.speedKnots,2); }
  
  // Ajouter le statut GPS (Natural, RTK Fix, RTK Float)
  String gpsStatus = "Natural";
  if (f.fixQuality == 4) gpsStatus = "RTK Fix";
  else if (f.fixQuality == 5) gpsStatus = "RTK Float";
  else if (f.fixQuality == 2) gpsStatus = "DGPS";
  json += ",\"status\":\"" + gpsStatus + "\",\"fixQuality\":" + String(f.fixQuality);
  int z; bool nh; double e,n; if (f.valid && wgs84ToUtm(f.latitude,f.longitude,z,nh,e,n)) {
    json += ",\"utm\":{\"zone\":" + String(z) + ",\"north\":" + String(nh?1:0) + ",\"e\":" + String(e,2) + ",\"n\":" + String(n,2) + "}";
  }
  json += "},";
  // SEAKER with nulls for non-finite
  json += "\"seaker\":{\"status\":\"" + gSeaker.lastStatus + "\",\"angle\":";
  if (isfinite(gSeaker.lastAngle)) json += String(gSeaker.lastAngle,1); else json += "null";
  json += ",\"dist\":";
  if (isfinite(gSeaker.lastDistance)) json += String(gSeaker.lastDistance,1); else json += "null";
  json += ",\"rx_freq\":";
  if (isfinite(gSeaker.rxFrequency)) json += String(gSeaker.rxFrequency,1); else json += "null";
  // Ajouter statistiques TAT
  json += ",\"tat\":{\"acc2s\":" + String((unsigned long)gSeaker.tatAcc2s) + ",\"rej2s\":" + String((unsigned long)gSeaker.tatRej2s) + ",\"accTot\":" + String((unsigned long)gSeaker.acceptedPings) + ",\"rejTot\":" + String((unsigned long)gSeaker.rejectedTat) + ",\"enabled\":" + String(gTatFilterEnabled?1:0) + "}";
  json += "},";
  {
    float v=0,i=0; if (readPower(v,i)) {
      json += "\"power\":{\"voltage\":" + String(v,2) + ",\"current_mA\":" + String(i,0) + "},";
    }
  }
  // NTRIP
  unsigned long now=millis(); bool streaming = (gRtcmLastMs!=0) && (now - gRtcmLastMs < 5000);
  json += "\"ntrip\":{\"enabled\":" + String(gNtripEnabled?1:0) + ",\"host\":\"" + gNtripHost + "\",\"port\":" + String((unsigned)gNtripPort) + ",\"mount\":\"" + gNtripMount + "\",\"streaming\":" + String(streaming?1:0) + "}";
  // TargetF
  if (!isnan(gTargetFLat) && !isnan(gTargetFLon) && !isnan(gTargetFR95)) {
    json += ",\"targetf\":{\"lat\":" + String(gTargetFLat,7) + ",\"lon\":" + String(gTargetFLon,7) + ",\"r95_m\":" + String(gTargetFR95,2);
    int tz; bool tnh; double te,tn; if (wgs84ToUtm(gTargetFLat,gTargetFLon,tz,tnh,te,tn)) {
      json += ",\"utm\":{\"zone\":" + String(tz) + ",\"north\":" + String(tnh?1:0) + ",\"e\":" + String(te,2) + ",\"n\":" + String(tn,2) + "}";
    }
    json += "}";
  }
  // IP locale, WiFi & version
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  
  // Obtenir le RSSI correctement selon le mode
  int rssiValue = 0;
  if (WiFi.status() == WL_CONNECTED) {
    rssiValue = WiFi.RSSI();
    // Si RSSI est 0 ou 31 (valeurs invalides), mettre une valeur par défaut
    if (rssiValue == 0 || rssiValue == 31) {
      rssiValue = -65; // Valeur par défaut "moyenne"
    }
  }
  
  json += ",\"rssi\":" + String(rssiValue);
  json += ",\"version\":\""; json += kFirmwareVersion; json += "\"";
  json += "}";
  
  // Debug RSSI
  Serial.printf("[API] RSSI: %d dBm, WiFi Status: %d\n", rssiValue, WiFi.status());
  
  // Debug: log targetF pour diagnostic
  if (!isnan(gTargetFLat) && !isnan(gTargetFLon)) {
    Serial.printf("[API] TargetF: lat=%.7f lon=%.7f r95=%.2f\n", gTargetFLat, gTargetFLon, gTargetFR95);
  }
  
  server.send(200, "application/json", json);
}

void webSetup(){
  gFsReady = LittleFS.begin(true);
  
  server.on("/", handleRoot);
  server.on("/map", handleMap);
  server.on("/console", handleConsole);
  server.on("/about", handleAbout);
  server.on("/about.html", handleAbout);
  // Alias pratique pour l'OTA
  server.on("/ota", [](){ server.sendHeader("Location", "/update", true); server.send(302, "text/plain", ""); });
  server.on("/api/telemetry", handleApiTelemetry);
  // API DEMO
  server.on("/api/demo", HTTP_GET, [](){
    DemoBoatParams bp; DemoRovParams rp;
    demoGetBoatParams(bp); demoGetRovParams(rp);
    String json = String("{") +
      "\"enabled\":" + String(demoIsEnabled()?"true":"false") +
      ",\"boat\":{\"speed_mps\":" + String(bp.speedMps,2) +
      ",\"radius_m\":" + String(bp.driftRadiusM,1) +
      ",\"heading_noise_deg\":" + String(bp.headingNoiseDeg,1) +
      ",\"circle_period_s\":" + String(bp.circlePeriodS,1) + "}" +
      ",\"rov\":{\"dist_min\":" + String(rp.distMinM,1) +
      ",\"dist_max\":" + String(rp.distMaxM,1) +
      ",\"period_s\":" + String(rp.periodS,1) +
      ",\"sweep_dps\":" + String(rp.sweepDps,1) +
      ",\"angle_noise_deg\":" + String(rp.angleNoiseDeg,1) +
      ",\"dist_noise_m\":" + String(rp.distNoiseM,1) +
      ",\"base_angle_deg\":" + String(rp.baseAngleDeg,1) + "}}";
    server.send(200, "application/json", json);
  });
  server.on("/api/demo", HTTP_POST, [](){
    bool changed = false;
    if (server.hasArg("enabled")) {
      String v = server.arg("enabled");
      bool en = (v == "true" || v == "1");
      demoSetEnabled(en, true);
      changed = true;
    }
    DemoBoatParams bp; DemoRovParams rp; demoGetBoatParams(bp); demoGetRovParams(rp);
    if (server.hasArg("boat.speed_mps")) { bp.speedMps = server.arg("boat.speed_mps").toFloat(); changed=true; }
    if (server.hasArg("boat.radius_m")) { bp.driftRadiusM = server.arg("boat.radius_m").toFloat(); changed=true; }
    if (server.hasArg("boat.heading_noise_deg")) { bp.headingNoiseDeg = server.arg("boat.heading_noise_deg").toFloat(); changed=true; }
    if (server.hasArg("boat.circle_period_s")) { bp.circlePeriodS = server.arg("boat.circle_period_s").toFloat(); changed=true; }
    if (server.hasArg("rov.dist_min")) { rp.distMinM = server.arg("rov.dist_min").toFloat(); changed=true; }
    if (server.hasArg("rov.dist_max")) { rp.distMaxM = server.arg("rov.dist_max").toFloat(); changed=true; }
    if (server.hasArg("rov.period_s")) { rp.periodS = server.arg("rov.period_s").toFloat(); changed=true; }
    if (server.hasArg("rov.sweep_dps")) { rp.sweepDps = server.arg("rov.sweep_dps").toFloat(); changed=true; }
    if (server.hasArg("rov.angle_noise_deg")) { rp.angleNoiseDeg = server.arg("rov.angle_noise_deg").toFloat(); changed=true; }
    if (server.hasArg("rov.dist_noise_m")) { rp.distNoiseM = server.arg("rov.dist_noise_m").toFloat(); changed=true; }
    if (server.hasArg("rov.base_angle_deg")) { rp.baseAngleDeg = server.arg("rov.base_angle_deg").toFloat(); changed=true; }
    if (changed) {
      demoSetBoatParams(bp, true);
      demoSetRovParams(rp, true);
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });
  // API simple pour activer/désactiver le filtre TAT
  server.on("/api/tat-filter", HTTP_GET, [](){
    String j = String("{\"enabled\":") + String(gTatFilterEnabled?"true":"false") + "}";
    server.send(200, "application/json", j);
  });
  server.on("/api/tat-filter", HTTP_POST, [](){
    if (server.hasArg("enabled")){
      String val = server.arg("enabled");
      gTatFilterEnabled = (val == "true" || val == "1");
      saveFilterPrefs();
      String j = String("{\"status\":\"ok\",\"enabled\":") + String(gTatFilterEnabled?"true":"false") + "}";
      server.send(200, "application/json", j);
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing enabled parameter\"}");
    }
  });
  server.on("/api/targetf", [](){
    if (!isnan(gTargetFLat) && !isnan(gTargetFLon) && !isnan(gTargetFR95)){
      String j = String("{\"lat\":") + String(gTargetFLat,7) + ",\"lon\":" + String(gTargetFLon,7) + ",\"r95_m\":" + String(gTargetFR95,2) + "}";
      server.send(200, "application/json", j);
    } else {
      server.send(204, "application/json", "{}");
    }
  });
  
  // API pour obtenir les paramètres WiFi actuels
  server.on("/api/wifi", HTTP_GET, [](){
    extern String gWifiSsid;
    // Escaper les caractères spéciaux pour JSON
    String escapedSsid = gWifiSsid;
    escapedSsid.replace("\\", "\\\\");
    escapedSsid.replace("\"", "\\\"");
    String json = "{\"ssid\":\"" + escapedSsid + "\"}";
    server.send(200, "application/json", json);
  });
  
  // API pour changer les paramètres WiFi
  server.on("/api/wifi", HTTP_POST, [](){
    extern String gWifiSsid;
    extern String gWifiPass;
    extern void saveWifiToPrefs();
    
    // Gérer à la fois les arguments URL et le body JSON
    String newSsid = "";
    String newPass = "";
    
    if (server.hasArg("plain")) {
      // Body JSON
      String body = server.arg("plain");
      // Parse simple du JSON
      int ssidIdx = body.indexOf("\"ssid\":");
      int passIdx = body.indexOf("\"password\":");
      
      if (ssidIdx >= 0 && passIdx >= 0) {
        int ssidStart = body.indexOf('\"', ssidIdx + 7) + 1;
        int ssidEnd = body.indexOf('\"', ssidStart);
        int passStart = body.indexOf('\"', passIdx + 11) + 1;
        int passEnd = body.indexOf('\"', passStart);
        
        if (ssidEnd > ssidStart && passEnd > passStart) {
          newSsid = body.substring(ssidStart, ssidEnd);
          newPass = body.substring(passStart, passEnd);
        }
      }
    } else if (server.hasArg("ssid") && server.hasArg("password")) {
      // Arguments URL
      newSsid = server.arg("ssid");
      newPass = server.arg("password");
    }
    
    if (newSsid.length() > 0) {
      gWifiSsid = newSsid;
      gWifiPass = newPass;
      saveWifiToPrefs();
      
      server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"WiFi updated. Rebooting...\"}");
      
      // Redémarrer après 1 seconde
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing ssid or password\"}");
    }
  });
  
  // API pour configurer la correction de distance SEAKER
  server.on("/api/seaker-config", HTTP_GET, [](){
    extern SeakerMode gSeakerMode;
    extern float gSeakerDistOffset;
    extern float gSeakerTransponderDelay;
    extern volatile bool gSeakerInvertAngle;
    
    String mode = "normal";
    if (gSeakerMode == SEAKER_OFFSET) mode = "offset";
    else if (gSeakerMode == SEAKER_TRANSPONDER) mode = "transponder";
    
    String json = "{\"mode\":\"" + mode + "\",\"offset\":" + String(gSeakerDistOffset,1) + 
                  ",\"delay\":" + String(gSeakerTransponderDelay,1) + 
                  ",\"invert\":" + String(gSeakerInvertAngle ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });
  
  server.on("/api/seaker-config", HTTP_POST, [](){
    extern SeakerMode gSeakerMode;
    extern float gSeakerDistOffset;
    extern float gSeakerTransponderDelay;
    extern volatile bool gSeakerInvertAngle;
    extern void saveSeakerCalibToPrefs();
    
    if (server.hasArg("mode")) {
      String mode = server.arg("mode");
      if (mode == "normal") gSeakerMode = SEAKER_NORMAL;
      else if (mode == "offset") gSeakerMode = SEAKER_OFFSET;
      else if (mode == "transponder") gSeakerMode = SEAKER_TRANSPONDER;
    }
    
    if (server.hasArg("offset")) {
      gSeakerDistOffset = server.arg("offset").toFloat();
    }
    
    if (server.hasArg("delay")) {
      gSeakerTransponderDelay = server.arg("delay").toFloat();
    }
    
    if (server.hasArg("invert")) {
      String invertStr = server.arg("invert");
      gSeakerInvertAngle = (invertStr == "true");
      saveSeakerCalibToPrefs(); // Sauvegarder l'inversion d'angle
    }
    
    Serial.printf("[SEAKER Config] Mode: %d, Offset: %.1f m, Delay: %.1f ms, Invert: %s\n", 
                  gSeakerMode, gSeakerDistOffset, gSeakerTransponderDelay, 
                  gSeakerInvertAngle ? "true" : "false");
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });
  
  // API pour contrôler le GPS Forward
  server.on("/api/gps-forward", HTTP_GET, [](){
    extern volatile bool gGpsForwardEnabled;
    String json = "{\"enabled\":" + String(gGpsForwardEnabled ? "true" : "false") + ",\"port\":10111}";
    server.send(200, "application/json", json);
  });

  // API pour gérer les profils CONFIG SEAKER
  server.on("/api/seaker-configs", HTTP_GET, [](){
    extern String gSeakerConfig[4];
    String json = "[";
    for (int i=0;i<4;i++){
      if (i) json += ",";
      String escaped = gSeakerConfig[i];
      escaped.replace("\\", "\\\\");
      escaped.replace("\"", "\\\"");
      json += "\"" + escaped + "\"";
    }
    json += "]";
    server.send(200, "application/json", json);
  });
  server.on("/api/seaker-configs", HTTP_POST, [](){
    extern String gSeakerConfig[4];
    extern void saveSeakerConfigToPrefs(uint8_t idx);
    if (!server.hasArg("idx") || !server.hasArg("payload")){
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing idx or payload\"}");
      return;
    }
    int idx = server.arg("idx").toInt();
    if (idx<0 || idx>3) { server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"idx out of range\"}"); return; }
    String payload = server.arg("payload");
    // Nettoyer: enlever '$' et checksum éventuel
    if (payload.length()>0 && payload[0]=='$') {
      int star = payload.indexOf('*');
      payload = (star>0)? payload.substring(1,star) : payload.substring(1);
    }
    gSeakerConfig[idx] = payload;
    saveSeakerConfigToPrefs((uint8_t)idx);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  });
  // Envoyer un profil CONFIG vers le SEAKER
  server.on("/api/seaker-configs/send", HTTP_POST, [](){
    if (!server.hasArg("idx")) { server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing idx\"}"); return; }
    int idx = server.arg("idx").toInt();
    extern String gSeakerConfig[4];
    if (idx<0 || idx>3) { server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"idx out of range\"}"); return; }
    String p = gSeakerConfig[idx];
    sendSEAKERCommand(p);
    server.send(200, "application/json", "{\"status\":\"sent\"}");
  });
  
  server.on("/api/gps-forward", HTTP_POST, [](){
    extern volatile bool gGpsForwardEnabled;
    if (server.hasArg("enabled")) {
      String val = server.arg("enabled");
      gGpsForwardEnabled = (val == "true" || val == "1");
      Serial.printf("[GPS Forward] %s\n", gGpsForwardEnabled ? "Enabled" : "Disabled");
      server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing enabled parameter\"}");
    }
  });
  
  // API pour redémarrer le système
  server.on("/api/reboot", HTTP_POST, [](){
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"System rebooting...\"}");
    Serial.println("[REBOOT] System restart requested via web API");
    delay(1000);
    ESP.restart();
  });
  
  // API pour obtenir le niveau de log actuel
  server.on("/api/loglevel", HTTP_GET, [](){
    String currentLevel = getLogLevelName();
    String json = "{\"level\":\"" + currentLevel + "\"}";
    server.send(200, "application/json", json);
  });
  
  // API pour changer le niveau de log
  server.on("/api/loglevel", HTTP_POST, [](){
    if (server.hasArg("level")) {
      String newLevel = server.arg("level");
      Serial.printf("[LogLevel] HTTP API: changing level to '%s'\n", newLevel.c_str());
      setLogLevelByName(newLevel, true);
      String currentLevel = getLogLevelName();
      Serial.printf("[LogLevel] HTTP API: level changed to '%s' successfully\n", currentLevel.c_str());
      String json = "{\"status\":\"ok\",\"level\":\"" + currentLevel + "\"}";
      server.send(200, "application/json", json);
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing level parameter\"}");
    }
  });
  
  // Handler pour favicon.ico pour éviter les erreurs 404
  server.on("/favicon.ico", [](){
    server.send(204, "image/x-icon", "");
  });
  
  // Configuration d'ElegantOTA AVANT onNotFound
  ElegantOTA.begin(&server);
  ElegantOTA.setAutoReboot(true); // Redémarrage automatique après mise à jour
  
  // Handler pour toutes les autres requêtes non gérées (APRÈS ElegantOTA)
  server.onNotFound([](){
    // Essayer de servir un fichier HTML correspondant automatiquement
    if (!gFsReady) { 
      server.send(500, "text/plain", "FS not mounted"); 
      return; 
    }
    
    String uri = server.uri();
    if (uri.length() == 0) { 
      server.send(404, "text/plain", "Not found"); 
      return; 
    }
    
    // Essayer d'abord le chemin tel quel
    String path = uri;
    if (!path.startsWith("/")) path = "/" + path;
    
    // Si le fichier existe tel quel, le servir
    if (LittleFS.exists(path)) {
      File f = LittleFS.open(path, "r");
      if (f) {
        String contentType = "text/html";
        if (path.endsWith(".css")) contentType = "text/css";
        else if (path.endsWith(".js")) contentType = "application/javascript";
        else if (path.endsWith(".json")) contentType = "application/json";
        server.streamFile(f, contentType);
        f.close();
        return;
      }
    }
    
    // Si pas trouvé et pas d'extension, essayer avec .html
    if (path.indexOf('.') == -1) {
      path += ".html";
      if (LittleFS.exists(path)) {
        File f = LittleFS.open(path, "r");
        if (f) {
          server.streamFile(f, "text/html");
          f.close();
          return;
        }
      }
    }
    
    server.send(404, "text/plain", String("Not found: ") + uri);
  });
  
  server.begin();
  ws.begin();
  ws.onEvent([](uint8_t c, WStype_t t, uint8_t * p, size_t l){
    if (t == WStype_TEXT && p && l){
      String s; s.reserve(l); for(size_t i=0;i<l;i++) s += (char)p[i]; s.trim();
      if (s.length()==0) return;
      // Très simple parse
      if (s.indexOf("\"cmd\":\"setLogLevel\"")>=0){
        int k = s.indexOf("\"level\":"); if (k>=0){
          int q1 = s.indexOf('"', k+8); int q2 = s.indexOf('"', q1+1);
          if (q1>=0 && q2>q1){ 
            String lvl = s.substring(q1+1, q2); 
            Serial.printf("[LogLevel] WebSocket: changing level to '%s'\n", lvl.c_str());
            setLogLevelByName(lvl, true); 
            Serial.printf("[LogLevel] WebSocket: level changed to '%s' successfully\n", getLogLevelName().c_str());
          }
        }
      } else if (s.indexOf("\"cmd\":\"seaker\"")>=0){
        int k = s.indexOf("\"nmea\":"); if (k>=0){ int q1 = s.indexOf('"', k+7); int q2 = s.indexOf('"', q1+1); if (q1>=0 && q2>q1){ String nmea = s.substring(q1+1,q2); int star = nmea.indexOf('*'); String payload = (star>0)? nmea.substring(1,star): nmea.substring(1); sendSEAKERCommand(payload); } }
      }
    }
  });
}

void webLoop(){
  server.handleClient();
  ws.loop();
  ElegantOTA.loop(); // Gestion des mises à jour OTA
}

void wsBroadcastJson(String json){ ws.broadcastTXT(json); }


