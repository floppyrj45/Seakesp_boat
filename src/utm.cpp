#include "utm.h"
#include <math.h>

// Implémentation compacte d'après l'ellipsoïde WGS84
// Références: Snyder, "Map Projections - A Working Manual"

static const double WGS84_A = 6378137.0;
static const double WGS84_F = 1.0 / 298.257223563;
static const double WGS84_E2 = WGS84_F * (2.0 - WGS84_F);
static const double K0 = 0.9996;

static inline double deg2rad(double d){ return d * (M_PI/180.0); }
static inline double rad2deg(double r){ return r * (180.0/M_PI); }

bool wgs84ToUtm(double latDeg, double lonDeg, int& zone, bool& northHemisphere, double& easting, double& northing) {
  if (!isfinite(latDeg) || !isfinite(lonDeg)) return false;
  if (lonDeg < -180.0 || lonDeg > 180.0 || latDeg < -80.0 || latDeg > 84.0) return false; // plages UTM
  zone = (int)floor((lonDeg + 180.0) / 6.0) + 1;
  if (zone < 1) zone = 1; if (zone > 60) zone = 60;

  northHemisphere = (latDeg >= 0.0);
  double lat = deg2rad(latDeg);
  double lon = deg2rad(lonDeg);
  double lon0 = deg2rad((zone - 1) * 6 - 180 + 3); // méridien central

  double e2 = WGS84_E2;
  double ep2 = e2 / (1.0 - e2);
  double sinLat = sin(lat), cosLat = cos(lat), tanLat = tan(lat);
  double N = WGS84_A / sqrt(1.0 - e2 * sinLat * sinLat);
  double T = tanLat * tanLat;
  double C = ep2 * cosLat * cosLat;
  double A = (lon - lon0) * cosLat;

  double M = WGS84_A * ((1 - e2/4 - 3*e2*e2/64 - 5*e2*e2*e2/256) * lat
    - (3*e2/8 + 3*e2*e2/32 + 45*e2*e2*e2/1024) * sin(2*lat)
    + (15*e2*e2/256 + 45*e2*e2*e2/1024) * sin(4*lat)
    - (35*e2*e2*e2/3072) * sin(6*lat));

  double x = K0 * N * (A + (1 - T + C) * A*A*A / 6.0 + (5 - 18*T + T*T + 72*C - 58*ep2) * A*A*A*A*A / 120.0) + 500000.0;
  double y = K0 * (M + N * tanLat * (A*A/2.0 + (5 - T + 9*C + 4*C*C) * A*A*A*A / 24.0 + (61 - 58*T + T*T + 600*C - 330*ep2) * A*A*A*A*A*A / 720.0));

  if (!northHemisphere) y += 10000000.0; // offset Sud
  easting = x; northing = y;
  return true;
}

bool utmToWgs84(int zone, bool northHemisphere, double easting, double northing, double& latDeg, double& lonDeg) {
  if (zone < 1 || zone > 60) return false;
  double x = easting - 500000.0;
  double y = northing;
  if (!northHemisphere) y -= 10000000.0;

  double e2 = WGS84_E2;
  double ep2 = e2 / (1.0 - e2);
  double lon0 = deg2rad((zone - 1) * 6 - 180 + 3);

  double M = y / K0;
  double mu = M / (WGS84_A * (1 - e2/4 - 3*e2*e2/64 - 5*e2*e2*e2/256));
  double e1 = (1 - sqrt(1 - e2)) / (1 + sqrt(1 - e2));
  double J1 = (3*e1/2 - 27*e1*e1*e1/32);
  double J2 = (21*e1*e1/16 - 55*e1*e1*e1*e1/32);
  double J3 = (151*e1*e1*e1/96);
  double J4 = (1097*e1*e1*e1*e1/512);
  double fp = mu + J1*sin(2*mu) + J2*sin(4*mu) + J3*sin(6*mu) + J4*sin(8*mu);

  double sinfp = sin(fp), cosfp = cos(fp), tanfp = tan(fp);
  double C1 = ep2 * cosfp * cosfp;
  double T1 = tanfp * tanfp;
  double N1 = WGS84_A / sqrt(1 - e2 * sinfp * sinfp);
  double R1 = N1 * (1 - e2) / (1 - e2 * sinfp * sinfp);
  double D = x / (N1 * K0);

  double lat = fp - (N1 * tanfp / R1) * (D*D/2.0 - (5 + 3*T1 + 10*C1 - 4*C1*C1 - 9*ep2) * D*D*D*D/24.0 + (61 + 90*T1 + 298*C1 + 45*T1*T1 - 252*ep2 - 3*C1*C1) * D*D*D*D*D*D/720.0);
  double lon = lon0 + (D - (1 + 2*T1 + C1) * D*D*D/6.0 + (5 - 2*C1 + 28*T1 - 3*C1*C1 + 8*ep2 + 24*T1*T1) * D*D*D*D*D/120.0) / cosfp;

  latDeg = rad2deg(lat);
  lonDeg = rad2deg(lon);
  return true;
}






