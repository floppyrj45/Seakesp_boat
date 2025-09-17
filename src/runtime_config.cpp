#include "runtime_config.h"

String gNtripHost = "crtk.net";
uint16_t gNtripPort = 2101;
String gNtripMount = "NEAR";
String gNtripUser = "";
String gNtripPass = "";
volatile bool gNtripEnabled = true;
volatile unsigned long gRtcmRxBytes = 0;
volatile unsigned long gRtcmFwdBytes = 0;
volatile unsigned long gRtcmLastMs = 0;
volatile bool gNtripReconnectRequested = false;

String gWifiSsid = "SILENT FLOW";
String gWifiPass = "0607626279";

volatile bool gSeakerInvertAngle = false;
volatile float gSeakerAngleOffsetDeg = 0.0f;
volatile float gSeakerAngleSigmaDeg = 3.0f;
volatile float gSeakerRangeRel = 0.005f; // 0.5%
volatile float gKalmanAccelStd = 0.5f;
volatile float gKalmanGate = 4.0f;
volatile bool gTatFilterEnabled = true;

// UDP target streaming removed

#include <Preferences.h>
static Preferences prefs;

// --- SEAKER CONFIG persistence ---
String gSeakerConfig[4] = {
  "CONFIG,1,1,1500,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0",
  "CONFIG,2,1,1500,0,0,0,0,0,2,1,6,1,0,0,0,0,0,0,0",
  "CONFIG,5,1,1500,0,0,0,0,0,2,1,5,1,0,0,0,0,0,0,0",
  "CONFIG,7,1,1500,0,0,0,0,0,2,1,7,1,0,0,0,0,0,0,0"
};

void loadWifiFromPrefs(){
  prefs.begin("wifi", true);
  if (prefs.isKey("ssid")) gWifiSsid = prefs.getString("ssid");
  if (prefs.isKey("pass")) gWifiPass = prefs.getString("pass");
  prefs.end();
}

void saveWifiToPrefs(){
  prefs.begin("wifi", false);
  prefs.putString("ssid", gWifiSsid);
  prefs.putString("pass", gWifiPass);
  prefs.end();
}

void loadNtripFromPrefs(){
  prefs.begin("ntrip", true);
  if (prefs.isKey("host"))  gNtripHost = prefs.getString("host");
  if (prefs.isKey("port"))  gNtripPort = (uint16_t)prefs.getUShort("port");
  if (prefs.isKey("mount")) gNtripMount = prefs.getString("mount");
  if (prefs.isKey("user"))  gNtripUser = prefs.getString("user");
  if (prefs.isKey("pass"))  gNtripPass = prefs.getString("pass");
  prefs.end();
}

void saveNtripToPrefs(){
  prefs.begin("ntrip", false);
  prefs.putString("host", gNtripHost);
  prefs.putUShort("port", gNtripPort);
  prefs.putString("mount", gNtripMount);
  prefs.putString("user", gNtripUser);
  prefs.putString("pass", gNtripPass);
  prefs.end();
}

void loadSeakerCalibFromPrefs(){
  prefs.begin("seaker", true);
  if (prefs.isKey("inv")) gSeakerInvertAngle = prefs.getBool("inv");
  if (prefs.isKey("ofs")) gSeakerAngleOffsetDeg = prefs.getFloat("ofs");
  prefs.end();
}

void saveSeakerCalibToPrefs(){
  prefs.begin("seaker", false);
  prefs.putBool("inv", gSeakerInvertAngle);
  prefs.putFloat("ofs", gSeakerAngleOffsetDeg);
  prefs.end();
}

void loadFilterPrefs(){
  prefs.begin("filter", true);
  if (prefs.isKey("angsd")) gSeakerAngleSigmaDeg = prefs.getFloat("angsd");
  if (prefs.isKey("rngrel")) gSeakerRangeRel = prefs.getFloat("rngrel");
  if (prefs.isKey("aStd")) gKalmanAccelStd = prefs.getFloat("aStd");
  if (prefs.isKey("gate")) gKalmanGate = prefs.getFloat("gate");
  if (prefs.isKey("tatEn")) gTatFilterEnabled = prefs.getBool("tatEn");
  prefs.end();
}

void saveFilterPrefs(){
  prefs.begin("filter", false);
  prefs.putFloat("angsd", gSeakerAngleSigmaDeg);
  prefs.putFloat("rngrel", gSeakerRangeRel);
  prefs.putFloat("aStd", gKalmanAccelStd);
  prefs.putFloat("gate", gKalmanGate);
  prefs.putBool("tatEn", gTatFilterEnabled);
  prefs.end();
}

// loadTargetUdpFromPrefs/saveTargetUdpToPrefs removed

uint8_t loadLogLevelFromPrefs(){
  prefs.begin("log", true);
  uint8_t lv = prefs.getUChar("level", 255);
  prefs.end();
  return lv;
}

void saveLogLevelToPrefs(uint8_t level){
  prefs.begin("log", false);
  prefs.putUChar("level", level);
  prefs.end();
}

void loadSeakerConfigsFromPrefs(){
  prefs.begin("seakcfg", true);
  for (int i=0;i<4;i++){
    String key = String("cfg") + String(i);
    if (prefs.isKey(key.c_str())) {
      gSeakerConfig[i] = prefs.getString(key.c_str());
    }
  }
  prefs.end();
}

void saveSeakerConfigToPrefs(uint8_t idx){
  if (idx>=4) return;
  prefs.begin("seakcfg", false);
  String key = String("cfg") + String(idx);
  prefs.putString(key.c_str(), gSeakerConfig[idx]);
  prefs.end();
}


