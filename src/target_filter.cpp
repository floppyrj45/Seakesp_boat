#include "target_filter.h"
#include <math.h>

static inline float fsq(float v){ return v*v; }

void targetFilterInit(TargetFilterState& s, float x, float y, float posStd){
  s.x = x; s.y = y; s.vx = 0.0f; s.vy = 0.0f;
  float r2 = fsq(posStd);
  s.Pxx = r2; s.Pyy = r2; s.Pvvx = 100.0f; s.Pvvy = 100.0f;
  s.Pxy = 0; s.Pxvx = 0; s.Pxvy = 0; s.Pyvx = 0; s.Pyvy = 0; s.Pvxvy = 0;
  s.initialized = true;
}

void targetFilterPredict(TargetFilterState& s, float dt, float aStd){
  if (!s.initialized) return;
  // Modèle déplacement constant
  s.x += s.vx * dt;
  s.y += s.vy * dt;
  float q = sq(aStd);
  // Mise à jour covariance (approx simple, diagonale dominante)
  s.Pxx += s.Pvvx * fsq(dt) + q * fsq(dt*dt*0.5f);
  s.Pyy += s.Pvvy * fsq(dt) + q * fsq(dt*dt*0.5f);
  s.Pvvx += q * dt;
  s.Pvvy += q * dt;
}

float targetFilterUpdate(TargetFilterState& s, float measX, float measY, float posStd){
  if (!s.initialized) { targetFilterInit(s, measX, measY, posStd); return 0.0f; }
  float r2 = fsq(posStd);
  // Innovation
  float yx = measX - s.x;
  float yy = measY - s.y;
  float Sx = s.Pxx + r2;
  float Sy = s.Pyy + r2;
  float Kx = s.Pxx / Sx;
  float Ky = s.Pyy / Sy;
  s.x += Kx * yx;
  s.y += Ky * yy;
  s.Pxx *= (1.0f - Kx);
  s.Pyy *= (1.0f - Ky);
  // Distance normalisée (approx en 2D, sans corrélation croisée)
  float nx = yx / sqrtf(Sx);
  float ny = yy / sqrtf(Sy);
  return sqrtf(nx*nx + ny*ny);
}


