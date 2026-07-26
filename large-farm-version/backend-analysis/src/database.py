"""SQLite database layer for bird analysis backend.

Stores detection records, device status, alerts, and statistical summaries
with proper indexing for efficient queries.
"""

import sqlite3
import json
import os
import logging
from pathlib import Path
from datetime import datetime, timedelta
from typing import List, Optional, Dict, Any, Tuple
from contextlib import contextmanager

logger = logging.getLogger(__name__)


CREATE_TABLES_SQL = """
CREATE TABLE IF NOT EXISTS detections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    timestamp TEXT NOT NULL,
    class_name TEXT NOT NULL,
    confidence REAL NOT NULL,
    bbox_x1 INTEGER,
    bbox_y1 INTEGER,
    bbox_x2 INTEGER,
    bbox_y2 INTEGER,
    count INTEGER DEFAULT 1,
    lat REAL,
    lng REAL,
    is_pest INTEGER DEFAULT 1,
    deter_action TEXT,
    deter_success INTEGER
);

CREATE TABLE IF NOT EXISTS device_status (
    device_id TEXT PRIMARY KEY,
    device_type TEXT NOT NULL,
    battery_voltage REAL,
    battery_soc REAL,
    is_charging INTEGER,
    gps_lat REAL,
    gps_lng REAL,
    altitude REAL,
    signal_strength INTEGER,
    status TEXT DEFAULT 'online',
    last_heartbeat TEXT,
    firmware_version TEXT
);

CREATE TABLE IF NOT EXISTS alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    alert_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    message TEXT,
    lat REAL,
    lng REAL,
    created_at TEXT NOT NULL,
    resolved_at TEXT,
    resolved_by TEXT
);

CREATE TABLE IF NOT EXISTS daily_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    date TEXT NOT NULL,
    total_detections INTEGER DEFAULT 0,
    pest_detections INTEGER DEFAULT 0,
    successful_deters INTEGER DEFAULT 0,
    peak_hour INTEGER,
    top_species TEXT,
    avg_confidence REAL,
    flight_hours REAL DEFAULT 0,
    distance_km REAL DEFAULT 0,
    UNIQUE(device_id, date)
);

CREATE INDEX IF NOT EXISTS idx_detections_device_time
    ON detections(device_id, timestamp);
CREATE INDEX IF NOT EXISTS idx_detections_class
    ON detections(class_name);
CREATE INDEX IF NOT EXISTS idx_detections_pest
    ON detections(is_pest);
CREATE INDEX IF NOT EXISTS idx_alerts_device_type
    ON alerts(device_id, alert_type);
CREATE INDEX IF NOT EXISTS idx_alerts_active
    ON alerts(resolved_at) WHERE resolved_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_daily_stats_date
    ON daily_stats(date);
"""


class Database:
    """SQLite database manager for bird analysis backend."""

    def __init__(self, db_path: str = "data/bird_analysis.db") -> None:
        """Initialize database.

        Args:
            db_path: Path to SQLite database file.
        """
        self.db_path = db_path
        Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _init_db(self) -> None:
        """Create tables and indexes if they don't exist."""
        with self._get_conn() as conn:
            conn.executescript(CREATE_TABLES_SQL)
            conn.commit()
        logger.info(f"Database initialized at {self.db_path}")

    @contextmanager
    def _get_conn(self):
        """Context manager for database connections."""
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute("PRAGMA foreign_keys=ON")
        try:
            yield conn
        finally:
            conn.close()

    # -------------------- Detections --------------------

    def insert_detection(
        self,
        device_id: str,
        class_name: str,
        confidence: float,
        bbox: Optional[Tuple[int, int, int, int]] = None,
        count: int = 1,
        lat: Optional[float] = None,
        lng: Optional[float] = None,
        is_pest: bool = True,
        deter_action: Optional[str] = None,
        deter_success: Optional[bool] = None,
        timestamp: Optional[str] = None,
    ) -> int:
        """Insert a detection record.

        Returns:
            Row ID of inserted record.
        """
        ts = timestamp or datetime.now().isoformat()

        with self._get_conn() as conn:
            cursor = conn.execute(
                """INSERT INTO detections
                   (device_id, timestamp, class_name, confidence,
                    bbox_x1, bbox_y1, bbox_x2, bbox_y2,
                    count, lat, lng, is_pest, deter_action, deter_success)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                (
                    device_id, ts, class_name, confidence,
                    bbox[0] if bbox else None,
                    bbox[1] if bbox else None,
                    bbox[2] if bbox else None,
                    bbox[3] if bbox else None,
                    count, lat, lng,
                    1 if is_pest else 0,
                    deter_action,
                    1 if deter_success else 0 if deter_success is False else None,
                ),
            )
            conn.commit()
            return cursor.lastrowid

    def query_detections(
        self,
        device_id: Optional[str] = None,
        start_time: Optional[str] = None,
        end_time: Optional[str] = None,
        class_name: Optional[str] = None,
        is_pest: Optional[bool] = None,
        limit: int = 100,
    ) -> List[Dict[str, Any]]:
        """Query detection records with filters."""
        query = "SELECT * FROM detections WHERE 1=1"
        params: list = []

        if device_id:
            query += " AND device_id = ?"
            params.append(device_id)
        if start_time:
            query += " AND timestamp >= ?"
            params.append(start_time)
        if end_time:
            query += " AND timestamp <= ?"
            params.append(end_time)
        if class_name:
            query += " AND class_name = ?"
            params.append(class_name)
        if is_pest is not None:
            query += " AND is_pest = ?"
            params.append(1 if is_pest else 0)

        query += " ORDER BY timestamp DESC LIMIT ?"
        params.append(limit)

        with self._get_conn() as conn:
            rows = conn.execute(query, params).fetchall()
            return [dict(row) for row in rows]

    # -------------------- Device Status --------------------

    def upsert_device_status(self, status: Dict[str, Any]) -> None:
        """Insert or update device status."""
        ts = datetime.now().isoformat()

        with self._get_conn() as conn:
            conn.execute(
                """INSERT INTO device_status
                   (device_id, device_type, battery_voltage, battery_soc,
                    is_charging, gps_lat, gps_lng, altitude,
                    signal_strength, status, last_heartbeat, firmware_version)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                   ON CONFLICT(device_id) DO UPDATE SET
                    battery_voltage = excluded.battery_voltage,
                    battery_soc = excluded.battery_soc,
                    is_charging = excluded.is_charging,
                    gps_lat = excluded.gps_lat,
                    gps_lng = excluded.gps_lng,
                    altitude = excluded.altitude,
                    signal_strength = excluded.signal_strength,
                    status = excluded.status,
                    last_heartbeat = excluded.last_heartbeat""",
                (
                    status.get("device_id"),
                    status.get("device_type", "unknown"),
                    status.get("battery_voltage"),
                    status.get("battery_soc"),
                    1 if status.get("is_charging") else 0,
                    status.get("gps_lat"),
                    status.get("gps_lng"),
                    status.get("altitude"),
                    status.get("signal_strength"),
                    status.get("status", "online"),
                    ts,
                    status.get("firmware_version"),
                ),
            )
            conn.commit()

    def get_device_status(self, device_id: str) -> Optional[Dict[str, Any]]:
        """Get device status by ID."""
        with self._get_conn() as conn:
            row = conn.execute(
                "SELECT * FROM device_status WHERE device_id = ?", (device_id,)
            ).fetchone()
            return dict(row) if row else None

    def get_all_devices(self) -> List[Dict[str, Any]]:
        """Get all device statuses."""
        with self._get_conn() as conn:
            rows = conn.execute("SELECT * FROM device_status").fetchall()
            return [dict(row) for row in rows]

    # -------------------- Alerts --------------------

    def create_alert(
        self,
        device_id: str,
        alert_type: str,
        severity: str,
        message: str,
        lat: Optional[float] = None,
        lng: Optional[float] = None,
    ) -> int:
        """Create an alert record.

        Returns:
            Alert ID.
        """
        ts = datetime.now().isoformat()

        with self._get_conn() as conn:
            cursor = conn.execute(
                """INSERT INTO alerts
                   (device_id, alert_type, severity, message, lat, lng, created_at)
                   VALUES (?, ?, ?, ?, ?, ?, ?)""",
                (device_id, alert_type, severity, message, lat, lng, ts),
            )
            conn.commit()
            return cursor.lastrowid

    def get_active_alerts(self, device_id: Optional[str] = None) -> List[Dict[str, Any]]:
        """Get unresolved alerts."""
        query = "SELECT * FROM alerts WHERE resolved_at IS NULL"
        params: list = []
        if device_id:
            query += " AND device_id = ?"
            params.append(device_id)
        query += " ORDER BY created_at DESC"

        with self._get_conn() as conn:
            rows = conn.execute(query, params).fetchall()
            return [dict(row) for row in rows]

    def resolve_alert(self, alert_id: int, resolved_by: str = "system") -> None:
        """Mark an alert as resolved."""
        ts = datetime.now().isoformat()
        with self._get_conn() as conn:
            conn.execute(
                "UPDATE alerts SET resolved_at = ?, resolved_by = ? WHERE id = ?",
                (ts, resolved_by, alert_id),
            )
            conn.commit()

    # -------------------- Statistics --------------------

    def upsert_daily_stats(self, stats: Dict[str, Any]) -> None:
        """Insert or update daily statistics."""
        with self._get_conn() as conn:
            conn.execute(
                """INSERT INTO daily_stats
                   (device_id, date, total_detections, pest_detections,
                    successful_deters, peak_hour, top_species,
                    avg_confidence, flight_hours, distance_km)
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                   ON CONFLICT(device_id, date) DO UPDATE SET
                    total_detections = excluded.total_detections,
                    pest_detections = excluded.pest_detections,
                    successful_deters = excluded.successful_deters,
                    peak_hour = excluded.peak_hour,
                    top_species = excluded.top_species,
                    avg_confidence = excluded.avg_confidence,
                    flight_hours = excluded.flight_hours,
                    distance_km = excluded.distance_km""",
                (
                    stats["device_id"],
                    stats["date"],
                    stats.get("total_detections", 0),
                    stats.get("pest_detections", 0),
                    stats.get("successful_deters", 0),
                    stats.get("peak_hour"),
                    stats.get("top_species"),
                    stats.get("avg_confidence"),
                    stats.get("flight_hours", 0),
                    stats.get("distance_km", 0),
                ),
            )
            conn.commit()

    def get_daily_stats(
        self,
        device_id: Optional[str] = None,
        date_from: Optional[str] = None,
        date_to: Optional[str] = None,
    ) -> List[Dict[str, Any]]:
        """Query daily statistics."""
        query = "SELECT * FROM daily_stats WHERE 1=1"
        params: list = []

        if device_id:
            query += " AND device_id = ?"
            params.append(device_id)
        if date_from:
            query += " AND date >= ?"
            params.append(date_from)
        if date_to:
            query += " AND date <= ?"
            params.append(date_to)

        query += " ORDER BY date DESC"

        with self._get_conn() as conn:
            rows = conn.execute(query, params).fetchall()
            return [dict(row) for row in rows]
