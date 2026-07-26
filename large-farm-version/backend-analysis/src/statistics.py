"""Statistical analysis module for bird detection data.

Computes species distribution, temporal patterns, spatial analysis,
and deterrence effectiveness metrics.
"""

import math
from typing import List, Dict, Any, Optional, Tuple
from collections import Counter, defaultdict
from datetime import datetime, timedelta
import numpy as np


class StatisticsEngine:
    """Computes statistical summaries from bird detection records."""

    @staticmethod
    def species_distribution(
        detections: List[Dict[str, Any]]
    ) -> Dict[str, Dict[str, Any]]:
        """Compute per-species statistics.

        Args:
            detections: List of detection records.

        Returns:
            Dict mapping species name to stats.
        """
        species = defaultdict(lambda: {
            "count": 0,
            "total_individuals": 0,
            "avg_confidence": 0.0,
            "is_pest": False,
            "deter_successes": 0,
            "deter_attempts": 0,
        })

        for det in detections:
            name = det.get("class_name", "unknown")
            s = species[name]
            s["count"] += 1
            s["total_individuals"] += det.get("count", 1)
            s["avg_confidence"] += det.get("confidence", 0)
            s["is_pest"] = bool(det.get("is_pest", True))

            if det.get("deter_action"):
                s["deter_attempts"] += 1
                if det.get("deter_success"):
                    s["deter_successes"] += 1

        # Finalize averages
        for name in species:
            s = species[name]
            if s["count"] > 0:
                s["avg_confidence"] /= s["count"]
            s["deter_rate"] = (
                s["deter_successes"] / s["deter_attempts"]
                if s["deter_attempts"] > 0 else 0.0
            )

        return dict(species)

    @staticmethod
    def hourly_activity(
        detections: List[Dict[str, Any]]
    ) -> List[Dict[str, Any]]:
        """Compute activity levels by hour of day.

        Args:
            detections: List of detection records.

        Returns:
            List of 24 hourly activity entries.
        """
        hours = [{"hour": h, "count": 0, "individuals": 0, "confidence_sum": 0.0,
                  "samples": 0} for h in range(24)]

        for det in detections:
            ts = det.get("timestamp", "")
            try:
                dt = datetime.fromisoformat(ts)
                h = hours[dt.hour]
                h["count"] += 1
                h["individuals"] += det.get("count", 1)
                h["confidence_sum"] += det.get("confidence", 0)
                h["samples"] += 1
            except (ValueError, TypeError):
                continue

        # Compute averages
        for h in hours:
            if h["samples"] > 0:
                h["avg_confidence"] = h["confidence_sum"] / h["samples"]
            else:
                h["avg_confidence"] = 0.0
            del h["confidence_sum"]
            del h["samples"]

        return hours

    @staticmethod
    def deterrence_effectiveness(
        detections: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Compute deterrence success rate and related metrics.

        Args:
            detections: List of detection records.

        Returns:
            Effectiveness metrics.
        """
        total = len(detections)
        with_deter = sum(1 for d in detections if d.get("deter_action"))
        successful = sum(1 for d in detections if d.get("deter_success"))

        # Time-series success rate (per day)
        daily_rates = defaultdict(lambda: {"attempts": 0, "successes": 0})
        for det in detections:
            if det.get("deter_action"):
                ts = det.get("timestamp", "")
                try:
                    date = datetime.fromisoformat(ts).strftime("%Y-%m-%d")
                    daily_rates[date]["attempts"] += 1
                    if det.get("deter_success"):
                        daily_rates[date]["successes"] += 1
                except (ValueError, TypeError):
                    pass

        daily_summary = []
        for date in sorted(daily_rates):
            d = daily_rates[date]
            daily_summary.append({
                "date": date,
                "attempts": d["attempts"],
                "successes": d["successes"],
                "rate": d["successes"] / d["attempts"] if d["attempts"] > 0 else 0,
            })

        return {
            "total_detections": total,
            "with_deterrence": with_deter,
            "successful_deters": successful,
            "overall_rate": successful / with_deter if with_deter > 0 else 0,
            "daily_breakdown": daily_summary,
        }

    @staticmethod
    def spatial_clustering(
        detections: List[Dict[str, Any]],
        cluster_radius_m: float = 100.0,
    ) -> List[Dict[str, Any]]:
        """Simple DBSCAN-like spatial clustering of detections.

        Args:
            detections: List of detection records with lat/lng.
            cluster_radius_m: Max distance for points in same cluster.

        Returns:
            List of clusters with centroid and member count.
        """
        points = []
        for det in detections:
            lat = det.get("lat")
            lng = det.get("lng")
            if lat is not None and lng is not None:
                points.append({
                    "lat": lat,
                    "lng": lng,
                    "count": det.get("count", 1),
                    "class": det.get("class_name", "unknown"),
                })

        if not points:
            return []

        # Greedy clustering
        visited = [False] * len(points)
        clusters = []

        for i in range(len(points)):
            if visited[i]:
                continue

            cluster = [i]
            visited[i] = True
            queue = [i]

            while queue:
                current = queue.pop(0)
                for j in range(len(points)):
                    if visited[j]:
                        continue
                    dist = StatisticsEngine._haversine_distance(
                        points[current]["lat"], points[current]["lng"],
                        points[j]["lat"], points[j]["lng"],
                    )
                    if dist <= cluster_radius_m:
                        visited[j] = True
                        cluster.append(j)
                        queue.append(j)

            # Compute cluster centroid
            lats = [points[k]["lat"] for k in cluster]
            lngs = [points[k]["lng"] for k in cluster]
            total_count = sum(points[k]["count"] for k in cluster)

            clusters.append({
                "centroid_lat": np.mean(lats),
                "centroid_lng": np.mean(lngs),
                "point_count": len(cluster),
                "total_birds": total_count,
                "radius_m": max(
                    StatisticsEngine._haversine_distance(
                        np.mean(lats), np.mean(lngs),
                        points[k]["lat"], points[k]["lng"],
                    )
                    for k in cluster
                ) if len(cluster) > 1 else 0,
            })

        return sorted(clusters, key=lambda c: c["total_birds"], reverse=True)

    @staticmethod
    def compute_summary(
        detections: List[Dict[str, Any]],
        device_id: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Compute comprehensive detection summary.

        Args:
            detections: All detection records.
            device_id: Optional device filter.

        Returns:
            Complete summary statistics.
        """
        if device_id:
            detections = [d for d in detections if d.get("device_id") == device_id]

        return {
            "total_records": len(detections),
            "species_distribution": StatisticsEngine.species_distribution(detections),
            "hourly_activity": StatisticsEngine.hourly_activity(detections),
            "deterrence": StatisticsEngine.deterrence_effectiveness(detections),
            "spatial_clusters": StatisticsEngine.spatial_clustering(detections),
            "time_range": StatisticsEngine._get_time_range(detections),
        }

    @staticmethod
    def _haversine_distance(
        lat1: float, lng1: float, lat2: float, lng2: float
    ) -> float:
        """Haversine distance in meters."""
        R = 6371000
        phi1, phi2 = math.radians(lat1), math.radians(lat2)
        dphi = math.radians(lat2 - lat1)
        dlambda = math.radians(lng2 - lng1)
        a = math.sin(dphi / 2) ** 2 + \
            math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
        return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

    @staticmethod
    def _get_time_range(detections: List[Dict[str, Any]]) -> Dict[str, Optional[str]]:
        """Get time range of detections."""
        if not detections:
            return {"start": None, "end": None}

        timestamps = []
        for d in detections:
            ts = d.get("timestamp", "")
            try:
                timestamps.append(datetime.fromisoformat(ts))
            except (ValueError, TypeError):
                pass

        if not timestamps:
            return {"start": None, "end": None}

        return {
            "start": min(timestamps).isoformat(),
            "end": max(timestamps).isoformat(),
        }
