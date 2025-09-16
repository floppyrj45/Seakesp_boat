# QGIS file watcher pour Bundle ROV
# - Charge/ajoute les couches target/gps (GeoJSON + CSV)
# - Sur modification des fichiers, recharge automatiquement les couches

import os
import time
from qgis.PyQt import QtCore
from qgis.core import QgsProject, QgsVectorLayer
from qgis.utils import iface


def _default_base_dir():
    try:
        return os.path.dirname(os.path.abspath(__file__))
    except Exception:
        # Fallback pour exécution depuis la console QGIS
        return r"C:\Users\flopp\OneDrive\Bureau\Bundle ROV\docs"


BASE_DIR = _default_base_dir()

PATHS = {
    "TARGET_GJ": os.path.join(BASE_DIR, "target.geojson"),
    "GPS_GJ": os.path.join(BASE_DIR, "gps.geojson"),
    "TARGET_CSV": os.path.join(BASE_DIR, "target.csv"),
    "GPS_CSV": os.path.join(BASE_DIR, "gps.csv"),
}


def _csv_uri(path: str) -> str:
    # CSV avec colonnes lon,lat
    # CRS: EPSG:4326
    # Voir: https://docs.qgis.org/ pour paramètres delimitedtext
    path_uri = path.replace("\\", "/")
    return f"file:///{path_uri}?delimiter=,&xField=lon&yField=lat&crs=EPSG:4326"


class LayerBinding:
    def __init__(self, name: str, path: str, provider: str, uri: str = None):
        self.name = name
        self.path = path
        self.provider = provider
        self.uri = uri if uri is not None else path
        self.layer = None

    def ensure_loaded(self):
        if self.layer and self.layer.isValid():
            return self.layer
        # Rechercher par nom d'abord
        for lyr in QgsProject.instance().mapLayers().values():
            if lyr.name() == self.name:
                self.layer = lyr
                return self.layer
        # Sinon, créer
        lyr = QgsVectorLayer(self.uri, self.name, self.provider)
        if not lyr or not lyr.isValid():
            print(f"[qgis_watch] ERREUR: couche invalide: {self.name} -> {self.uri}")
            return None
        QgsProject.instance().addMapLayer(lyr)
        self.layer = lyr
        print(f"[qgis_watch] Couche ajoutée: {self.name} ({self.provider})")
        return self.layer

    def reload(self):
        lyr = self.ensure_loaded()
        if not lyr:
            return
        try:
            # QGIS >= 3.10: reload() disponible sur QgsMapLayer/QgsVectorLayer
            if hasattr(lyr, "reload"):
                lyr.reload()
            else:
                # Fallback: réappliquer la source
                lyr.setDataSource(lyr.source(), lyr.name(), lyr.providerType())
            lyr.triggerRepaint()
            iface.mapCanvas().refresh()
            print(f"[qgis_watch] Reload: {self.name}")
        except Exception as e:
            print(f"[qgis_watch] Reload échec {self.name}: {e}")


class QgisFileWatcher(QtCore.QObject):
    def __init__(self, bindings: list, parent=None):
        super().__init__(parent)
        self.bindings = bindings
        self._path_to_binding = {b.path: b for b in bindings}
        self._watcher = QtCore.QFileSystemWatcher(self)
        self._mtimes = {}
        self._debounce_ms = 250
        self._timer = QtCore.QTimer(self)
        self._timer.setInterval(0)
        self._timer.setSingleShot(True)
        self._timer.timeout.connect(self._process_pending)
        self._pending_paths = set()

    def start(self):
        # S'assurer que les couches sont chargées au lancement
        for b in self.bindings:
            b.ensure_loaded()
        # Ajouter les chemins existants au watcher
        paths = [p for p in self._path_to_binding.keys() if os.path.exists(p)]
        if paths:
            self._watcher.addPaths(paths)
        self._watcher.fileChanged.connect(self._on_file_changed)
        print(f"[qgis_watch] Watcher démarré sur {len(paths)} fichier(s)")

    def stop(self):
        self._watcher.fileChanged.disconnect(self._on_file_changed)
        if self._watcher.files():
            self._watcher.removePaths(self._watcher.files())
        print("[qgis_watch] Watcher arrêté")

    def _on_file_changed(self, path: str):
        # Débounce + re-ajout du path car os.replace() peut détacher le watcher
        try:
            if path and os.path.exists(path) and path not in self._watcher.files():
                self._watcher.addPath(path)
        except Exception:
            pass
        self._pending_paths.add(path)
        self._timer.start(self._debounce_ms)

    def _process_pending(self):
        for path in list(self._pending_paths):
            self._pending_paths.discard(path)
            b = self._path_to_binding.get(path)
            if not b:
                continue
            # Vérifier mtime pour éviter les doublons
            try:
                mtime = os.path.getmtime(path)
            except Exception:
                # Fichier absent, on ignore pour cette fois
                continue
            last = self._mtimes.get(path)
            if last and (mtime - last) < 0.05:
                continue
            self._mtimes[path] = mtime
            b.reload()


def _make_bindings():
    return [
        LayerBinding("TARGET", PATHS["TARGET_GJ"], "ogr", PATHS["TARGET_GJ"]),
        LayerBinding("GPS", PATHS["GPS_GJ"], "ogr", PATHS["GPS_GJ"]),
        LayerBinding("TARGET_csv", PATHS["TARGET_CSV"], "delimitedtext", _csv_uri(PATHS["TARGET_CSV"])) ,
        LayerBinding("GPS_csv", PATHS["GPS_CSV"], "delimitedtext", _csv_uri(PATHS["GPS_CSV"])) ,
    ]


# Point d’entrée pratique depuis la console QGIS
watcher = None


def start():
    global watcher
    if watcher is not None:
        print("[qgis_watch] déjà démarré")
        return watcher
    b = _make_bindings()
    for it in b:
        it.ensure_loaded()
    w = QgisFileWatcher(b)
    w.start()
    watcher = w
    return watcher


def stop():
    global watcher
    if watcher is not None:
        watcher.stop()
        watcher = None
        return True
    return False


print(f"[qgis_watch] BASE_DIR = {BASE_DIR}")
print("[qgis_watch] Utilisation depuis la console: from qgis_watch import start, stop; start()")






