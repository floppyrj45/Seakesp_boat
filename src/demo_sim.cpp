#include "demo_sim.h"
#include "gps_skytraq.h"
#include "seaker.h"
#include "runtime_config.h"
#include <math.h>
#include <Preferences.h>

static bool demoEnabled = false;
static bool helloPattern = false; // active le tracé HELLO
static unsigned long t0Ms = 0;

static DemoBoatParams boat = {
  /*speedMps*/ 0.05f,
  /*driftRadiusM*/ 3.0f,
  /*headingNoiseDeg*/ 2.0f,
  /*circlePeriodS*/ 120.0f
};

static DemoRovParams rov = {
  /*distMinM*/ 30.0f,
  /*distMaxM*/ 200.0f,
  /*periodS*/ 180.0f,
  /*sweepDps*/ 1.0f,
  /*angleNoiseDeg*/ 2.0f,
  /*distNoiseM*/ 1.0f,
  /*baseAngleDeg*/ 0.0f
};

// Position bateau de référence (WGS84) - en mer (Atlantique, large Bretagne)
static double boatLat = 47.500000;
static double boatLon = -3.200000;
static float boatHdg = 0.0f;

// Helpers
static float frand(float a, float b){ return a + (b-a) * ((float)random(0,10001) / 10000.0f); }

// Persistence (local Preferences namespace "demo")
static Preferences demoPrefs;

static void sanitizeRovParams(){
  if (rov.distMinM < 5.0f) rov.distMinM = 5.0f;
  if (rov.distMaxM < rov.distMinM + 5.0f) rov.distMaxM = rov.distMinM + 5.0f;
  if (rov.distMaxM > 300.0f) rov.distMaxM = 300.0f;
  if (rov.periodS < 60.0f) rov.periodS = 120.0f;
  if (rov.periodS > 3600.0f) rov.periodS = 3600.0f;
  if (rov.sweepDps < 0.0f) rov.sweepDps = 0.0f;
  if (rov.sweepDps > 2.0f) rov.sweepDps = 2.0f;
}

static void loadParamsFromPrefs(){
  demoPrefs.begin("demo", false);
  if (demoPrefs.isKey("bspeed")) boat.speedMps = demoPrefs.getFloat("bspeed", boat.speedMps);
  if (demoPrefs.isKey("bradius")) boat.driftRadiusM = demoPrefs.getFloat("bradius", boat.driftRadiusM);
  if (demoPrefs.isKey("bhnoise")) boat.headingNoiseDeg = demoPrefs.getFloat("bhnoise", boat.headingNoiseDeg);
  if (demoPrefs.isKey("bcirc")) boat.circlePeriodS = demoPrefs.getFloat("bcirc", boat.circlePeriodS);
  if (demoPrefs.isKey("rmin")) rov.distMinM = demoPrefs.getFloat("rmin", rov.distMinM);
  if (demoPrefs.isKey("rmax")) rov.distMaxM = demoPrefs.getFloat("rmax", rov.distMaxM);
  if (demoPrefs.isKey("rper")) rov.periodS = demoPrefs.getFloat("rper", rov.periodS);
  if (demoPrefs.isKey("rsweep")) rov.sweepDps = demoPrefs.getFloat("rsweep", rov.sweepDps);
  if (demoPrefs.isKey("ranoise")) rov.angleNoiseDeg = demoPrefs.getFloat("ranoise", rov.angleNoiseDeg);
  if (demoPrefs.isKey("rdnoise")) rov.distNoiseM = demoPrefs.getFloat("rdnoise", rov.distNoiseM);
  if (demoPrefs.isKey("rbase")) rov.baseAngleDeg = demoPrefs.getFloat("rbase", rov.baseAngleDeg);
  demoPrefs.end();
  sanitizeRovParams();
}

static void saveParamsToPrefs(){
  demoPrefs.begin("demo", false);
  demoPrefs.putFloat("bspeed", boat.speedMps);
  demoPrefs.putFloat("bradius", boat.driftRadiusM);
  demoPrefs.putFloat("bhnoise", boat.headingNoiseDeg);
  demoPrefs.putFloat("bcirc", boat.circlePeriodS);
  demoPrefs.putFloat("rmin", rov.distMinM);
  demoPrefs.putFloat("rmax", rov.distMaxM);
  demoPrefs.putFloat("rper", rov.periodS);
  demoPrefs.putFloat("rsweep", rov.sweepDps);
  demoPrefs.putFloat("ranoise", rov.angleNoiseDeg);
  demoPrefs.putFloat("rdnoise", rov.distNoiseM);
  demoPrefs.putFloat("rbase", rov.baseAngleDeg);
  demoPrefs.end();
}

void demoInit(){
  t0Ms = millis();
  // Charger paramètres persistés
  loadParamsFromPrefs();
  sanitizeRovParams();
  // Activer mocks
  gpsSetMockEnabled(true);
  seakerSetMock(true, rov.baseAngleDeg, (rov.distMinM + rov.distMaxM) * 0.5f, rov.angleNoiseDeg, rov.distNoiseM, rov.sweepDps != 0.0f, rov.sweepDps);

  // Si flag global active la démo
  if (gDemoEnabled) demoEnabled = true;
}

void demoSetEnabled(bool enabled, bool persist){
  demoEnabled = enabled;
  gDemoEnabled = enabled;
  if (persist) saveDemoToPrefs();
  // Toggle GPS mock
  gpsSetMockEnabled(enabled);
  // Toggle SEAKER mock
  seakerSetMock(enabled, rov.baseAngleDeg, (rov.distMinM+rov.distMaxM)*0.5f, rov.angleNoiseDeg, rov.distNoiseM, rov.sweepDps!=0.0f, rov.sweepDps);
}

bool demoIsEnabled(){ return demoEnabled; }

void demoGetBoatParams(DemoBoatParams& out){ out = boat; }
void demoSetBoatParams(const DemoBoatParams& p, bool persist){ boat = p; if (persist) saveParamsToPrefs(); }

void demoGetRovParams(DemoRovParams& out){ out = rov; }
void demoSetRovParams(const DemoRovParams& p, bool persist){ rov = p; sanitizeRovParams(); if (persist) saveParamsToPrefs(); }

void demoStep(){
  if (!demoEnabled) return;
  unsigned long nowMs = millis();
  float t = (nowMs - t0Ms) / 1000.0f;

  // --- BOAT motion ---
  // Modèle: petite orbite + dérive lente dans une direction (ici nord-est) avec bruit
  // 1) orbite
  double dlat = 0.0, dlon = 0.0;
  if (boat.driftRadiusM > 0.0f && boat.circlePeriodS > 0.1f) {
    float w = 2.0f * PI / boat.circlePeriodS;
    float e = boat.driftRadiusM * sinf(w * t);
    float n = boat.driftRadiusM * cosf(w * t);
    // approx deg conversions near mid-latitude
    dlat += (n / 111320.0);
    dlon += (e / (111320.0 * cos(boatLat * (PI/180.0))));
  }
  // 2) dérive
  float driftDist = boat.speedMps * (nowMs - t0Ms) / 1000.0f;
  double driftN = 0.7071 * driftDist;
  double driftE = 0.7071 * driftDist;
  dlat += (driftN / 111320.0);
  dlon += (driftE / (111320.0 * cos(boatLat * (PI/180.0))));

  double lat = boatLat + dlat;
  double lon = boatLon + dlon;

  // Heading cohérent ~ tangente du cercle + bruit
  float hdg = atan2f((float)driftE, (float)driftN) * 180.0f / PI;
  if (hdg < 0) hdg += 360.0f;
  hdg += frand(-boat.headingNoiseDeg, boat.headingNoiseDeg);
  boatHdg = hdg;

  GpsFix fix;
  fix.valid = true;
  fix.latitude = lat;
  fix.longitude = lon;
  fix.headingDeg = boatHdg;
  fix.trueHeadingDeg = boatHdg;
  fix.speedKnots = boat.speedMps * 1.94384f;
  fix.satellites = 12;
  fix.hdop = 0.9f;
  fix.altitudeM = 0.0f;
  fix.fixQuality = 4; // RTK fix pour une démo stable
  gpsSetMockFix(fix);

  // --- ROV motion (SEAKER) ---
  // Mode "HELLO" paramétrique simple si activé
  float dist, angDeg;
  if (helloPattern) {
    const float seg = fmodf(t, 36.0f); // 6s par lettre + cercle
    float x=0, y=0; // Est, Nord (m)
    auto move = [&](float x0,float y0,float x1,float y1,float s,float e){ float u = (seg-s)/(e-s); u = max(0.0f,min(1.0f,u)); x = x0 + (x1-x0)*u; y = y0 + (y1-y0)*u; };
    if (seg < 6.0f) { // H
      if (seg < 2.0f) move(-40,-40,-40,40,0,2); else if (seg < 4.0f) move(-40,0,40,0,2,4); else move(40,-40,40,40,4,6);
    } else if (seg < 12.0f) { // E
      float s = seg-6.0f; if (s<2) move(-40,40,40,40,0,2); else if (s<3.5) move(-40,40,-40,-40,2,3.5); else if (s<4.5) move(-40,-40,10,-40,3.5,4.5); else if (s<5.5) move(-40,0,10,0,4.5,5.5); else move(-40,40,10,40,5.5,6);
    } else if (seg < 18.0f) { // L
      float s = seg-12.0f; if (s<3) move(-40,40,-40,-40,0,3); else move(-40,-40,30,-40,3,6);
    } else if (seg < 24.0f) { // L
      float s = seg-18.0f; if (s<3) move(-40,40,-40,-40,0,3); else move(-40,-40,30,-40,3,6);
    } else { // O
      float s = seg-24.0f; float th = (s/12.0f)*2.0f*PI; x = 0 + 35.0f*cosf(th); y = 0 + 35.0f*sinf(th);
    }
    dist = sqrtf(x*x + y*y);
    angDeg = atan2f(x, y) * 180.0f/PI; // 0° nord
    dist = max(0.0f, dist + frand(-rov.distNoiseM, rov.distNoiseM));
    angDeg += frand(-rov.angleNoiseDeg, rov.angleNoiseDeg);
  } else {
    // distance(t) sinus entre distMin et distMax
    float mid = 0.5f * (rov.distMinM + rov.distMaxM);
    float amp = 0.5f * (rov.distMaxM - rov.distMinM);
    dist = mid + amp * sinf(2.0f * PI * t / max(rov.periodS, 0.1f));
    dist = max(0.0f, dist + frand(-rov.distNoiseM, rov.distNoiseM));
    angDeg = rov.baseAngleDeg;
  }

  // Mettre à jour le mock SEAKER (sans reset complet)
  seakerUpdateMock(angDeg, dist);
}

void demoSetHelloPattern(bool enabled){ helloPattern = enabled; }
