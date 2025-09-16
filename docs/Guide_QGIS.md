## Afficher la position GPS et la CIBLE en temps réel dans QGIS

### Ce que vous allez obtenir
- La position GPS du ROV (affichée dans le panneau GPS de QGIS si vous utilisez la méthode série/USB).
- La position de la CIBLE (TARGET) mise à jour en temps réel sur la carte via un fichier GeoJSON.

### Prérequis
- PC et ESP32 sur le même réseau Wi‑Fi
- QGIS 3.x installé
- Python 3 installé
- Pare‑feu autorisant l’UDP sur le port 22222 (par défaut)

---

### Étape 1 — Lancer le script PC (TARGET via TCP)
1. Ouvrez `docs/Qgis.py` et mettez `TCP_IP` à l’IP de votre ESP32 (voir `$SYS ... ip=...`).
2. Exécutez:
   ```
   python ./docs/Qgis.py
   ```
3. Le script se connecte en TCP au port 10110 et écrit/actualise `target.geojson` lorsque `$TARGET` arrive.
4. Laissez-le tourner.

### Étape 2 — Vérifier l’ESP32
1. Assurez-vous que l’ESP32 est connecté au Wi‑Fi (`$SYS ... wifi=STA, ip=...`).
2. Le firmware ouvre automatiquement un serveur TCP sur le port 10110 et diffuse toutes les lignes NMEA/diagnostic, y compris `$TARGET`.

### Étape 3 — Ajouter la CIBLE dans QGIS
1. Menu: Couche → Ajouter une couche → Ajouter une couche vecteur.
2. Choisissez `target.geojson` (fichier créé par le script).
3. Propriétés de la couche → Rendu → cochez «Rafraîchir automatiquement» et mettez 1 s.
4. La CIBLE apparaît et se met à jour.

### (Option) Étape 4 — Afficher la position GPS dans QGIS
Méthode simple (USB):
1. Branchez l’ESP32 en USB et repérez le port COM.
2. QGIS → Affichage → Panneaux → «Informations GPS».
3. Paramètres → Type: «Port série», Port: COMx, Vitesse: 115200, Format: NMEA.
4. Connecter.

- Aucun point TARGET: vérifier `TCP_IP` dans `docs/Qgis.py`, l’IP de l’ESP32 (`$SYS`), la présence de trames `$TARGET` côté terminal.
- Le GPS ne bouge pas: vérifier la connexion dans «Informations GPS» et les trames NMEA.


