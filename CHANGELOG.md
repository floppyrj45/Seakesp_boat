# 🚀 CHANGELOG - ROV ESP32 Seaker v2.0

## 🆕 Version 2.0 - Septembre 2025

### ✨ Nouvelles fonctionnalités majeures

#### 🌐 **Interface Web améliorée**
- **📊 Signal WiFi RSSI** : Affichage de la qualité WiFi (dBm) dans le dashboard et sur la carte
- **🔄 Mise à jour OTA** : Interface ElegantOTA accessible via `/update`
- **📖 Page À propos** : Documentation complète du projet accessible via `/about`
- **⚙️ Configuration WiFi** : Modification SSID/mot de passe directement depuis le dashboard

#### 🎯 **SEAKER amélioré**
- **📝 Profils CONFIG** : 4 profils SEAKER configurables et sauvegardables en NVS
- **🔧 Correction distance** : Mode Offset et Transponder pour ajuster la distance SEAKER
- **🎨 Interface CONFIG** : Panneau graphique pour éditer et envoyer les profils

#### 🛰️ **GPS/Navigation**
- **🚦 Statut GPS correct** : Affichage RTK Fix/Float/DGPS/Natural au lieu de "Natural" uniquement  
- **📡 GPS Forward TCP** : Retransmission NMEA GGA avec position TARGET sur port 10111
- **🎯 Points TARGET doubles** : Marqueur gris (TARGET brut) + rouge (TARGETF filtré) sur la carte
- **🌙 Mode sombre carte** : Toggle jour/nuit pour la carte marine

#### ⚡ **Performance & UX**
- **🔋 Batterie optimisée** : Jauge max 11.2V avec estimation temps restant moyennée
- **📍 Coordonnées overlay** : TARGETF en temps réel (bas droite carte)
- **📶 WiFi overlay** : Indicateur signal avec barres animées (bas gauche carte)
- **🎨 Icônes personnalisées** : Cible TARGET + marqueurs colorés

### 🔧 Corrections techniques

#### 🐛 **Bugs corrigés**
- **🔴 Bootloop ESP32** : Résolu par effacement complet flash + upload propre
- **❌ Routes 404** : About et OTA désormais accessibles
- **📡 RSSI manquant** : Signal WiFi affiché partout (logs série + web)
- **🎯 TARGET figé** : Points de la carte se mettent à jour en temps réel
- **⚙️ GPS Forward inactif** : Bouton ON/OFF fonctionnel

#### 🏗️ **Architecture améliorée**  
- **🎛️ ElegantOTA** : Intégration propre avant `server.onNotFound()`
- **📁 Fallback HTML** : Gestion automatique des fichiers `.html` 
- **🔧 WebSocket RSSI** : Push signal WiFi toutes les 2 secondes
- **💾 NVS étendu** : Stockage profils CONFIG SEAKER

### 📊 **API REST étendue**

#### 🆕 Nouveaux endpoints
```
GET  /api/seaker-configs        → 4 profils CONFIG
POST /api/seaker-configs        → Sauvegarder profil  
POST /api/seaker-configs/send   → Envoyer au SEAKER

GET  /api/gps-forward          → État GPS Forward
POST /api/gps-forward          → Activer/désactiver

GET  /api/wifi                 → SSID actuel
POST /api/wifi                 → Changer WiFi (redémarre ESP32)
```

#### ✅ **API enrichie**
- **📡 RSSI** : Ajouté dans `/api/telemetry`
- **🛰️ GPS Status** : RTK Fix/Float/DGPS dans telemetry
- **🎯 SEAKER Config** : Mode correction (Normal/Offset/Transponder)

### 📝 **Trames NMEA étendues**

#### 🆕 Nouvelles trames
```
$SYS    → Ajout rssi=-XXdBm
$GPS    → Ajout status=FIX/FLT/DGP  
$CONFIG → Profils SEAKER (4 variations)
```

#### 🎯 **GPS Forward**
- **📍 Trames GPGGA** : Position TARGET transmise au ROV via TCP:10111
- **🔄 Reconstruction** : Conversion TARGET → format NMEA standard
- **⚙️ Filtrage** : Seules les trames GPS reconstruites sont transmises

### 🎨 **Interface utilisateur**

#### 📱 **Dashboard amélioré**
- **📊 Signal temps réel** : WiFi RSSI avec icônes qualité
- **🔋 Batterie intelligente** : Temps restant avec moyennage adaptatif
- **⚙️ CONFIG direct** : Édition/envoi profils SEAKER inline
- **📧 Support** : Lien contact pré-rempli

#### 🗺️ **Carte interactive**
- **🎯 Double marqueurs** : TARGET (gris) + TARGETF (rouge cible)
- **🌙 Mode sombre** : CartoDB Dark tiles
- **📊 Overlays riches** : Batterie, WiFi, coordonnées TARGETF
- **🎨 Animations** : Barres WiFi + couleurs dynamiques

### 🔍 **Debug & monitoring**

#### 📋 **Logs enrichis**
```
[RSSI] WiFi signal: -XX dBm        → Toutes les 2s
[GPS] GGA fixQuality=X (raw field 6) → Debug GPS
[GPS Forward] Enabled/Disabled      → État TCP Forward
```

#### 📊 **Métriques système**
- **🧠 RAM usage** : 15.3% (50228/327680 bytes)
- **💾 Flash usage** : 75.0% (982681/1310720 bytes)  
- **⚡ Tâches FreeRTOS** : NTRIP (Core0) + SEAKER (Core1)

### 🌊 **Applications marines**

#### 🤖 **ROV Integration**
- **📍 Position TARGET** : Transmise en NMEA standard
- **🔌 Port dédié** : TCP:10111 pour navigation ROV
- **🎯 Compatibilité** : BlueOS + logiciels nav standard

#### 🎯 **SEAKER Pro**
- **⚙️ 4 profils** : Configurations mission-specific
- **🔧 Correction distance** : Offset + transponder delay
- **💾 Persistance** : Sauvegarde NVS automatique

---

## 📈 Statistiques de développement

- **📝 Commits** : +50 modifications majeures
- **🗂️ Fichiers modifiés** : 8 fichiers core + 4 HTML
- **⏱️ Temps compilation** : ~210 secondes (release optimisée)
- **📦 Taille firmware** : 989 KB (optimisé)
- **🌐 Pages web** : 4 interfaces complètes

## 🙏 Remerciements

Développement collaboratif avec tests terrain intensifs sur systèmes ROV professionnels.

---

**🚀 Cette version transforme le système SEAKER en solution complète de navigation sous-marine avec interface web moderne et intégration ROV native.**



