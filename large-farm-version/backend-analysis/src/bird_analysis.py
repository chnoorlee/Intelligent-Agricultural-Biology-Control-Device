"""Bird behavior analysis module.

Analyzes spatiotemporal patterns in bird detection data to identify
activity trends, predict peak periods, and generate deterrence insights.
"""

import logging
from typing import List, Dict, Any, Optional
from datetime import datetime, timedelta
from collections import defaultdict
import numpy as np

from .database import Database
from .statistics import StatisticsEngine
from .deepseek_client import DeepSeekClient

logger = logging.getLogger(__name__)


class BirdBehaviorAnalyzer:
    """Analyzes bird behavior patterns from detection history."""

    def __init__(
        self,
        database: Database,
        deepseek_client: Optional[DeepSeekClient] = None,
    ) -> None:
        """Initialize analyzer.

        Args:
            database: Database instance.
            deepseek_client: Optional DeepSeek client for AI analysis.
        """
        self.db = database
        self.deepseek = deepseek_client
        self.stats = StatisticsEngine()

    def analyze_device(
        self,
        device_id: str,
        days: int = 7,
        use_ai: bool = False,
    ) -> Dict[str, Any]:
        """Comprehensive analysis for a single device.

        Args:
            device_id: Device identifier.
            days: Number of days of data to analyze.
            use_ai: Whether to use DeepSeek AI analysis.

        Returns:
            Analysis results dictionary.
        """
        start_time = (datetime.now() - timedelta(days=days)).isoformat()
        detections = self.db.query_detections(
            device_id=device_id,
            start_time=start_time,
            limit=10000,
        )

        if not detections:
            return {
                "device_id": device_id,
                "message": f"No detection data for last {days} days",
                "days_analyzed": days,
            }

        # Compute statistics
        summary = self.stats.compute_summary(detections, device_id)
        hotspots = self._extract_hotspots(detections)
        trends = self._analyze_trends(detections, days)
        peak_periods = self._identify_peak_periods(detections)

        result = {
            "device_id": device_id,
            "days_analyzed": days,
            "total_detections": summary["total_records"],
            "species_distribution": summary["species_distribution"],
            "hourly_activity": summary["hourly_activity"],
            "deterrence": summary["deterrence"],
            "spatial_hotspots": hotspots,
            "activity_trends": trends,
            "peak_periods": peak_periods,
            "time_range": summary["time_range"],
        }

        # AI-powered analysis (optional, uses API credits)
        if use_ai and self.deepseek:
            try:
                ai_analysis = self.deepseek.analyze_bird_patterns({
                    "species_distribution": summary["species_distribution"],
                    "hourly_activity": summary["hourly_activity"],
                    "spatial_hotspots": hotspots[:5],
                    "deter_success_rate": summary["deterrence"]["overall_rate"],
                })
                result["ai_insights"] = ai_analysis
            except Exception as e:
                logger.error(f"AI analysis failed: {e}")
                result["ai_insights"] = {"error": str(e)}

        return result

    def analyze_all_devices(
        self,
        days: int = 7,
    ) -> Dict[str, Any]:
        """Analyze bird patterns across all devices.

        Args:
            days: Days of data to analyze.

        Returns:
            Multi-device analysis results.
        """
        devices = self.db.get_all_devices()
        device_analyses = {}
        all_detections = []

        for device in devices:
            device_id = device["device_id"]
            analysis = self.analyze_device(device_id, days, use_ai=False)
            device_analyses[device_id] = analysis

            detections = self.db.query_detections(
                device_id=device_id,
                start_time=(datetime.now() - timedelta(days=days)).isoformat(),
                limit=10000,
            )
            all_detections.extend(detections)

        # Cross-device summary
        cross_device = self.stats.compute_summary(all_detections)

        return {
            "devices_analyzed": len(devices),
            "days": days,
            "total_detections": cross_device["total_records"],
            "per_device": device_analyses,
            "overall_summary": {
                "species": cross_device["species_distribution"],
                "hourly": cross_device["hourly_activity"],
                "deterrence": cross_device["deterrence"],
            },
            "spatial_clusters": cross_device.get("spatial_clusters", []),
        }

    def predict_activity(
        self,
        device_id: str,
        forecast_hours: int = 24,
    ) -> Dict[str, Any]:
        """Predict bird activity for upcoming hours.

        Args:
            device_id: Device identifier.
            forecast_hours: Hours to forecast ahead.

        Returns:
            Activity prediction.
        """
        # Get historical data for pattern learning
        historical = self.db.query_detections(
            device_id=device_id,
            start_time=(datetime.now() - timedelta(days=14)).isoformat(),
            limit=5000,
        )

        if not historical:
            return {"device_id": device_id, "message": "Insufficient historical data"}

        # Compute hourly patterns
        hourly = self.stats.hourly_activity(historical)
        peak_hours = sorted(
            range(24),
            key=lambda h: hourly[h]["count"],
            reverse=True,
        )[:3]

        # Simple heuristic prediction
        now = datetime.now()
        predictions = []
        for h in range(forecast_hours):
            hour_of_day = (now.hour + h) % 24
            base_activity = hourly[hour_of_day]["count"]

            # Adjust: mornings and evenings typically higher
            time_factor = 1.0
            if 5 <= hour_of_day <= 8:  # Dawn peak
                time_factor = 1.5
            elif 16 <= hour_of_day <= 19:  # Dusk peak
                time_factor = 1.3

            level = "low"
            if base_activity > 10:
                level = "high"
            elif base_activity > 5:
                level = "medium"

            predictions.append({
                "timestamp": (now + timedelta(hours=h)).isoformat(),
                "hour_of_day": hour_of_day,
                "predicted_level": level,
                "expected_count": int(base_activity * time_factor),
                "confidence": min(0.9, 0.5 + base_activity / 50),
                "recommended_action": (
                    "intensive_patrol" if level == "high"
                    else "standard_patrol" if level == "medium"
                    else "light_patrol"
                ),
            })

        # AI-enhanced prediction (if available)
        ai_prediction = None
        if self.deepseek:
            try:
                ai_prediction = self.deepseek.predict_bird_activity(
                    historical, forecast_hours
                )
            except Exception as e:
                logger.error(f"AI prediction failed: {e}")

        return {
            "device_id": device_id,
            "forecast_hours": forecast_hours,
            "peak_hours_historical": peak_hours,
            "predictions": predictions,
            "ai_enhanced": ai_prediction,
        }

    # -------------------- Helpers --------------------

    def _extract_hotspots(
        self, detections: List[Dict[str, Any]]
    ) -> List[Dict[str, Any]]:
        """Extract spatial hotspots from detections."""
        clusters = self.stats.spatial_clustering(detections)
        return [
            {
                "lat": c["centroid_lat"],
                "lng": c["centroid_lng"],
                "total_birds": c["total_birds"],
                "radius_m": c["radius_m"],
            }
            for c in clusters[:10]
        ]

    def _analyze_trends(
        self, detections: List[Dict[str, Any]], days: int
    ) -> Dict[str, Any]:
        """Analyze activity trends over time."""
        # Daily counts
        daily_counts = defaultdict(int)
        for det in detections:
            ts = det.get("timestamp", "")
            try:
                date = datetime.fromisoformat(ts).strftime("%Y-%m-%d")
                daily_counts[date] += det.get("count", 1)
            except (ValueError, TypeError):
                pass

        dates = sorted(daily_counts.keys())
        if len(dates) < 3:
            return {"trend": "insufficient_data"}

        counts = [daily_counts[d] for d in dates]

        # Simple linear regression for trend direction
        x = np.arange(len(counts))
        slope = np.polyfit(x, counts, 1)[0]

        if slope > 1:
            trend = "increasing"
        elif slope < -1:
            trend = "decreasing"
        else:
            trend = "stable"

        return {
            "trend": trend,
            "slope": float(slope),
            "daily_counts": dict(sorted(daily_counts.items())[-days:]),
            "avg_daily": float(np.mean(counts)),
            "max_daily": int(max(counts)),
            "max_daily_date": dates[counts.index(max(counts))],
        }

    def _identify_peak_periods(
        self, detections: List[Dict[str, Any]]
    ) -> List[Dict[str, Any]]:
        """Identify peak bird activity periods."""
        hourly = self.stats.hourly_activity(detections)
        avg_count = np.mean([h["count"] for h in hourly])

        peaks = []
        for h in hourly:
            if h["count"] > avg_count * 1.5:  # 50% above average
                peaks.append({
                    "hour": h["hour"],
                    "count": h["count"],
                    "intensity": "high" if h["count"] > avg_count * 2.5 else "medium",
                })

        return sorted(peaks, key=lambda p: p["count"], reverse=True)
