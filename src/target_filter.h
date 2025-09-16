#pragma once
#include <Arduino.h>

struct TargetFilterState {
  // State vector: [x, y, vx, vy] en mètres et m/s (UTM)
  float x, y, vx, vy;
  // Covariance 4x4 compacte (diagonale + croisements essentiels)
  float Pxx, Pyy, Pvvx, Pvvy, Pxy, Pxvx, Pxvy, Pyvx, Pyvy, Pvxvy;
  bool initialized;
};

// Initialise le filtre à partir d'une mesure (x,y) UTM
void targetFilterInit(TargetFilterState& s, float x, float y, float posStd);

// Prédiction avec dt secondes, bruit accélération std aStd (m/s^2)
void targetFilterPredict(TargetFilterState& s, float dt, float aStd);

// Mise à jour avec une mesure (x,y) et écart-type posStd (m)
// Retourne l'innovation (distance normalisée) pour détection d'outlier
float targetFilterUpdate(TargetFilterState& s, float measX, float measY, float posStd);






