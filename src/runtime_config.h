#pragma once
#include <Arduino.h>

// NTRIP configuration shared between modules
extern String gNtripHost;
extern uint16_t gNtripPort;
extern String gNtripMount;
extern String gNtripUser;
extern String gNtripPass;
extern volatile bool gNtripEnabled;
extern volatile unsigned long gRtcmRxBytes;
extern volatile unsigned long gRtcmFwdBytes;
extern volatile unsigned long gRtcmLastMs;

// Demande de reconnexion NTRIP en cas de changement de configuration
extern volatile bool gNtripReconnectRequested;

// WiFi (dev) simple RAM-only storage
extern String gWifiSsid;
extern String gWifiPass;


// SEAKER calibrations
extern volatile bool gSeakerInvertAngle;     // inverser l'angle (miroir)
extern volatile float gSeakerAngleOffsetDeg; // offset en degrés (ajouté après inversion)

// Filtrage / incertitudes
extern volatile float gSeakerAngleSigmaDeg; // écart-type angulaire SEAKER (deg)
extern volatile float gSeakerRangeRel;      // erreur relative de distance (fraction, ex 0.005 = 0.5%)
extern volatile float gKalmanAccelStd;      // bruit process (m/s^2)
extern volatile float gKalmanGate;          // seuil d'innovation pour rejet (en sigma)
extern volatile bool gTatFilterEnabled;     // activation du filtre TAT

// Persistence helpers (Preferences)
void loadWifiFromPrefs();
void saveWifiToPrefs();
void loadNtripFromPrefs();
void saveNtripToPrefs();
void loadSeakerCalibFromPrefs();
void saveSeakerCalibToPrefs();
void loadFilterPrefs();
void saveFilterPrefs();

// SEAKER configuration frames (payload without '$' and checksum)
extern String gSeakerConfig[4];
void loadSeakerConfigsFromPrefs();
void saveSeakerConfigToPrefs(uint8_t idx);

// Log level persistence
uint8_t loadLogLevelFromPrefs();
void saveLogLevelToPrefs(uint8_t level);


