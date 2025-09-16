# 📋 Documentation complète du système ROV ESP32

## 🌐 ENDPOINTS API & ROUTES WEB

### Pages Web (HTML)
| Route | Description | Handler |
|-------|-------------|---------|
| `/` | Dashboard principal | `handleRoot()` → `index.html` |
| `/map` | Carte interactive | `handleMap()` → `map.html` |
| `/console` | Console NMEA | `handleConsole()` → `console.html` |
| `/about` ou `/about.html` | Page À propos | `handleAbout()` → `about.html` |
| `/ota` | Redirection vers OTA | Redirect → `/update` |
| `/update` | Interface OTA ElegantOTA | `ElegantOTA` (page auto-générée) |
| `/favicon.ico` | Icône du site | Return 204 No Content |

### API REST - Lecture (GET)
| Endpoint | Description | Retour |
|----------|-------------|--------|
| `/api/telemetry` | Télémétrie complète | JSON avec GPS, SEAKER, power, NTRIP, targetF, RSSI, IP, version |
| `/api/targetf` | Position cible filtrée | JSON `{lat, lon, r95_m}` ou 204 si pas de données |
| `/api/wifi` | Config WiFi actuelle | JSON `{ssid}` |
| `/api/seaker-config` | Config correction SEAKER | JSON `{mode, offset, delay}` |
| `/api/gps-forward` | État du GPS Forward | JSON `{enabled, port:10111}` |
| `/api/seaker-configs` | 4 profils CONFIG SEAKER | JSON array avec 4 strings |
| `/api/loglevel` | Niveau de log actuel | JSON `{level}` (ERROR, WARN, LOW, INFO, DEBUG) |

### API REST - Écriture (POST)
| Endpoint | Paramètres | Action |
|----------|------------|--------|
| `/api/wifi` | JSON `{ssid, password}` | Change WiFi et redémarre |
| `/api/seaker-config` | `mode`, `offset`, `delay` | Configure correction distance |
| `/api/gps-forward` | `enabled` (true/false) | Active/désactive GPS forward |
| `/api/seaker-configs` | `idx` (0-3), `payload` | Sauvegarde un profil CONFIG |
| `/api/seaker-configs/send` | `idx` (0-3) | Envoie un profil au SEAKER |
| `/api/loglevel` | `level` (ERROR/WARN/LOW/INFO/DEBUG) | Change niveau de log |
| `/api/reboot` | Aucun paramètre | Redémarre l'ESP32 |

### WebSocket
| URL | Port | Description |
|-----|------|-------------|
| `ws://[IP]:81/` | 81 | WebSocket pour données temps réel |

**Messages WebSocket entrants:**
- `{"cmd":"setLogLevel","level":"DEBUG"}` - Change le niveau de log
- `{"cmd":"seaker","nmea":"..."}` - Envoie commande NMEA au SEAKER

**Messages WebSocket sortants:**
- `{"gps":{...},"targetf":{...}}` - Positions GPS et TARGET
- `{"rssi":-65}` - Signal WiFi toutes les 2 secondes

## 🔄 SERVEURS TCP

| Port | Service | Description |
|------|---------|-------------|
| 80 | HTTP/WebServer | Interface web principale |
| 81 | WebSocket | Données temps réel |
| 10110 | TCP Telemetry | Streaming JSON télémétrie (désactivé par défaut) |
| 10111 | GPS Forward | Retransmission NMEA GGA avec position TARGET |

## 🧠 TÂCHES & PROCESSUS (FreeRTOS)

### Répartition sur les cœurs ESP32

| Tâche | Cœur | Priorité | Stack | Description |
|-------|------|----------|-------|-------------|
| **main/loop()** | Core 1 | 1 (Normal) | Default | Boucle principale Arduino |
| **ntripTask** | Core 0 | 1 (Normal) | 8192 bytes | Client NTRIP pour corrections RTK |
| **seakerTask** | Core 1 | 1 (Normal) | 4096 bytes | Traitement données SEAKER |
| **WiFi/Network** | Core 0 | System | System | Stack réseau ESP32 (automatique) |
| **WebServer** | Core 0/1 | 1 | Shared | Serveur HTTP (appelé depuis loop) |
| **mDNS** | Core 0 | System | System | Service discovery `seakesp.local` |

### Détails des tâches

#### 🌐 **ntripTask** (Core 0)
```cpp
xTaskCreatePinnedToCore(ntripTask, "ntrip", 8192, nullptr, 1, &ntripTaskHandle, 0);
```
- **Fonction**: Se connecte au caster NTRIP et récupère les corrections RTCM
- **Fréquence**: Continue avec délais de 10-1000ms
- **Communication**: Envoie RTCM au GPS via UART

#### 🎯 **seakerTask** (Core 1)
```cpp
xTaskCreatePinnedToCore(seakerTask, "seaker", 4096, nullptr, 1, &seakerTaskHandle, 1);
```
- **Fonction**: Lit et traite les données SEAKER
- **Fréquence**: Continue, délai 10ms
- **Communication**: UART avec le sonar SEAKER

#### 🔄 **loop()** (Core 1)
- **GPS**: Lecture UART et parsing NMEA
- **Power**: Lecture I2C INA219 toutes les secondes
- **WebSocket**: Push RSSI toutes les 2 secondes
- **NMEA Broadcast**: Diffusion des trames système
- **CLI**: Gestion des commandes série
- **GPS Forward**: Gestion des connexions TCP sur port 10111

## 📊 FLUX DE DONNÉES

```
GPS (UART2) ─────┐
                 ├──→ [Core 1: loop()] ──→ Parsing ──→ Telemetry JSON
SEAKER (UART1) ──┘                      │
                                        ├──→ WebSocket (81)
INA219 (I2C) ───────────────────────────┤
                                        ├──→ HTTP API (80)
NTRIP (Core 0) ──→ RTCM ──→ GPS        │
                                        └──→ TCP Forward (10111)
```

## 🔧 CONFIGURATION MATÉRIELLE

| Périphérique | Interface | Pins | Config |
|--------------|-----------|------|--------|
| GPS | UART2 | RX:16, TX:17 | 115200 baud |
| SEAKER | UART1 | RX:32, TX:33 | 115200 baud |
| INA219 | I2C | SDA:21, SCL:22 | Adresse 0x40 |
| WiFi | Internal | - | Mode STA |

## 📝 NMEA CUSTOM FRAMES

| Frame | Description | Exemple |
|-------|-------------|---------|
| `$SYS` | Système | `$SYS,uptime=123456,wifi=STA,ip=192.168.1.100,rssi=-65dBm*CS` |
| `$NTRIP` | État NTRIP | `$NTRIP,enabled=1,streaming=1,rx=1234,fwd=5678,age=2*CS` |
| `$GPS` | Résumé GPS | `$GPS,valid=1,status=FIX,sats=12,lat=47.123456,lon=2.123456*CS` |
| `$SEAKER` | État sonar | `$SEAKER,status=3,angle=45.2,dist=125.3,freq=40000*CS` |
| `$TARGET` | Position brute | `$TARGET,lat=47.123456,lon=2.123456,r95=5.2*CS` |
| `$TARGETF` | Position filtrée | `$TARGETF,lat=47.123456,lon=2.123456,r95=2.1*CS` |
| `$CONFIG` | Profil SEAKER | `$CONFIG,1,1,1500,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0*CS` |

## 🐛 DEBUG - Statut GPS

Le debug ajouté affichera dans le port série :
```
[GPS] GGA fixQuality=4 (raw field 6)
```

Les valeurs possibles :
- `0` = Invalid
- `1` = GPS fix (SPS)
- `2` = DGPS fix
- `3` = PPS fix
- `4` = RTK Fix (centimétrique)
- `5` = RTK Float (décimétrique)
- `6` = Estimated
- `7` = Manual input
- `8` = Simulation

## 🚦 Priorités des tâches FreeRTOS

- **0** = Idle (plus basse)
- **1** = Normal (défaut pour toutes nos tâches)
- **2-4** = Au-dessus de normal
- **5** = Haute priorité
- **configMAX_PRIORITIES-1** = Critique (système)

Les tâches réseau système (WiFi, TCP/IP) ont généralement une priorité plus élevée (2-3) pour garantir la réactivité.


