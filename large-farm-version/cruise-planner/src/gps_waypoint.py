"""GPS waypoint management: KML/GPX import/export and coordinate transforms.

Supports loading waypoint files, coordinate system conversions,
and generating flight plans for drone autopilot systems.
"""

import math
import json
import xml.etree.ElementTree as ET
from typing import List, Tuple, Optional
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from .path_planner import Waypoint, haversine_distance


@dataclass
class FlightPlan:
    """Complete flight plan with metadata."""

    name: str
    waypoints: List[Waypoint]
    created_at: datetime = field(default_factory=datetime.now)
    description: str = ""
    home_location: Optional[Tuple[float, float]] = None

    @property
    def total_distance_m(self) -> float:
        """Total flight distance in meters."""
        if len(self.waypoints) < 2:
            return 0.0
        dist = 0.0
        for i in range(len(self.waypoints) - 1):
            dist += haversine_distance(
                self.waypoints[i].lat, self.waypoints[i].lng,
                self.waypoints[i + 1].lat, self.waypoints[i + 1].lng,
            )
        return dist

    @property
    def max_altitude_m(self) -> float:
        """Maximum altitude in flight plan."""
        if not self.waypoints:
            return 0.0
        return max(wp.altitude for wp in self.waypoints)


class WaypointManager:
    """Manages waypoint import, export, and coordinate conversions."""

    # WGS84 constants
    EARTH_RADIUS_M = 6378137.0
    ECCENTRICITY_SQ = 0.00669437999014

    @staticmethod
    def import_kml(filepath: str) -> FlightPlan:
        """Import waypoints from KML file.

        Args:
            filepath: Path to .kml file.

        Returns:
            FlightPlan with imported waypoints.
        """
        tree = ET.parse(filepath)
        root = tree.getroot()

        ns = {"kml": "http://www.opengis.net/kml/2.2"}
        name = "Imported Flight Plan"
        description = ""

        # Get document name
        doc_name = root.find(".//kml:Document/kml:name", ns)
        if doc_name is not None:
            name = doc_name.text or name

        waypoints: List[Waypoint] = []

        # Parse Placemarks
        for pm in root.findall(".//kml:Placemark", ns):
            pm_name = pm.find("kml:name", ns)
            coords_elem = pm.find(".//kml:coordinates", ns)

            if coords_elem is not None and coords_elem.text:
                coords_text = coords_elem.text.strip()
                for line in coords_text.split():
                    parts = line.split(",")
                    if len(parts) >= 2:
                        lng, lat = float(parts[0]), float(parts[1])
                        alt = float(parts[2]) if len(parts) >= 3 else 50.0
                        waypoints.append(Waypoint(
                            lat=lat,
                            lng=lng,
                            altitude=alt,
                            action="cruise",
                            id=pm_name.text if pm_name is not None else "",
                        ))

        return FlightPlan(
            name=name,
            waypoints=waypoints,
            description=description,
        )

    @staticmethod
    def export_kml(flight_plan: FlightPlan, filepath: str) -> None:
        """Export flight plan to KML format.

        Args:
            flight_plan: FlightPlan to export.
            filepath: Output .kml path.
        """
        kml = ET.Element("kml", xmlns="http://www.opengis.net/kml/2.2")
        doc = ET.SubElement(kml, "Document")
        ET.SubElement(doc, "name").text = flight_plan.name
        ET.SubElement(doc, "description").text = flight_plan.description

        for i, wp in enumerate(flight_plan.waypoints):
            pm = ET.SubElement(doc, "Placemark")
            ET.SubElement(pm, "name").text = f"WP{i:03d}"
            ET.SubElement(pm, "description").text = f"Action: {wp.action}, Speed: {wp.speed}m/s"

            point = ET.SubElement(pm, "Point")
            coords = f"{wp.lng:.8f},{wp.lat:.8f},{wp.altitude:.1f}"
            ET.SubElement(point, "coordinates").text = coords

        tree = ET.ElementTree(kml)
        tree.write(filepath, encoding="utf-8", xml_declaration=True)
        print(f"Exported {len(flight_plan.waypoints)} waypoints to {filepath}")

    @staticmethod
    def import_gpx(filepath: str) -> FlightPlan:
        """Import waypoints from GPX file.

        Args:
            filepath: Path to .gpx file.

        Returns:
            FlightPlan with imported waypoints.
        """
        tree = ET.parse(filepath)
        root = tree.getroot()

        ns = {"gpx": "http://www.topografix.com/GPX/1/1"}
        name = "Imported GPX Route"

        trk_name = root.find(".//gpx:trk/gpx:name", ns)
        if trk_name is not None:
            name = trk_name.text or name

        waypoints: List[Waypoint] = []

        for trkpt in root.findall(".//gpx:trkpt", ns):
            lat = float(trkpt.get("lat", "0"))
            lng = float(trkpt.get("lon", "0"))

            ele_elem = trkpt.find("gpx:ele", ns)
            alt = float(ele_elem.text) if ele_elem is not None and ele_elem.text else 50.0

            waypoints.append(Waypoint(lat=lat, lng=lng, altitude=alt, action="cruise"))

        return FlightPlan(name=name, waypoints=waypoints)

    @staticmethod
    def export_gpx(flight_plan: FlightPlan, filepath: str) -> None:
        """Export flight plan to GPX format."""
        gpx = ET.Element("gpx",
                         version="1.1",
                         xmlns="http://www.topografix.com/GPX/1/1")
        trk = ET.SubElement(gpx, "trk")
        ET.SubElement(trk, "name").text = flight_plan.name

        trkseg = ET.SubElement(trk, "trkseg")
        for wp in flight_plan.waypoints:
            trkpt = ET.SubElement(trkseg, "trkpt",
                                  lat=f"{wp.lat:.8f}",
                                  lon=f"{wp.lng:.8f}")
            ET.SubElement(trkpt, "ele").text = f"{wp.altitude:.1f}"

        tree = ET.ElementTree(gpx)
        tree.write(filepath, encoding="utf-8", xml_declaration=True)

    @staticmethod
    def export_mission_planner(
        flight_plan: FlightPlan, filepath: str
    ) -> None:
        """Export as ArduPilot Mission Planner waypoint file.

        Args:
            flight_plan: FlightPlan to export.
            filepath: Output .waypoints path.
        """
        lines = ["QGC WPL 110"]

        # Home position (index 0)
        home_lat = flight_plan.home_location[0] if flight_plan.home_location else flight_plan.waypoints[0].lat
        home_lng = flight_plan.home_location[1] if flight_plan.home_location else flight_plan.waypoints[0].lng
        lines.append(f"0\t1\t0\t16\t0\t0\t0\t0\t{home_lat:.8f}\t{home_lng:.8f}\t0\t1")

        # Waypoints
        for i, wp in enumerate(flight_plan.waypoints, start=1):
            # MAV_CMD_NAV_WAYPOINT = 16
            lines.append(
                f"{i}\t0\t3\t16\t0\t0\t0\t0\t"
                f"{wp.lat:.8f}\t{wp.lng:.8f}\t{wp.altitude:.1f}\t1"
            )

        Path(filepath).write_text("\n".join(lines), encoding="utf-8")
        print(f"Exported Mission Planner waypoints to {filepath}")

    @staticmethod
    def latlng_to_utm(
        lat: float, lng: float
    ) -> Tuple[int, float, float]:
        """Convert WGS84 lat/lng to UTM coordinates.

        Args:
            lat: Latitude in degrees.
            lng: Longitude in degrees.

        Returns:
            (zone_number, easting_m, northing_m).
        """
        zone = int((lng + 180) / 6) + 1

        lat_rad = math.radians(lat)
        lng_rad = math.radians(lng)

        # UTM central meridian
        lon0 = math.radians((zone - 1) * 6 - 180 + 3)

        # Meridional arc
        N = WaypointManager.EARTH_RADIUS_M / math.sqrt(
            1 - WaypointManager.ECCENTRICITY_SQ * math.sin(lat_rad) ** 2
        )
        T = math.tan(lat_rad) ** 2
        C = WaypointManager.ECCENTRICITY_SQ * math.cos(lat_rad) ** 2 / (
            1 - WaypointManager.ECCENTRICITY_SQ
        )
        A = math.cos(lat_rad) * (lng_rad - lon0)

        M = WaypointManager.EARTH_RADIUS_M * (
            (1 - WaypointManager.ECCENTRICITY_SQ / 4
             - 3 * WaypointManager.ECCENTRICITY_SQ ** 2 / 64
             - 5 * WaypointManager.ECCENTRICITY_SQ ** 3 / 256) * lat_rad
            - (3 * WaypointManager.ECCENTRICITY_SQ / 8
               + 3 * WaypointManager.ECCENTRICITY_SQ ** 2 / 32
               + 45 * WaypointManager.ECCENTRICITY_SQ ** 3 / 1024) * math.sin(2 * lat_rad)
            + (15 * WaypointManager.ECCENTRICITY_SQ ** 2 / 256
               + 45 * WaypointManager.ECCENTRICITY_SQ ** 3 / 1024) * math.sin(4 * lat_rad)
            - (35 * WaypointManager.ECCENTRICITY_SQ ** 3 / 3072) * math.sin(6 * lat_rad)
        )

        easting = 500000 + N * (
            A + (1 - T + C) * A ** 3 / 6
            + (5 - 18 * T + T ** 2 + 72 * C - 58) * A ** 5 / 120
        )

        northing = M + N * math.tan(lat_rad) * (
            A ** 2 / 2
            + (5 - T + 9 * C + 4 * C ** 2) * A ** 4 / 24
            + (61 - 58 * T + T ** 2 + 600 * C - 330) * A ** 6 / 720
        )

        if lat < 0:
            northing += 10000000

        return (zone, easting, northing)

    @staticmethod
    def simplify_waypoints(
        waypoints: List[Waypoint],
        tolerance_m: float = 5.0,
    ) -> List[Waypoint]:
        """Ramer-Douglas-Peucker waypoint simplification.

        Args:
            waypoints: Input waypoint list.
            tolerance_m: Simplification tolerance in meters.

        Returns:
            Simplified waypoint list.
        """
        if len(waypoints) <= 2:
            return waypoints

        # Find point with maximum distance from line
        max_dist = 0.0
        max_idx = 0

        start = waypoints[0]
        end = waypoints[-1]

        for i in range(1, len(waypoints) - 1):
            dist = WaypointManager._perpendicular_distance(
                waypoints[i], start, end
            )
            if dist > max_dist:
                max_dist = dist
                max_idx = i

        if max_dist > tolerance_m:
            left = WaypointManager.simplify_waypoints(
                waypoints[:max_idx + 1], tolerance_m
            )
            right = WaypointManager.simplify_waypoints(
                waypoints[max_idx:], tolerance_m
            )
            return left[:-1] + right
        else:
            return [start, end]

    @staticmethod
    def _perpendicular_distance(
        point: Waypoint, line_start: Waypoint, line_end: Waypoint
    ) -> float:
        """Calculate perpendicular distance from point to line segment in meters."""
        # Use UTM for accurate distance
        _, px, py = WaypointManager.latlng_to_utm(point.lat, point.lng)
        _, sx, sy = WaypointManager.latlng_to_utm(line_start.lat, line_start.lng)
        _, ex, ey = WaypointManager.latlng_to_utm(line_end.lat, line_end.lng)

        dx = ex - sx
        dy = ey - sy

        if dx == 0 and dy == 0:
            return math.sqrt((px - sx) ** 2 + (py - sy) ** 2)

        t = ((px - sx) * dx + (py - sy) * dy) / (dx * dx + dy * dy)
        t = max(0.0, min(1.0, t))

        proj_x = sx + t * dx
        proj_y = sy + t * dy

        return math.sqrt((px - proj_x) ** 2 + (py - proj_y) ** 2)


def main() -> None:
    """Demo waypoint management."""
    # Create sample flight plan
    plan = FlightPlan(
        name="Farm Patrol Route",
        waypoints=[
            Waypoint(29.8620, 121.5400, 50, 15, "takeoff"),
            Waypoint(29.8630, 121.5410, 50, 15, "cruise"),
            Waypoint(29.8640, 121.5420, 50, 15, "cruise"),
            Waypoint(29.8635, 121.5430, 50, 15, "cruise"),
            Waypoint(29.8625, 121.5420, 50, 15, "land"),
        ],
        home_location=(29.8620, 121.5400),
    )

    mgr = WaypointManager()

    # Export formats
    mgr.export_kml(plan, "flight_plan.kml")
    mgr.export_gpx(plan, "flight_plan.gpx")
    mgr.export_mission_planner(plan, "flight_plan.waypoints")

    # UTM conversion
    zone, e, n = mgr.latlng_to_utm(29.863, 121.541)
    print(f"UTM Zone {zone}: E={e:.1f}, N={n:.1f}")

    # Simplify
    simplified = mgr.simplify_waypoints(plan.waypoints, tolerance_m=10.0)
    print(f"Simplified from {len(plan.waypoints)} to {len(simplified)} waypoints")


if __name__ == "__main__":
    main()
