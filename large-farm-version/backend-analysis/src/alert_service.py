"""Alert service for bird analysis backend.

Monitors detection data and device status, generates alerts
for high-density bird activity, device anomalies, and low battery.
"""

import logging
from typing import List, Dict, Any, Optional
from datetime import datetime, timedelta
from .database import Database

logger = logging.getLogger(__name__)


class AlertRule:
    """Base alert rule."""

    def __init__(self, name: str, severity: str = "warning") -> None:
        self.name = name
        self.severity = severity

    def evaluate(self, context: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Evaluate alert condition. Return alert dict or None."""
        raise NotImplementedError


class HighDensityAlert(AlertRule):
    """Alert when bird density exceeds threshold within time window."""

    def __init__(
        self,
        threshold: int = 50,
        window_minutes: int = 10,
        severity: str = "high",
    ) -> None:
        super().__init__("high_bird_density", severity)
        self.threshold = threshold
        self.window_minutes = window_minutes

    def evaluate(self, context: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Check if recent detections exceed density threshold."""
        detections: List[Dict[str, Any]] = context.get("recent_detections", [])
        if not detections:
            return None

        cutoff = datetime.now() - timedelta(minutes=self.window_minutes)
        recent_count = sum(
            d.get("count", 1)
            for d in detections
            if d.get("timestamp", "") >= cutoff.isoformat()
        )

        if recent_count >= self.threshold:
            species = set(d.get("class_name", "?") for d in detections[-10:])
            return {
                "alert_type": self.name,
                "severity": self.severity,
                "message": (
                    f"High bird density detected: {recent_count} birds "
                    f"in last {self.window_minutes}min. "
                    f"Species: {', '.join(species)}. "
                    f"Recommend switching to manual mode."
                ),
                "details": {
                    "bird_count": recent_count,
                    "window_minutes": self.window_minutes,
                    "species": list(species),
                },
            }
        return None


class BatteryLowAlert(AlertRule):
    """Alert when device battery drops below threshold."""

    def __init__(self, threshold: float = 20.0, severity: str = "high") -> None:
        super().__init__("battery_low", severity)
        self.threshold = threshold

    def evaluate(self, context: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        device_status = context.get("device_status", {})
        soc = device_status.get("battery_soc")

        if soc is not None and soc <= self.threshold:
            return {
                "alert_type": self.name,
                "severity": self.severity,
                "message": (
                    f"Battery low: {soc:.1f}% SOC remaining. "
                    f"Device: {device_status.get('device_id', 'unknown')}. "
                    f"Auto-return recommended."
                ),
                "details": {
                    "battery_soc": soc,
                    "battery_voltage": device_status.get("battery_voltage"),
                    "threshold": self.threshold,
                },
            }
        return None


class SignalLossAlert(AlertRule):
    """Alert when device stops sending heartbeat."""

    def __init__(self, timeout_seconds: int = 30, severity: str = "critical") -> None:
        super().__init__("signal_loss", severity)
        self.timeout_seconds = timeout_seconds

    def evaluate(self, context: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        device_status = context.get("device_status", {})
        last_heartbeat = device_status.get("last_heartbeat", "")

        if not last_heartbeat:
            return None

        try:
            hb_time = datetime.fromisoformat(last_heartbeat)
            elapsed = (datetime.now() - hb_time).total_seconds()

            if elapsed > self.timeout_seconds:
                return {
                    "alert_type": self.name,
                    "severity": self.severity,
                    "message": (
                        f"Signal lost from {device_status.get('device_id', 'unknown')}. "
                        f"No heartbeat for {elapsed:.0f}s. "
                        f"Last known position: "
                        f"({device_status.get('gps_lat')}, {device_status.get('gps_lng')})"
                    ),
                    "details": {
                        "elapsed_seconds": elapsed,
                        "last_heartbeat": last_heartbeat,
                        "last_position": {
                            "lat": device_status.get("gps_lat"),
                            "lng": device_status.get("gps_lng"),
                        },
                    },
                }
        except (ValueError, TypeError):
            pass

        return None


class DeterFailureAlert(AlertRule):
    """Alert when consecutive deterrence attempts fail."""

    def __init__(self, max_consecutive_failures: int = 3, severity: str = "warning") -> None:
        super().__init__("deter_failure", severity)
        self.max_consecutive_failures = max_consecutive_failures

    def evaluate(self, context: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        detections = context.get("recent_detections", [])
        if not detections:
            return None

        # Count consecutive failures
        consecutive = 0
        for det in reversed(detections):
            if det.get("deter_action") and det.get("deter_success") is False:
                consecutive += 1
            else:
                break

        if consecutive >= self.max_consecutive_failures:
            return {
                "alert_type": self.name,
                "severity": self.severity,
                "message": (
                    f"{consecutive} consecutive deterrence failures. "
                    f"Check ultrasonic module and drone positioning. "
                    f"Consider manual intervention."
                ),
                "details": {
                    "consecutive_failures": consecutive,
                    "threshold": self.max_consecutive_failures,
                },
            }
        return None


class AlertService:
    """Alert monitoring and management service."""

    def __init__(self, database: Database) -> None:
        """Initialize alert service.

        Args:
            database: Database instance for persisting alerts.
        """
        self.db = database
        self.rules: List[AlertRule] = [
            HighDensityAlert(threshold=50, window_minutes=10),
            BatteryLowAlert(threshold=20.0),
            SignalLossAlert(timeout_seconds=30),
            DeterFailureAlert(max_consecutive_failures=3),
        ]

    def add_rule(self, rule: AlertRule) -> None:
        """Add a custom alert rule."""
        self.rules.append(rule)

    def evaluate_all(
        self,
        device_id: str,
        recent_detections: List[Dict[str, Any]],
        device_status: Optional[Dict[str, Any]] = None,
    ) -> List[Dict[str, Any]]:
        """Evaluate all alert rules for a device.

        Args:
            device_id: Device identifier.
            recent_detections: Recent detection records.
            device_status: Current device status (optional).

        Returns:
            List of triggered alert dicts.
        """
        if device_status is None:
            device_status = self.db.get_device_status(device_id) or {}

        context = {
            "device_id": device_id,
            "recent_detections": recent_detections,
            "device_status": device_status,
        }

        triggered = []
        for rule in self.rules:
            try:
                alert_data = rule.evaluate(context)
                if alert_data:
                    alert_data["device_id"] = device_id
                    alert_data["lat"] = device_status.get("gps_lat")
                    alert_data["lng"] = device_status.get("gps_lng")

                    # Persist alert
                    alert_id = self.db.create_alert(
                        device_id=device_id,
                        alert_type=alert_data["alert_type"],
                        severity=alert_data["severity"],
                        message=alert_data["message"],
                        lat=alert_data.get("lat"),
                        lng=alert_data.get("lng"),
                    )
                    alert_data["id"] = alert_id
                    triggered.append(alert_data)

                    logger.warning(
                        f"Alert triggered: {alert_data['alert_type']} "
                        f"[{alert_data['severity']}] for {device_id}"
                    )
            except Exception as e:
                logger.error(f"Alert rule {rule.name} failed: {e}")

        return triggered

    def get_active_alerts(self, device_id: Optional[str] = None) -> List[Dict[str, Any]]:
        """Get unresolved alerts."""
        return self.db.get_active_alerts(device_id)

    def resolve_alert(self, alert_id: int, resolved_by: str = "system") -> None:
        """Resolve an alert."""
        self.db.resolve_alert(alert_id, resolved_by)
