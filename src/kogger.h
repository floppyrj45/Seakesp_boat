#pragma once
#include <Arduino.h>

struct KoggerState {
  float depthM = NAN;               // Profondeur (m) issue de DPT ou DBT
  float tempC = NAN;                // Température eau (°C) depuis MTW
  float offsetM = NAN;              // Offset transducteur (m) depuis DPT
  float depthBelowTransducerM = NAN;// Variante si fournie
  unsigned long lastMs = 0;         // Timestamp dernière mise à jour
};

extern KoggerState gKogger;

void startKOGGER(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin);
void pollKOGGER();

// Écho des trames brutes sur le port série pour debug
void koggerSetEchoRaw(bool enable);
bool koggerGetEchoRaw();


