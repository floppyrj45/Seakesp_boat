#pragma once
#include <Arduino.h>

struct DemoBoatParams {
  float speedMps;           // vitesse de dérive moyenne
  float driftRadiusM;       // rayon de petite orbite (0 => pas de cercle, juste dérive très lente)
  float headingNoiseDeg;    // bruit sur le cap
  float circlePeriodS;      // période de rotation si driftRadiusM>0
};

struct DemoRovParams {
  float distMinM;           // distance minimale au bateau
  float distMaxM;           // distance maximale au bateau
  float periodS;            // période de va-et-vient (in/out)
  float sweepDps;           // balayage angulaire en °/s (0 => angle fixe)
  float angleNoiseDeg;      // bruit sur angle
  float distNoiseM;         // bruit sur distance
  float baseAngleDeg;       // angle de base (si sweepDps=0)
};

// Initialisation et cycle
void demoInit();
void demoStep();

// Paramètres & état
void demoSetEnabled(bool enabled, bool persist);
bool demoIsEnabled();

void demoGetBoatParams(DemoBoatParams& out);
void demoSetBoatParams(const DemoBoatParams& p, bool persist);

void demoGetRovParams(DemoRovParams& out);
void demoSetRovParams(const DemoRovParams& p, bool persist);

// Activer/désactiver le tracé "HELLO" (lettres) pour la cible
void demoSetHelloPattern(bool enabled);


