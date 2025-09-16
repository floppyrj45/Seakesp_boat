# 🚤 Seakesp Boat - ESP32 Bundle ROV

**Système de positionnement et télémétrie pour ROV avec GPS RTK et sonar SEAKER**

Firmware complet intégrant GPS haute précision, NTRIP/RTCM, sonar SEAKER, interface web avancée, filtrage Kalman, et streaming temps réel.

## ✨ Fonctionnalités principales

- 📡 **GPS RTK** - Skytraq avec correction NTRIP/RTCM (précision centimétrique)
- 🎯 **Sonar SEAKER** - Positionnement de cibles sous-marines
- 🧮 **Filtrage Kalman 2D** - Fusion GPS/Sonar avec gating intelligent  
- 🌐 **Interface Web moderne** - Dashboard temps réel avec cartes Leaflet
- 📊 **Streaming TCP/WebSocket** - Données temps réel pour QGis/Mavlink
- 🔧 **Configuration complète** - WiFi, NTRIP, calibration sonar
- 📱 **Mode AP/Station** - Auto-reconnexion et fallback intelligent
- ⚡ **OTA Updates** - Mise à jour firmware sans câble
- 🔋 **Monitoring alimentation** - INA219 pour tension/courant

## 🔧 Build et Upload (PlatformIO)

### Installation
```powershell
# Installer PlatformIO Core ou extension VSCode
pip install platformio

# Cloner le projet
git clone https://github.com/floppyrj45/Seakesp_boat.git
cd Seakesp_boat
```

### Build et Upload
```powershell
# Build firmware
pio run -e esp32dev

# Build système de fichiers (interface web)
pio run -e esp32dev -t buildfs

# Upload firmware (adapter COM port)
pio run -e esp32dev -t upload --upload-port COM13

# Upload interface web
pio run -e esp32dev -t uploadfs --upload-port COM13
```

## 🌐 Interface Web & API

### Pages principales
- **`/`** - Dashboard principal avec cartes et télémétrie
- **`/console`** - Console NMEA temps réel 
- **`/map`** - Vue carte complète (Leaflet + OpenSeaMap)
- **`/about`** - Informations système et version

### API REST
- **`/api/reboot`** - Redémarrage système (POST)
- **`/api/wifi`** - Configuration WiFi (GET/POST) 
- **`/api/ntrip`** - Configuration NTRIP (GET/POST)
- **`/api/gps-forward`** - Forward GPS vers ROV (POST)
- **`/api/loglevel`** - Gestion niveau logs (GET/POST)
- **`/api/seaker-configs/*`** - Profils configuration SEAKER

### Streaming temps réel
- **WebSocket `/ws`** - JSON temps réel pour interface web
- **TCP 10110** - Console NMEA pour monitoring
- **TCP 10111** - GPS Forward vers ROV/Mavlink

## ⚡ Accès et Configuration

### Mode Station WiFi
- **SSID**: `SILENT FLOW` 
- **URL**: IP attribuée ou `http://seakesp.local`

### Mode Point d'Accès (fallback)
- **SSID**: `SeakerESP-Config`
- **Mot de passe**: `seaker123`
- **URL**: `http://192.168.4.1`

### Connexion série (monitoring/debug)
- **Baud**: 115200
- **Commandes CLI**: `h` pour aide

## 🔌 Câblage (ESP32 DevKitC-32D)

### Connexions principales
- **GPS SkyTraq (UART1)**: RX=GPIO25, TX=GPIO26, PPS=GPIO4  
- **SEAKER Sonar (UART2)**: RX=GPIO17, TX=GPIO16
- **Alimentation (I2C)**: SDA=GPIO21, SCL=GPIO22 (INA219)

### Pins de configuration
```cpp
// GPS
#define GPS_RX_PIN    25
#define GPS_TX_PIN    26  
#define GPS_PPS_PIN   4

// SEAKER  
#define SONAR_RX_PIN  17
#define SONAR_TX_PIN  16

// I2C Power monitoring
#define I2C_SDA_PIN   21
#define I2C_SCL_PIN   22
```

## 📚 Documentation

### Fichiers de référence
- **`API_ENDPOINTS_AND_TASKS.md`** - Liste complète des endpoints API
- **`CHANGELOG.md`** - Historique des versions et modifications
- **`TEST_COMPLET.md`** - Procédures de test système
- **`docs/TARGET_CALCULATION.md`** - Calculs de positionnement 
- **`docs/Filtering.md`** - Documentation filtrage Kalman
- **`docs/Guide_QGIS.md`** - Intégration QGis temps réel

### Scripts utilitaires
- **`docs/qgis_live_tcp.py`** - Client QGis temps réel
- **`docs/Seaker AS GPS for Mavlink/`** - Bridge Mavlink/ArduPilot
- **`release.bat/sh`** - Scripts de génération de releases

## 🔄 Version actuelle

- **Firmware**: `v2.1.0`  
- **Build**: Voir `/about` dans l'interface web
- **Architecture**: ESP32 DevKitC-32D (Dual Core)

## 🛠️ Développement

### Structure du projet
```
src/                    # Code source principal
├── main.cpp           # Firmware principal  
├── gps_skytraq.cpp    # Driver GPS
├── seaker.cpp         # Driver sonar SEAKER
├── web_server.cpp     # Serveur HTTP/WebSocket
├── target_filter.cpp  # Filtrage Kalman 2D
└── runtime_config.cpp # Configuration persistante

data/                   # Interface web (LittleFS)
├── index.html         # Dashboard principal
├── map.html           # Vue carte
└── console.html       # Console NMEA

docs/                   # Documentation et outils
├── qgis_*.py          # Scripts QGis  
└── Seaker AS GPS for Mavlink/  # Bridge ROV

include/config.h        # Configuration GPIO/hardware
platformio.ini         # Configuration build PlatformIO
```

### Compilation des releases
```powershell
# Générer release complète (firmware + filesystem)
./release.bat

# Fichiers générés dans releases/
# - seaker_esp32_v2.1.0.bin (firmware)
# - littlefs_v2.1.0.bin (interface web)
```

## 🤝 Contribuer

1. Fork le projet
2. Créez une branche (`git checkout -b feature/AmazingFeature`) 
3. Commitez vos changements (`git commit -m 'Add AmazingFeature'`)
4. Push vers la branche (`git push origin feature/AmazingFeature`)
5. Ouvrez une Pull Request

## 📜 Licence

Ce projet est sous licence libre. Voir fichier `LICENSE` pour détails.

## 🔗 Liens utiles

- **Hardware SEAKER**: Documentation technique sonar
- **SkyTraq GPS**: Protocole NMEA et configuration RTK  
- **PlatformIO**: [Documentation ESP32](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- **ESP32**: [Référence technique Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)









