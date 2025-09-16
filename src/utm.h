#pragma once
#include <Arduino.h>

struct UtmCoord {
  int zone;
  bool northHemisphere;
  double easting;
  double northing;
};

// Convertit WGS84 (lat/lon en degrés) vers UTM (auto-zone)
bool wgs84ToUtm(double latDeg, double lonDeg, int& zone, bool& northHemisphere, double& easting, double& northing);

// Convertit UTM -> WGS84 (lat/lon en degrés)
bool utmToWgs84(int zone, bool northHemisphere, double easting, double northing, double& latDeg, double& lonDeg);






