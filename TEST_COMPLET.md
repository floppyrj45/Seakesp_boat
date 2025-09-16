# 🔬 PROGRAMME DE TEST COMPLET ROV ESP32

## ⚠️ PROBLÈME IDENTIFIÉ
Les logs montrent que la version compilée n'inclut PAS les dernières modifications :
- **Pas de RSSI** dans la trame `$SYS` (devrait avoir `rssi=-XXdBm`)
- **Pas de log `[RSSI]`** toutes les 2 secondes
- Probablement d'autres fonctionnalités manquantes

## 📋 ÉTAPES DE TEST

### 1. 🔄 COMPILATION FRAÎCHE
```powershell
# IMPORTANT : Recompiler TOUT
pio run -e esp32dev -t clean
pio run -e esp32dev -t build
```

### 2. 📤 UPLOAD COMPLET
```powershell
# Upload firmware ET filesystem
pio run -e esp32dev -t uploadfs --upload-port COM11
pio run -e esp32dev -t upload --upload-port COM11
```

### 3. 🖥️ TEST SERIAL (Port COM11, 115200 baud)

#### ✅ Ce que vous DEVEZ voir :
```
$BOOT,ok*00
$SYS,uptime=1,rtcm_rx=0,rtcm_fwd=0,rtcm_age_ms=0,wifi=STA,ip=192.168.0.117,rssi=-65dBm*XX
[RSSI] WiFi signal: -65 dBm    <-- TOUTES LES 2 SECONDES
[GPS] GGA fixQuality=4 (raw field 6)  <-- QUAND GPS REÇU
```

#### ❌ Si vous voyez :
```
$SYS,uptime=34,rtcm_rx=33318,rtcm_fwd=33318,rtcm_age_ms=749,wifi=STA,ip=192.168.0.117*1C
```
➡️ **PAS de rssi** = Ancienne version ! Recompiler !

### 4. 🌐 TEST WEB (http://192.168.0.117 ou http://seakesp.local)

#### A. Dashboard (/)
- [ ] **Signal WiFi** affiché : "📶 Excellent (-50 dBm)"
- [ ] **GPS Status** badge : "RTK FIX" (vert) ou "RTK FLOAT" (orange)
- [ ] **Bouton GPS Forward** : Cliquable ON/OFF
- [ ] **Profils CONFIG** : 4 champs visibles avec boutons 💾 📡

#### B. Liens à tester
- [ ] **📖 À propos** → `/about` : Doit afficher la page
- [ ] **🔄 Mise à jour OTA** → `/update` : Interface ElegantOTA
- [ ] **🗺️ Carte** → `/map` : Carte avec marqueurs

### 5. 🔍 TEST API (curl ou navigateur)

```powershell
# Test RSSI dans telemetry
curl http://192.168.0.117/api/telemetry

# Doit contenir : "rssi":-65

# Test GPS Forward
curl http://192.168.0.117/api/gps-forward
# Réponse : {"enabled":false,"port":10111}

# Test configs SEAKER
curl http://192.168.0.117/api/seaker-configs
# Réponse : ["CONFIG,1,1,1500...", ...]
```

### 6. 🎯 TEST GPS FORWARD

```powershell
# Activer GPS Forward
curl -X POST "http://192.168.0.117/api/gps-forward?enabled=true"

# Vérifier port 10111
netstat -an | findstr 10111
# Doit montrer : TCP    192.168.0.117:10111    0.0.0.0:0    LISTENING
```

### 7. 📡 TEST WEBSOCKET

Ouvrir la console du navigateur (F12) sur la carte :
```javascript
// Doit voir dans la console :
// RSSI reçu: -65
// Cible filtrée mise à jour: {lat: 47.xxx, lon: -2.xxx}
```

## 🔧 SOLUTIONS RAPIDES

### Si OTA ne marche pas :
1. Vérifier que `/update` retourne bien une page (pas 404)
2. Si 404 : Le firmware n'a pas ElegantOTA compilé

### Si About ne marche pas :
1. Vérifier que le fichier existe : `data/about.html`
2. Tester : http://192.168.0.117/about.html (avec .html)

### Si GPS Forward ne marche pas :
1. Vérifier dans les logs série : `[GPS Forward] Enabled`
2. Tester avec telnet : `telnet 192.168.0.117 10111`

### Si RSSI n'apparaît pas :
1. **PROBLÈME PRINCIPAL** : Firmware pas à jour !
2. Solution : Recompiler et re-uploader

## 🚨 DIAGNOSTIC RAPIDE

Tapez 'h' dans le terminal série, vous devez voir :
```
=== Aide CLI ===
h/H: Afficher cette aide
[...]
```

Si les commandes ne correspondent pas à la doc → **Mauvaise version !**

## ✅ CHECKLIST FINALE

- [ ] RSSI visible dans `$SYS` et log `[RSSI]`
- [ ] GPS status correct (RTK FIX/FLOAT/DGPS)
- [ ] `/about` accessible
- [ ] `/update` pour OTA accessible
- [ ] GPS Forward activable
- [ ] Profils CONFIG visibles
- [ ] WebSocket envoie RSSI
- [ ] Marqueurs gris TARGET sur carte

**SI UN SEUL ITEM ÉCHOUE → RECOMPILER !**



