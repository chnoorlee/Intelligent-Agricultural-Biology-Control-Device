"""FastAPI backend service for bird analysis system.

Provides REST API endpoints for:
- Detection data ingestion
- Statistical queries
- Device status management
- Alert management
- AI-powered analysis
"""

import os
import yaml
import logging
from pathlib import Path
from typing import Optional, List, Dict, Any
from datetime import datetime, timedelta
from contextlib import asynccontextmanager

from fastapi import FastAPI, HTTPException, Query, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

from .database import Database
from .statistics import StatisticsEngine
from .bird_analysis import BirdBehaviorAnalyzer
from .alert_service import AlertService
from .deepseek_client import DeepSeekClient

# -------------------- Config --------------------

def load_config(config_path: str = "config.yaml") -> Dict[str, Any]:
    """Load YAML configuration file."""
    # Try relative to this file first
    paths = [
        Path(__file__).parent.parent / config_path,
        Path(config_path),
        Path("large-farm-version/backend-analysis") / config_path,
    ]
    for p in paths:
        if p.exists():
            with open(p) as f:
                return yaml.safe_load(f)
    return {}

config = load_config()

# -------------------- Logging --------------------

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("bird-backend")

# -------------------- Services --------------------

db = Database(
    db_path=config.get("database", {}).get("path", "data/bird_analysis.db")
)

deepseek_cfg = config.get("deepseek", {})
deepseek = DeepSeekClient(
    api_key=os.environ.get("DEEPSEEK_API_KEY", deepseek_cfg.get("api_key", "")),
    api_base=deepseek_cfg.get("api_base", "https://api.deepseek.com/v1"),
    model=deepseek_cfg.get("model", "deepseek-chat"),
    max_tokens=deepseek_cfg.get("max_tokens", 2048),
    temperature=deepseek_cfg.get("temperature", 0.3),
) if deepseek_cfg.get("api_key") or os.environ.get("DEEPSEEK_API_KEY") else None

analyzer = BirdBehaviorAnalyzer(db, deepseek)
alert_service = AlertService(db)
stats_engine = StatisticsEngine()


# -------------------- App --------------------

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan handler."""
    logger.info("Bird Analysis Backend starting...")
    yield
    logger.info("Bird Analysis Backend shutting down...")


app = FastAPI(
    title="智能农业生物防控 - 后端分析服务",
    description="Bird detection analysis, statistics, and alert management API",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# -------------------- Pydantic Models --------------------

class DetectionRequest(BaseModel):
    """Detection upload request."""
    device_id: str = Field(..., description="Device identifier")
    timestamp: Optional[str] = Field(None, description="ISO 8601 timestamp")
    detections: List[Dict[str, Any]] = Field(..., description="Detection results")
    location: Optional[Dict[str, float]] = Field(None, description="GPS location")


class DeviceHeartbeat(BaseModel):
    """Device heartbeat request."""
    device_id: str
    device_type: str = "unknown"
    battery_voltage: Optional[float] = None
    battery_soc: Optional[float] = None
    is_charging: Optional[bool] = None
    gps_lat: Optional[float] = None
    gps_lng: Optional[float] = None
    altitude: Optional[float] = None
    signal_strength: Optional[int] = None
    status: str = "online"
    firmware_version: Optional[str] = None


class AlertResolveRequest(BaseModel):
    """Alert resolution request."""
    resolved_by: str = "system"


# -------------------- Root --------------------

@app.get("/")
async def root():
    """Health check."""
    return {
        "service": "Bird Analysis Backend",
        "version": "1.0.0",
        "status": "running",
    }


# -------------------- Detection Endpoints --------------------

@app.post("/api/v1/detections")
async def ingest_detections(
    data: DetectionRequest,
    background_tasks: BackgroundTasks,
):
    """Ingest bird detection results from field devices.

    Accepts batch detection data from drones and fixed camera units.
    """
    inserted = []
    location = data.location or {}

    for det in data.detections:
        bbox = None
        if "bbox" in det and len(det["bbox"]) == 4:
            bbox = tuple(det["bbox"])

        row_id = db.insert_detection(
            device_id=data.device_id,
            class_name=det.get("class", "unknown"),
            confidence=det.get("confidence", 0.0),
            bbox=bbox,
            count=det.get("count", 1),
            lat=location.get("lat"),
            lng=location.get("lng"),
            is_pest=det.get("is_pest", True),
            deter_action=det.get("deter_action"),
            deter_success=det.get("deter_success"),
            timestamp=data.timestamp,
        )
        inserted.append(row_id)

    # Background alert evaluation
    if data.device_id:
        background_tasks.add_task(
            _evaluate_alerts,
            data.device_id,
        )

    return {
        "status": "ok",
        "detection_ids": inserted,
        "count": len(inserted),
    }


# -------------------- Statistics Endpoints --------------------

@app.get("/api/v1/statistics")
async def get_statistics(
    device_id: Optional[str] = Query(None, description="Device filter"),
    start: Optional[str] = Query(None, description="Start date (ISO 8601)"),
    end: Optional[str] = Query(None, description="End date (ISO 8601)"),
    days: int = Query(7, description="Days to analyze if no date range"),
):
    """Get detection statistics."""
    if not start:
        start = (datetime.now() - timedelta(days=days)).isoformat()
    if not end:
        end = datetime.now().isoformat()

    detections = db.query_detections(
        device_id=device_id,
        start_time=start,
        end_time=end,
        limit=5000,
    )

    summary = stats_engine.compute_summary(detections, device_id)

    return {
        "device_id": device_id,
        "time_range": {"start": start, "end": end},
        **summary,
    }


@app.get("/api/v1/analysis")
async def get_analysis(
    device_id: str = Query(..., description="Device ID"),
    days: int = Query(7, description="Days to analyze"),
    use_ai: bool = Query(False, description="Use DeepSeek AI analysis"),
):
    """Get comprehensive bird behavior analysis."""
    result = analyzer.analyze_device(device_id, days, use_ai=use_ai)
    return result


@app.get("/api/v1/analysis/all")
async def get_all_devices_analysis(
    days: int = Query(7, description="Days to analyze"),
):
    """Get cross-device analysis."""
    result = analyzer.analyze_all_devices(days)
    return result


@app.get("/api/v1/predict")
async def predict_activity(
    device_id: str = Query(..., description="Device ID"),
    hours: int = Query(24, description="Hours to forecast"),
):
    """Predict bird activity for upcoming hours."""
    result = analyzer.predict_activity(device_id, hours)
    return result


# -------------------- Device Endpoints --------------------

@app.post("/api/v1/devices/heartbeat")
async def device_heartbeat(data: DeviceHeartbeat):
    """Receive device heartbeat update."""
    db.upsert_device_status(data.model_dump())
    return {"status": "ok", "device_id": data.device_id}


@app.get("/api/v1/devices")
async def list_devices():
    """List all registered devices."""
    devices = db.get_all_devices()
    return {"devices": devices, "count": len(devices)}


@app.get("/api/v1/devices/{device_id}")
async def get_device(device_id: str):
    """Get device status by ID."""
    status = db.get_device_status(device_id)
    if not status:
        raise HTTPException(status_code=404, detail="Device not found")
    return status


# -------------------- Alert Endpoints --------------------

@app.get("/api/v1/alerts")
async def get_alerts(
    device_id: Optional[str] = Query(None),
    status: str = Query("active", description="active/resolved/all"),
):
    """Get alerts."""
    if status == "active":
        alerts = alert_service.get_active_alerts(device_id)
    else:
        # Could add resolved query here
        alerts = alert_service.get_active_alerts(device_id)
    return {"alerts": alerts, "count": len(alerts)}


@app.post("/api/v1/alerts/{alert_id}/resolve")
async def resolve_alert(alert_id: int, data: AlertResolveRequest):
    """Resolve an alert."""
    alert_service.resolve_alert(alert_id, data.resolved_by)
    return {"status": "resolved", "alert_id": alert_id}


# -------------------- Species Endpoints --------------------

@app.get("/api/v1/species")
async def list_species():
    """List known bird species with pest/beneficial classification."""
    pest_classes = config.get("detection", {}).get("pest_classes", [])
    return {
        "pest_species": pest_classes,
        "beneficial_species": [
            "swallow", "robin", "egret", "heron", "eagle", "hawk", "owl"
        ],
    }


# -------------------- Background Tasks --------------------

def _evaluate_alerts(device_id: str) -> None:
    """Background task: evaluate alerts for a device."""
    try:
        recent = db.query_detections(
            device_id=device_id,
            limit=100,
        )
        triggered = alert_service.evaluate_all(device_id, recent)
        if triggered:
            logger.info(f"Triggered {len(triggered)} alerts for {device_id}")
    except Exception as e:
        logger.error(f"Alert evaluation failed for {device_id}: {e}")


# -------------------- Entry Point --------------------

def main() -> None:
    """Run the FastAPI server."""
    import uvicorn
    server_cfg = config.get("server", {})
    uvicorn.run(
        "src.app:app",
        host=server_cfg.get("host", "0.0.0.0"),
        port=server_cfg.get("port", 8000),
        reload=server_cfg.get("debug", False),
    )


if __name__ == "__main__":
    main()
