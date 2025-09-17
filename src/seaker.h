#pragma once
#include <Arduino.h>

struct SeakerState {
  float lastAngle = NAN;
  float lastDistance = NAN;
  String lastStatus;
  volatile unsigned long pingCounter = 0; // incrémenté à chaque $DTPING valide
  volatile unsigned long acceptedPings = 0; // pings acceptés (après filtrage TAT)
  volatile unsigned long rejectedTat = 0;   // pings rejetés par le filtre TAT
  volatile unsigned long tatAcc2s = 0;      // acceptés sur la dernière fenêtre ~2s
  volatile unsigned long tatRej2s = 0;      // rejetés sur la dernière fenêtre ~2s
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





