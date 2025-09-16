#include "console_broadcast.h"
#include <WiFi.h>

static WiFiServer gConsoleServer(10110); // port NMEA standard
static WiFiClient gConsoleClient;
static bool gStarted = false;

static void ensureStarted(){
  if (!gStarted) return;
  if (!gConsoleClient || !gConsoleClient.connected()) {
    WiFiClient c = gConsoleServer.available();
    if (c) {
      gConsoleClient.stop();
      gConsoleClient = c;
      IPAddress rip = gConsoleClient.remoteIP();
      Serial.printf("[TCP10110] client connecté: %s\n", rip.toString().c_str());
    }
  }
}

void consoleBegin(){
  if (!gStarted) { gConsoleServer.begin(); gConsoleServer.setNoDelay(true); gStarted = true; }
}

void consoleBroadcastLine(const String& line){
  if (WiFi.status() != WL_CONNECTED) return;
  ensureStarted();
  if (gConsoleClient && gConsoleClient.connected()) {
    if (line.startsWith("$TARGET") || line.startsWith("$TARGETF") || line.startsWith("$STATUS") || line.startsWith("$DTPING") || line.startsWith("$ACK") || line.startsWith("$SEAK") || line.startsWith("$RET") || line.startsWith("$CONFIG") || line.startsWith("$GOSEAK")) {
      gConsoleClient.print(line);
      gConsoleClient.print("\r\n");
    }
  } else {
    static unsigned long lastWarn = 0;
    unsigned long now = millis();
    if (now - lastWarn > 5000) {
      Serial.println("[TCP10110] aucun client connecté pour diffusion");
      lastWarn = now;
    }
  }
}

void consoleLoop(){
  ensureStarted();
  if (!gConsoleClient || !gConsoleClient.connected()) return;
  while (gConsoleClient.available()) {
    String line = gConsoleClient.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    if (line[0] == '$') {
      int star = line.indexOf('*');
      String payload = (star > 0) ? line.substring(1, star) : line.substring(1);
      extern bool sendSEAKERCommand(const String& payload);
      sendSEAKERCommand(payload);
    }
  }
}


