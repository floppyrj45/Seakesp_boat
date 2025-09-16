#pragma once
#include <Arduino.h>

// Dernière cible filtrée (TARGETF) connue
extern volatile double gTargetFLat;
extern volatile double gTargetFLon;
extern volatile float gTargetFR95;
extern volatile unsigned long gTargetFMs;

inline void telemetrySetTargetF(double lat, double lon, float r95){
  gTargetFLat = lat; gTargetFLon = lon; gTargetFR95 = r95; gTargetFMs = millis();
}






