## Filtrage et incertitudes de la CIBLE

### Objectifs
- Estimer une incertitude (r95) sur la position calculée de la CIBLE.
- Lisser la CIBLE avec un filtre de Kalman 2D et rejeter les outliers.

### Contributions d'erreur
- GPS: selon `fixQuality` (GGA champ 6) et/ou `HDOP`.
  - RTK Fix (4) → σ ≈ 0.03 m
  - RTK Float (5) → σ ≈ 0.10 m
  - Sinon → σ ≈ max(1.0, HDOP×1.5)
- SEAKER:
  - erreur angulaire σθ (par défaut 3°) → erreur latérale σlat ≈ d × σθ(rad)
  - erreur de distance relative ρ (par défaut 0.5%) → σrng ≈ ρ × d (≥ 0.1 m)
  - σseaker ≈ sqrt(σlat² + σrng²)

L'incertitude de mesure combinée est: σmeas ≈ sqrt(σgps² + σseaker²), et
on publie r95 ≈ 2.45 × σmeas (approximation 2D).

### Filtre de Kalman 2D (position-vitesse)
- Etat: [x, y, vx, vy] en UTM.
- Modèle: vitesse constante; bruit process aStd (m/s²), par défaut 0.5.
- Mesure: position (x,y) issue de la CIBLE instantanée, bruit σmeas.
- Gating (rejet d'outliers): innovation normalisée < gate (par défaut 4σ).

### Trames émises
- `$TARGET,lat,lon,az=...,dist_m=...,r95_m=...*CS`
- `$TARGETF,lat,lon,r95_m=...*CS` (filtrée, si mesure acceptée)

### Réglages (CLI)
- `A`: sigma angulaire SEAKER en degrés (σθ)
- `R`: erreur relative distance SEAKER (ρ)
- `K`: bruit process Kalman aStd (m/s²)
- `G`: seuil de gating (sigma)

Ces réglages sont persistés (NVS) et chargés au démarrage.






