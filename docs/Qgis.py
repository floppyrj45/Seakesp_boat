import json, socket, time, os

TCP_IP = "192.168.0.117"  # IP de l'ESP32
TCP_PORT = 10110           # Port serveur NMEA/console
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUT_FILE = os.path.join(SCRIPT_DIR, "target.geojson")
CSV_FILE = os.path.join(SCRIPT_DIR, "target.csv")
GPS_GEOJSON = os.path.join(SCRIPT_DIR, "gps.geojson")
GPS_CSV = os.path.join(SCRIPT_DIR, "gps.csv")

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((TCP_IP, TCP_PORT))
sock.settimeout(1.0)
print(f"Connected TCP {TCP_IP}:{TCP_PORT}")
last_status = time.time()
print("Communication établie. En attente de trames $TARGET...")

def ensure_placeholders():
    # Crée un GeoJSON vide et un CSV avec en-tête si absents
    if not os.path.exists(OUT_FILE):
        fc = {"type":"FeatureCollection","features":[]}
        tmp = OUT_FILE + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(fc, f)
        os.replace(tmp, OUT_FILE)
        print(f"Fichier créé: {OUT_FILE} (vide)")
    if not os.path.exists(CSV_FILE):
        tmp = CSV_FILE + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write("lon,lat,ts\n")
        os.replace(tmp, CSV_FILE)
        print(f"Fichier créé: {CSV_FILE} (en-tête)")
    if not os.path.exists(GPS_GEOJSON):
        fc = {"type":"FeatureCollection","features":[]}
        tmp = GPS_GEOJSON + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(fc, f)
        os.replace(tmp, GPS_GEOJSON)
        print(f"Fichier créé: {GPS_GEOJSON} (vide)")
    if not os.path.exists(GPS_CSV):
        tmp = GPS_CSV + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write("lon,lat,ts,valid,sats,hdop,alt,hdg,kn\n")
        os.replace(tmp, GPS_CSV)
        print(f"Fichier créé: {GPS_CSV} (en-tête)")

def write_geojson(lat, lon):
    fc = {
        "type":"FeatureCollection",
        "features":[
            {"type":"Feature",
             "geometry":{"type":"Point","coordinates":[lon, lat]},
             "properties":{"ts": int(time.time())}}
        ]
    }
    tmp = OUT_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(fc, f)
    os.replace(tmp, OUT_FILE)
    print(f"Position mise à jour: lat={lat:.6f}, lon={lon:.6f} -> {OUT_FILE}")

def write_csv(lat, lon):
    # On remplace tout pour simplicité (1 point courant)
    tmp = CSV_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write("lon,lat,ts\n")
        f.write(f"{lon:.7f},{lat:.7f},{int(time.time())}\n")
    os.replace(tmp, CSV_FILE)
    print(f"CSV mis à jour -> {CSV_FILE}")

def write_geojson_gps(lat, lon):
    fc = {
        "type":"FeatureCollection",
        "features":[
            {"type":"Feature",
             "geometry":{"type":"Point","coordinates":[lon, lat]},
             "properties":{"ts": int(time.time())}}
        ]
    }
    tmp = GPS_GEOJSON + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(fc, f)
    os.replace(tmp, GPS_GEOJSON)
    print(f"GPS mis à jour: lat={lat:.6f}, lon={lon:.6f} -> {GPS_GEOJSON}")

def write_csv_gps(lat, lon, meta):
    # meta est un dict avec clés optionnelles: valid,sats,hdop,alt,hdg,kn
    tmp = GPS_CSV + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write("lon,lat,ts,valid,sats,hdop,alt,hdg,kn\n")
        f.write(
            f"{lon:.7f},{lat:.7f},{int(time.time())},"
            f"{meta.get('valid','')},{meta.get('sats','')},{meta.get('hdop','')},"
            f"{meta.get('alt','')},{meta.get('hdg','')},{meta.get('kn','')}\n"
        )
    os.replace(tmp, GPS_CSV)
    print(f"CSV GPS mis à jour -> {GPS_CSV}")

ensure_placeholders()
buf = b""
while True:
    try:
        chunk = sock.recv(2048)
        if not chunk:
            now = time.time()
            if now - last_status >= 5:
                print("Avertissement: aucune donnée reçue (connexion inactive ?)...")
                last_status = now
            time.sleep(0.2)
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            try:
                s = line.decode("utf-8", errors="replace").strip()
                if s:
                    print(f"RX: {s}", flush=True)
                # On ne garde que les trames $TARGET/$TARGETF pour la cible
                if s.startswith("$TARGET,") or s.startswith("$TARGETF,"):
                    # $TARGET,lat,lon,az=...,dist_m=...*CS
                    core = s[1:]
                    star = core.find('*')
                    if star >= 0:
                        core = core[:star]
                    parts = core.split(',')
                    if len(parts) >= 3:
                        lat = float(parts[1])
                        lon = float(parts[2])
                        # On a reçu une nouvelle position
                        last_status = time.time()
                        write_geojson(lat, lon)
                        write_csv(lat, lon)
                # Trames $GPS pour la position plateforme
                elif s.startswith("$GPS,"):
                    core = s[1:]
                    star = core.find('*')
                    if star >= 0:
                        core = core[:star]
                    parts = core.split(',')
                    # Chercher lat=... et lon=...
                    kv = {}
                    for p in parts[1:]:
                        if '=' in p:
                            a, b = p.split('=', 1)
                            kv[a] = b
                    try:
                        lat = float(kv.get('lat', 'nan'))
                        lon = float(kv.get('lon', 'nan'))
                        if not (lat != lat or lon != lon):  # check NaN
                            last_status = time.time()
                            write_geojson_gps(lat, lon)
                            write_csv_gps(lat, lon, kv)
                    except Exception:
                        pass
            except Exception:
                pass
    except socket.timeout:
        now = time.time()
        if now - last_status >= 5:
            print("En attente de données...")
            last_status = now
        pass