# Architecture Cloud Bundle ROV (Ingestion + Dashboard + OTA)

## Objectifs
- Ingestion temps réel depuis ESP32 via HTTPS (push sortant, pas d'ouverture côté ESP32)
- Dashboard multi-appareils (live via SSE/WebSocket), historique (NDJSON/DB), carte
- OTA managée: binaires signés, manifest par canal (stable/beta), endpoints publics

## Composants
- Reverse proxy (Caddy): TLS, static front, reverse proxy vers backend
- Backend (FastAPI): `/api/telemetry` (ingestion), `/api/events` (SSE), `/api/telemetry/latest`, `/ota/manifest/{device_id}`, `/firmware/*`
- Stockage léger: fichiers NDJSON par device (`server/backend/data/*.ndjson`)
- Firmware: `server/backend/firmware/<channel>/manifest.json` et binaires

## Sécurité
- TLS obligatoire (proxy)
- Auth ingestion via `X-API-Key` (clé par appareil)
- Binaires OTA publics en lecture; manifest peut être public ou require token
- Rate limiting sur proxy recommandé

## Déploiement
- Via `docker-compose` dans `server/`
- Variables: `INGEST_API_KEY`

## Évolutions possibles
- Persistance time-series (InfluxDB/TimescaleDB)
- Authentification dashboard + ACL par client
- MQTT broker optionnel pour haute disponibilité
- Signatures de firmware + vérification côté ESP32

