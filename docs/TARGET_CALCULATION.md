# 📍 Calcul de Position TARGET (Non Filtrée)

## Vue d'ensemble

La position **TARGET** représente la position **instantanée** et **non filtrée** de la cible détectée par le sonar SEAKER, calculée en combinant :
- 📡 **Position GPS** de la plateforme (ROV/bateau)
- 🎯 **Angle et distance SEAKER** vers la cible
- 🧭 **Cap de la plateforme** (heading GPS)

---

## 🔄 Processus de Calcul Détaillé

### 1️⃣ **Acquisition des Données**
```cpp
// Dans printTargetFrame() - src/main.cpp:302-349
GpsFix fix = gpsGetFix();                    // Position GPS plateforme
double platformAz = fix.trueHeadingDeg;      // Cap GPS (ou headingDeg si indispo)
double rel = gSeaker.lastAngle;              // Angle relatif SEAKER (degrés)
double rawDistance = gSeaker.lastDistance;   // Distance brute SEAKER (mètres)
```

### 2️⃣ **Corrections d'Angle SEAKER**
```cpp
// Application des corrections configurables
if (gSeakerInvertAngle) rel = -rel;          // ✅ NOUVEAU: Inversion sens rotation
rel += (double)gSeakerAngleOffsetDeg;        // Offset angulaire fixe
while (rel < 0) rel += 360.0;                // Normalisation 0-360°
while (rel >= 360.0) rel -= 360.0;
```

### 3️⃣ **Calcul Azimut Absolu**
```cpp
double az = platformAz + rel;                // Azimut absolu = Cap + Angle relatif
while (az < 0) az += 360.0;                  // Normalisation 0-360°
while (az >= 360.0) az -= 360.0;
```

### 4️⃣ **Correction de Distance**
Selon le mode configuré (`gSeakerMode`) :

#### **Mode NORMAL** (par défaut)
```cpp
double correctedDistance = rawDistance;      // Aucune correction
```

#### **Mode OFFSET**
```cpp
double correctedDistance = rawDistance - gSeakerDistOffset;  // Soustraction fixe
```

#### **Mode TRANSPONDER**
```cpp
// Compensation délai transpondeur
float delayDistance = (gSeakerTransponderDelay / 1000.0f) * 1500.0f;  // Vitesse son 1500 m/s
float remainingDistance = rawDistance - delayDistance;
double correctedDistance = remainingDistance / 2.0f;  // Division par 2 (aller-retour)
```

### 5️⃣ **Projection Géographique UTM**
```cpp
// Conversion WGS84 → UTM pour calculs métriques
int zone; bool north; double e0, n0;
wgs84ToUtm(fix.latitude, fix.longitude, zone, north, e0, n0);

// Calcul déplacement en coordonnées UTM
double brg = az * (M_PI/180.0);              // Azimut en radians
double de = correctedDistance * sin(brg);    // Déplacement Est (m)
double dn = correctedDistance * cos(brg);    // Déplacement Nord (m)

// Position cible en UTM
double e1 = e0 + de;                         // Coordonnée Est cible
double n1 = n0 + dn;                         // Coordonnée Nord cible
```

### 6️⃣ **Reconversion WGS84**
```cpp
// UTM → WGS84 pour position finale
double tgtLat, tgtLon;
utmToWgs84(zone, north, e1, n1, tgtLat, tgtLon);
```

---

## 📊 Estimation d'Incertitude (R95)

### **Erreur GPS**
```cpp
float gpsStd = estimateGpsPosStd(fix);
// RTK Fix (4) → 0.03m | RTK Float (5) → 0.10m | Autre → max(1.0, HDOP×1.5)
```

### **Erreur SEAKER**
```cpp
float seakerStd = estimateSeakerPosStd(correctedDistance);
// Erreur angulaire: distance × σθ (3° par défaut)
// Erreur distance: distance × 0.5% (minimum 0.1m)
// Combined: √(σangulaire² + σdistance²)
```

### **Incertitude Combinée**
```cpp
float measStd = √(gpsStd² + seakerStd²);    // Écart-type combiné
float r95 = measStd × 2.45f;                // Rayon 95% confiance (approximation 2D)
```

---

## 📡 Diffusion des Données

### **Trame NMEA TARGET**
```
$TARGET,lat,lon,az=123.4,dist_m=45.6,r95_m=2.1*CS
```

### **WebSocket JSON**
```json
{
  "targetf": {
    "lat": 47.6612270,
    "lon": -2.7375880,
    "r95_m": 2.10,
    "filtered": false
  }
}
```

### **Variables Globales Mises à Jour**
```cpp
telemetrySetTargetF(tgtLat, tgtLon, r95);   // API /api/telemetry
gTargetFLat = tgtLat;                       // Pour GPS Forward
gTargetFLon = tgtLon;                       // Pour GPS Forward
```

---

## ⚙️ Paramètres Configurables

| Paramètre | Variable | Description | Défaut |
|-----------|----------|-------------|--------|
| **Inversion angle** | `gSeakerInvertAngle` | Inverse le sens de rotation | `false` |
| **Offset angulaire** | `gSeakerAngleOffsetDeg` | Correction fixe d'angle | `0.0°` |
| **Mode distance** | `gSeakerMode` | Normal/Offset/Transponder | `NORMAL` |
| **Offset distance** | `gSeakerDistOffset` | Correction fixe de distance | `0.0m` |
| **Délai transponder** | `gSeakerTransponderDelay` | Délai aller-retour | `0.0ms` |
| **Erreur angulaire** | `gSeakerAngleSigmaDeg` | Incertitude angle | `3.0°` |
| **Erreur distance** | `gSeakerRangeRel` | Erreur relative distance | `0.5%` |

---

## 🔄 Différence avec TARGETF (Filtrée)

- **TARGET** : Position **instantanée**, **brute**, recalculée à chaque mesure SEAKER
- **TARGETF** : Position **filtrée** par Kalman 2D, **lissée**, avec rejet d'outliers

Le calcul TARGET est la **base** qui alimente ensuite le filtre de Kalman pour produire TARGETF.

---

## 🎯 Utilisation

1. **Affichage temps réel** : Dashboard et carte
2. **GPS Forward** : Envoi vers ROV via TCP (port 10111)
3. **Filtrage Kalman** : Entrée pour calcul TARGETF
4. **Logging/Debug** : Traçabilité des mesures brutes

Cette position TARGET représente la **meilleure estimation instantanée** de la position de la cible, avant tout filtrage ou lissage.


