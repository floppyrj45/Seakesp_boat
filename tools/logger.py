import argparse
import datetime as dt
import json
import os
import socket
import sys
import threading
import time
from urllib import request, error


def ts():
    return dt.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.%fZ')


def ensure_dir(path: str):
    d = os.path.dirname(path)
    if d and not os.path.exists(d):
        os.makedirs(d, exist_ok=True)


class RotatingFile:
    def __init__(self, base_path: str, max_mb: int = 50):
        self.base_path = base_path
        self.max_bytes = max_mb * 1024 * 1024
        self.idx = 0
        self.lines_written = 0
        ensure_dir(base_path)
        self.f = open(self._path(), 'a', encoding='utf-8')

    def _path(self):
        root, ext = os.path.splitext(self.base_path)
        return f"{root}.{self.idx:03d}{ext or '.log'}"

    def write(self, line: str):
        if self.f.tell() > self.max_bytes:
            self.f.close()
            self.idx += 1
            self.f = open(self._path(), 'a', encoding='utf-8')
        self.f.write(line)
        self.f.flush()
        self.lines_written += 1

    def get_total_size_mb(self):
        """Retourne la taille totale de tous les fichiers de logs en MB"""
        total_size = 0
        root, ext = os.path.splitext(self.base_path)
        i = 0
        while True:
            path = f"{root}.{i:03d}{ext or '.log'}"
            if os.path.exists(path):
                total_size += os.path.getsize(path)
                i += 1
            else:
                break
        return total_size / (1024 * 1024)

    def close(self):
        try:
            self.f.close()
        except Exception:
            pass


def tcp_console_logger(host: str, port: int, out: RotatingFile, stop_evt: threading.Event):
    backoff = 1
    while not stop_evt.is_set():
        try:
            with socket.create_connection((host, port), timeout=5) as s:
                s.settimeout(2)
                out.write(f"{ts()} TCP connected {host}:{port}\n")
                backoff = 1
                buf = b''
                while not stop_evt.is_set():
                    try:
                        chunk = s.recv(4096)
                        if not chunk:
                            raise ConnectionError('socket closed')
                        buf += chunk
                        while b"\n" in buf:
                            line, buf = buf.split(b"\n", 1)
                            line = line.decode('utf-8', errors='replace').rstrip('\r')
                            out.write(f"{ts()} NMEA {line}\n")
                    except socket.timeout:
                        continue
        except Exception as e:
            out.write(f"{ts()} TCP error: {e}\n")
            time.sleep(backoff)
            backoff = min(backoff * 2, 10)


def poll_api(host: str, out: RotatingFile, stop_evt: threading.Event, period_s: float = 1.0):
    url = f"http://{host}/api/telemetry"
    backoff = 1
    while not stop_evt.is_set():
        try:
            with request.urlopen(url, timeout=3) as r:
                data = r.read().decode('utf-8', errors='replace')
                # validate json once, then write compact
                try:
                    j = json.loads(data)
                    out.write(f"{ts()} API {json.dumps(j, separators=(',',':'))}\n")
                except json.JSONDecodeError:
                    out.write(f"{ts()} API_INVALID {data}\n")
            backoff = 1
        except Exception as e:
            out.write(f"{ts()} API error: {e}\n")
            time.sleep(backoff)
            backoff = min(backoff * 2, 10)
        time.sleep(period_s)


def stats_display(out: RotatingFile, stop_evt: threading.Event, period_s: int = 30):
    """Affiche périodiquement les statistiques d'exécution"""
    start_time = time.time()
    while not stop_evt.is_set():
        try:
            uptime_min = int((time.time() - start_time) / 60)
            size_mb = out.get_total_size_mb()
            lines = out.lines_written
            
            print(f"📊 STATS: {uptime_min}min uptime | {lines} mesures | {size_mb:.1f} MB logs")
            
            for _ in range(period_s):
                if stop_evt.is_set():
                    break
                time.sleep(1)
        except Exception as e:
            print(f"❌ Erreur stats: {e}")
            time.sleep(period_s)


def main():
    p = argparse.ArgumentParser(description='Logger ROV (TCP console + API telemetry)')
    p.add_argument('--host', default='seakesp.local', help='hôte ESP32 (ex: 192.168.1.42)')
    p.add_argument('--tcp-port', type=int, default=10110, help='port TCP console NMEA')
    p.add_argument('--out', default='logs/rov.log', help='chemin du fichier log (rotation .000, .001, ...)')
    p.add_argument('--max-mb', type=int, default=50, help='taille max par fichier (MB)')
    p.add_argument('--no-api', action='store_true', help="désactiver le polling /api/telemetry")
    p.add_argument('--stats-period', type=int, default=30, help="période d'affichage des stats (secondes)")
    args = p.parse_args()

    out = RotatingFile(args.out, args.max_mb)
    stop_evt = threading.Event()

    # Thread pour logger TCP NMEA
    t1 = threading.Thread(target=tcp_console_logger, args=(args.host, args.tcp_port, out, stop_evt), daemon=True)
    t1.start()

    # Thread pour polling API (optionnel)
    if not args.no_api:
        t2 = threading.Thread(target=poll_api, args=(args.host, out, stop_evt, 1.0), daemon=True)
        t2.start()

    # Thread pour affichage des stats
    t3 = threading.Thread(target=stats_display, args=(out, stop_evt, args.stats_period), daemon=True)
    t3.start()

    # Messages de démarrage
    print(f"🚀 Logger ROV démarré !")
    print(f"📡 Hôte: {args.host}:{args.tcp_port}")
    print(f"💾 Logs: {args.out} (max {args.max_mb}MB par fichier)")
    if args.no_api:
        print(f"⚠️  API polling désactivé")
    print(f"📊 Stats toutes les {args.stats_period}s")
    print(f"⏹️  Ctrl+C pour arrêter\n")
    
    out.write(f"{ts()} Logger started host={args.host} tcp={args.tcp_port} out={args.out}\n")
    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n🛑 Arrêt en cours...")
        out.write(f"{ts()} Logger stopping\n")
        stop_evt.set()
        time.sleep(0.2)
        out.close()
        print("✅ Logger arrêté.")


if __name__ == '__main__':
    main()


