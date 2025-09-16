@echo off
echo.
echo ========================================
echo   🚀 ROV SEAKER v2.0 - GIT RELEASE
echo ========================================
echo.

:: Vérification que nous sommes dans un repo git
if not exist .git (
    echo ❌ ERREUR: Pas de repository Git détecté
    echo 📋 Initialisez avec: git init
    pause
    exit /b 1
)

echo 📋 Étape 1: Ajout de tous les fichiers...
git add .

echo.
echo 📝 Étape 2: Commit avec message de release...
git commit -m "🚀 Release v2.0 - ROV ESP32 Seaker

✨ Nouvelles fonctionnalités:
- 📊 Signal WiFi RSSI (dashboard + carte)
- 🔄 OTA Updates (ElegantOTA)
- 📖 Page À propos complète
- ⚙️ Configuration WiFi inline
- 🎯 4 profils SEAKER configurables
- 📡 GPS Forward TCP:10111 pour ROV
- 🛰️ Statut GPS correct (RTK Fix/Float/DGPS)
- 🎯 Double marqueurs TARGET (brut + filtré)
- 🌙 Mode sombre pour carte marine
- 🔋 Batterie optimisée (11.2V max + temps restant)

🔧 Corrections:
- ❌ Routes About/OTA 404 → Résolues
- 📡 RSSI manquant → Affiché partout
- 🎯 TARGET figé → Temps réel
- 🔴 Bootloop ESP32 → Process fiabilisé

🏗️ Architecture:
- 🌐 12 endpoints API REST
- 📡 4 serveurs TCP (80, 81, 10110, 10111)
- 💾 Stockage NVS étendu
- ⚡ Tâches FreeRTOS optimisées

🎯 ROV Ready: Interface complète navigation sous-marine"

echo.
echo 📌 Étape 3: Création du tag v2.0...
git tag -a v2.0 -m "ROV ESP32 Seaker v2.0 - Interface moderne + GPS Forward + OTA"

echo.
echo 🌐 Étape 4: Push vers repository distant...
echo 📋 Commandes à exécuter manuellement:
echo.
echo   git remote add origin https://github.com/[votre-username]/[votre-repo].git
echo   git push -u origin main
echo   git push origin v2.0
echo.
echo ✅ Package Git créé avec succès !
echo.
echo 📁 Fichiers ajoutés:
echo   - CHANGELOG.md     (historique complet)
echo   - RELEASE_NOTES.md (guide installation)  
echo   - Tous les sources (.cpp, .h)
echo   - Interface web (data/*.html)
echo   - Configuration (platformio.ini)
echo.
echo 🎯 Prêt pour publication sur GitHub !
echo.
pause



