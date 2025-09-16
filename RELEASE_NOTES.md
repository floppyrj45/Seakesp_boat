# 🎉 Release v2.0 - ROV ESP32 Seaker

## 🚀 **INSTALLATION RAPIDE**

### 📋 Prérequis
- ESP32-D0WD-V3
- PlatformIO Core 6.12+
- Hardware : GPS + SEAKER + INA219

### ⚡ Flash rapide
```powershell
# Clone du projet
git clone [votre-repo]
cd Bundle-ROV

# Compilation & Upload
pio run -e esp32dev -t uploadfs -t upload --upload-port COM11
```

## 🌟 **NOUVELLES FONCTIONNALITÉS**

### 🎯 **Pour utilisateurs**
- **🌐 Dashboard moderne** : WiFi, batterie, GPS temps réel
- **🗺️ Carte interactive** : Double marqueurs + mode sombre
- **⚙️ Configuration simple** : WiFi, SEAKER profiles inline
- **🔄 OTA Updates** : Mise à jour sans câble

### 🤖 **Pour développeurs ROV**
- **📡 GPS Forward** : Port TCP:10111 → NMEA GGA avec TARGET
- **🛰️ API REST complète** : Telemetry, config, contrôle
- **📊 WebSocket** : Données temps réel (RSSI, positions)
- **💾 Persistance NVS** : Configurations sauvegardées

## 📍 **ENDPOINTS PRINCIPAUX**

### 🌐 **Interface Web**
```
http://[ESP32-IP]/           → Dashboard principal
http://[ESP32-IP]/map        → Carte interactive  
http://[ESP32-IP]/about      → Documentation
http://[ESP32-IP]/update     → Mise à jour OTA
```

### 🔌 **API REST**
```
GET  /api/telemetry          → GPS, SEAKER, power, RSSI
GET  /api/gps-forward        → État TCP forward
POST /api/wifi               → Change SSID/password
GET  /api/seaker-configs     → 4 profils CONFIG
```

### 📡 **TCP Servers**
```
TCP:80    → HTTP/WebServer
TCP:81    → WebSocket temps réel
TCP:10110 → JSON Telemetry (optionnel)
TCP:10111 → GPS Forward (NMEA GGA)
```

## 🎯 **GUIDE ROV INTEGRATION**

### 1️⃣ **Activation GPS Forward**
```javascript
// Via API REST
fetch('http://[ESP32]/api/gps-forward', {
  method: 'POST', 
  body: 'enabled=true'
});
```

### 2️⃣ **Connexion TCP ROV**
```python
import socket
sock = socket.socket()
sock.connect(('[ESP32-IP]', 10111))

while True:
    nmea = sock.recv(1024).decode()
    if nmea.startswith('$GPGGA'):
        # Position TARGET reçue !
        parse_gga(nmea)
```

### 3️⃣ **Configuration SEAKER**
```bash
# Profile pour mission XYZ
curl -X POST "http://[ESP32]/api/seaker-configs" \
  -d "idx=0&payload=CONFIG,1,1,1500,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0"

# Envoi au SEAKER  
curl -X POST "http://[ESP32]/api/seaker-configs/send" \
  -d "idx=0"
```

## 🔍 **LOGS DE DEBUG**

### 📋 **Série (115200 baud)**
```
$BOOT,ok*00
$SYS,uptime=42,rssi=-65dBm,wifi=STA,ip=192.168.1.100*XX
[RSSI] WiFi signal: -65 dBm
[GPS] GGA fixQuality=4 (raw field 6)
$GPS,valid=1,status=FIX,sats=18,lat=47.661227,lon=-2.737588*XX
```

### ✅ **Vérification santé**
- ✅ RSSI visible : `rssi=-XXdBm` dans `$SYS`  
- ✅ GPS status : `status=FIX/FLT/DGP` dans `$GPS`
- ✅ WiFi qualité : Dashboard show "📶 Excellent (-50 dBm)"
- ✅ OTA accessible : `http://[IP]/update` ≠ 404

## ⚠️ **TROUBLESHOOTING**

### 🚨 **Bootloop ESP32**
```powershell
# Effacement complet + re-flash
python -m esptool --chip esp32 --port COM11 erase_flash
pio run -e esp32dev -t uploadfs -t upload --upload-port COM11
```

### 📡 **GPS Forward ne marche pas**
```bash
# Test connexion
telnet [ESP32-IP] 10111
# Doit recevoir des trames GPGGA avec lat/lon TARGET
```

### 🌐 **Pages web 404**
```bash
# Hard refresh navigateur
Ctrl + F5

# Vérifier filesystem
curl http://[ESP32]/about
curl http://[ESP32]/update  
```

## 📊 **PERFORMANCE**

- **⚡ Boot time** : ~5 secondes
- **🔋 Consommation** : 365-410 mA @ 9.8V
- **📡 Latence WiFi** : <50ms (réseau local)
- **🎯 Précision TARGET** : Dépend GPS (RTK: ~2cm, DGPS: ~1m)

## 🏆 **RECORD DE FONCTIONNALITÉS**

Cette version **v2.0** inclut :
- ✅ **8 pages web** complètes
- ✅ **12 endpoints API**  
- ✅ **4 serveurs TCP**
- ✅ **6 trames NMEA** custom
- ✅ **4 profils SEAKER** configurables
- ✅ **Mode sombre** interface
- ✅ **OTA updates** sans fil
- ✅ **ROV integration** native

**🚀 Solution complète de navigation sous-marine avec interface moderne !**



