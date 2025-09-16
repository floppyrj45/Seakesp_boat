#pragma once
#include <Arduino.h>

// Lit la tension (V) et le courant (mA) si INA219 prêt. Retourne true si OK.
bool readPower(float& voltageV, float& current_mA);


