#pragma once

// Brochage ESP32 DevKitC-32D
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

#define GPS_RX_PIN 25
#define GPS_TX_PIN 26
#define GPS_PPS_PIN 4

// SEAKER sur UART2: remappé sur GPIO32 (RX) et GPIO33 (TX)
#define SONAR_RX_PIN 32
#define SONAR_TX_PIN 33

// PM02 non utilisé: ne pas définir de broches

static const char* kFirmwareVersion = "v2.2.2";





