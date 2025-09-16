#pragma once
#include <Arduino.h>

struct GpsFix {
  bool valid = false;
  double latitude = 0.0;
  double longitude = 0.0;
  float headingDeg = NAN;         // from VTG
  float trueHeadingDeg = NAN;     // from HDT/PASHR
  float speedKnots = NAN;         // from RMC/VTG
  uint16_t satellites = 0;        // from GGA
  float hdop = NAN;               // from GGA
  float altitudeM = NAN;          // from GGA
  float geoidM = NAN;             // from GGA
  uint8_t fixQuality = 0;         // from GGA field 6 (0=invalid,1=GPS,2=DGPS,4=RTK Fix,5=RTK Float)
  float pdop = NAN;               // optional
  float vdop = NAN;               // optional
  String navMode;                 // A/D/E
  uint16_t year = 0;              // UTC
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

void gpsBegin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin);
void gpsPoll();
GpsFix gpsGetFix();
String gpsGetRaw(int maxLines);
void gpsFeedRtcm(const uint8_t* data, size_t len);
void gpsSwapPins();
// Simulation / Mock helpers
void gpsSetMockEnabled(bool enabled);
void gpsSetMockFix(const GpsFix& fix);

// Dev/Debug helpers
bool gpsAutoDetectBaud(uint32_t& selectedBaud);
void gpsSetEchoRaw(bool enabled);
bool gpsGetEchoRaw();


