import asyncio
import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, AsyncGenerator, Dict, List

from fastapi import FastAPI, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, Response, StreamingResponse
from fastapi.staticfiles import StaticFiles


INGEST_API_KEY: str = os.getenv("INGEST_API_KEY", "change-me")
DATA_DIR = Path("/app/data")
FIRMWARE_DIR = Path("/app/firmware")

DATA_DIR.mkdir(parents=True, exist_ok=True)
FIRMWARE_DIR.mkdir(parents=True, exist_ok=True)


app = FastAPI(title="Bundle ROV Server", version="0.1.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"]
)


# Mémoire: dernier état par device
latest_by_device: Dict[str, Dict[str, Any]] = {}


# SSE: chaque abonné reçoit les événements via sa file
subscribers: List[asyncio.Queue] = []


def _utc_iso_now() -> str:
    return datetime.now(timezone.utc).isoformat()


async def broadcast_event(event_type: str, payload: Dict[str, Any]) -> None:
    message = {"type": event_type, "at": _utc_iso_now(), "data": payload}
    for queue in list(subscribers):
        try:
            queue.put_nowait(message)
        except Exception:
            # On ignore les erreurs de file pleine
            pass


@app.post("/api/telemetry")
async def ingest_telemetry(request: Request) -> JSONResponse:
    api_key = request.headers.get("x-api-key") or request.headers.get("X-API-Key")
    if api_key != INGEST_API_KEY:
        raise HTTPException(status_code=401, detail="Invalid API key")

    try:
        body = await request.json()
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=400, detail="Invalid JSON") from exc

    device_id = str(body.get("deviceId") or body.get("device_id") or "unknown")
    if device_id == "unknown":
        raise HTTPException(status_code=400, detail="deviceId is required")

    body["serverReceivedAt"] = _utc_iso_now()
    latest_by_device[device_id] = body

    # Append en NDJSON pour audit/trace (simple et robuste)
    ndjson_path = DATA_DIR / f"{device_id}.ndjson"
    try:
        with ndjson_path.open("a", encoding="utf-8") as f:
            f.write(json.dumps(body, ensure_ascii=False) + "\n")
    except Exception:
        # On ne bloque pas l'ingestion si l'écriture échoue
        pass

    await broadcast_event("telemetry", {"deviceId": device_id, "telemetry": body})
    return JSONResponse({"ok": True})


@app.get("/api/telemetry/latest")
async def get_latest() -> JSONResponse:
    return JSONResponse(latest_by_device)


@app.get("/api/devices")
async def list_devices() -> JSONResponse:
    return JSONResponse(sorted(latest_by_device.keys()))


@app.get("/api/events")
async def sse_events() -> StreamingResponse:
    queue: asyncio.Queue = asyncio.Queue(maxsize=1000)
    subscribers.append(queue)

    async def event_generator() -> AsyncGenerator[bytes, None]:
        try:
            while True:
                message = await queue.get()
                data = json.dumps(message, ensure_ascii=False)
                yield f"event: {message['type']}\n".encode("utf-8")
                yield f"data: {data}\n\n".encode("utf-8")
        except asyncio.CancelledError:  # noqa: PERF203
            pass
        finally:
            try:
                subscribers.remove(queue)
            except ValueError:
                pass

    headers = {"Cache-Control": "no-cache", "Content-Type": "text/event-stream"}
    return StreamingResponse(event_generator(), headers=headers)


@app.get("/ota/manifest/{device_id}")
async def ota_manifest(device_id: str, channel: str | None = None) -> JSONResponse:
    # Canal par défaut
    channel_name = channel or "stable"
    manifest_path = FIRMWARE_DIR / channel_name / "manifest.json"
    if not manifest_path.exists():
        raise HTTPException(status_code=404, detail="manifest not found")
    try:
        with manifest_path.open("r", encoding="utf-8") as f:
            manifest = json.load(f)
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail="manifest error") from exc
    # Facultatif: vous pouvez filtrer/adapter par device_id ici
    return JSONResponse(manifest, headers={"Cache-Control": "no-cache"})


# Exposer les binaires firmware via /firmware/*
app.mount("/firmware", StaticFiles(directory=str(FIRMWARE_DIR), html=False), name="firmware")


@app.get("/healthz")
async def healthz() -> Response:
    return Response(status_code=204)

