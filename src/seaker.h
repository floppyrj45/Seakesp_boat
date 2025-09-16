#pragma once
#include <Arduino.h>

struct SeakerState {
  float lastAngle = NAN;
  float lastDistance = NAN;
  String lastStatus;
  volatile unsigned long pingCounter = 0; // incrémenté à chaque $DTPING valide
  // Quelques champs additionnels issus de $STATUS si présents
  float rxFrequency = NAN;
  float snr = NAN;
  float energyTx = NAN;
  float energyRx = NAN;
};

extern SeakerState gSeaker;

void startSEAKER(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin);
void parseSeakerNMEA(const String& line);
void pollSEAKER();
bool sendSEAKERCommand(const String& payload);

// Dev/Debug
void seakerSetEchoRaw(bool enable);
bool seakerGetEchoRaw();

// Mock / Simulation
void seakerSetMock(bool enable, float baseAngleDeg, float baseDistanceM,
                   float noiseAngleDeg = 3.0f, float noiseDistanceM = 1.5f,
                   bool sweep = false, float sweepDegPerSec = 0.0f);





