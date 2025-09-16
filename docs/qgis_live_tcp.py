# QGIS Live TCP client for Bundle ROV (ESP32 console on port 10110)
# - Connects to TCP, parses $GPS/$TARGET/$TARGETF frames
# - Maintains 3 memory layers with the latest positions
# - Thread-safe updates via QTimer on main thread

import socket
import threading
import time
from qgis.PyQt import QtCore
from qgis.core import (
    QgsProject,
    QgsVectorLayer,
    QgsFields,
    QgsField,
    QgsFeature,
    QgsGeometry,
    QgsPointXY,
    QgsWkbTypes,
    QgsCoordinateReferenceSystem,
)
from qgis.PyQt.QtCore import QVariant
from qgis.utils import iface


class _Layer:
    def __init__(self, name: str):
        self.name = name
        self.layer = None

    def ensure(self):
        if self.layer and self.layer.isValid():
            return self.layer
        # try find existing
        for lyr in QgsProject.instance().mapLayers().values():
            if lyr.name() == self.name:
                self.layer = lyr
                return self.layer
        vl = QgsVectorLayer("Point?crs=EPSG:4326", self.name, "memory")
        pr = vl.dataProvider()
        pr.addAttributes([
            QgsField("ts", QVariant.LongLong),
            QgsField("src", QVariant.String),
            QgsField("info", QVariant.String),
        ])
        vl.updateFields()
        QgsProject.instance().addMapLayer(vl)
        self.layer = vl
        return vl

    def set_point(self, lon: float, lat: float, ts: int, src: str, info: str = ""):
        vl = self.ensure()
        if not vl:
            return
        pr = vl.dataProvider()
        pr.truncate()
        f = QgsFeature(vl.fields())
        f.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(lon, lat)))
        f.setAttributes([ts, src, info])
        pr.addFeatures([f])
        vl.triggerRepaint()
        iface.mapCanvas().refresh()


class TcpReader(QtCore.QObject):
    frame_received = QtCore.pyqtSignal(str)

    def __init__(self, ip: str, port: int, parent=None):
        super().__init__(parent)
        self.ip = ip
        self.port = port
        self._thread = None
        self._stop = threading.Event()

    def start(self):
        if self._thread and self._thread.is_alive():
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()

    def _run(self):
        backoff = 1.0
        while not self._stop.is_set():
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(3.0)
                s.connect((self.ip, self.port))
                s.settimeout(1.0)
                buf = b""
                backoff = 1.0
                while not self._stop.is_set():
                    try:
                        chunk = s.recv(2048)
                        if not chunk:
                            time.sleep(0.05)
                            continue
                        buf += chunk
                        while b"\n" in buf:
                            line, buf = buf.split(b"\n", 1)
                            try:
                                sline = line.decode("utf-8", errors="replace").strip()
                                if sline:
                                    self.frame_received.emit(sline)
                            except Exception:
                                pass
                    except socket.timeout:
                        continue
            except Exception:
                time.sleep(backoff)
                backoff = min(5.0, backoff * 1.5)
            finally:
                try:
                    s.close()
                except Exception:
                    pass


class LiveController(QtCore.QObject):
    def __init__(self, ip: str, port: int, parent=None):
        super().__init__(parent)
        self.ip = ip
        self.port = port
        self.tcp = TcpReader(ip, port)
        self.tcp.frame_received.connect(self.on_frame)
        self.lyr_gps = _Layer("GPS_live")
        self.lyr_tgt = _Layer("TARGET_live")
        self.lyr_tgtf = _Layer("TARGETF_live")

    def start(self):
        self.lyr_gps.ensure(); self.lyr_tgt.ensure(); self.lyr_tgtf.ensure()
        self.tcp.start()
        print(f"[qgis_live] Connect {self.ip}:{self.port}")

    def stop(self):
        self.tcp.stop()

    def on_frame(self, line: str):
        # print(line)
        if not line.startswith("$"):
            return
        core = line[1:]
        star = core.find('*')
        if star >= 0:
            core = core[:star]
        parts = core.split(',')
        if not parts:
            return
        head = parts[0]
        ts = int(time.time())
        try:
            if head == "GPS":
                meta = {}
                for p in parts[1:]:
                    if '=' in p:
                        a, b = p.split('=', 1)
                        meta[a] = b
                lat = float(meta.get('lat', 'nan'))
                lon = float(meta.get('lon', 'nan'))
                if not (lat != lat or lon != lon):
                    info = f"valid={meta.get('valid','')},sats={meta.get('sats','')},hdop={meta.get('hdop','')}"
                    self.lyr_gps.set_point(lon, lat, ts, "GPS", info)
            elif head == "TARGET" or head == "TARGETF":
                if len(parts) >= 3:
                    lat = float(parts[1])
                    lon = float(parts[2])
                    if head == "TARGETF":
                        lyr = self.lyr_tgtf
                    else:
                        lyr = self.lyr_tgt
                    lyr.set_point(lon, lat, ts, head)
        except Exception:
            pass


controller = None


def start(ip: str = "192.168.0.117", port: int = 10110):
    global controller
    if controller is not None:
        print("[qgis_live] déjà démarré")
        return controller
    c = LiveController(ip, port)
    c.start()
    controller = c
    return controller


def stop():
    global controller
    if controller is not None:
        controller.stop()
        controller = None
        return True
    return False


print("[qgis_live] Utilisation: from qgis_live_tcp import start, stop; start('IP_ESP32',10110)")






