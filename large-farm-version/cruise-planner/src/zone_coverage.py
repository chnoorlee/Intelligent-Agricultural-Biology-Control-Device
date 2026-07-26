"""Zone coverage algorithms for systematic area-scanning.

Implements boustrophedon (ox-plow) and spiral coverage patterns
for complete field coverage by fixed-wing drones.
"""

import math
from typing import List, Tuple
import numpy as np
from .path_planner import Waypoint, haversine_distance


class ZoneCoveragePlanner:
    """Generates waypoints for complete coverage of a polygonal zone.

    Supports boustrophedon (back-and-forth) and spiral coverage patterns
    optimized for fixed-wing flight characteristics.
    """

    def __init__(
        self,
        swath_width_m: float = 30.0,
        overlap_ratio: float = 0.2,
        turn_radius_m: float = 50.0,
        altitude_m: float = 50.0,
        speed_ms: float = 15.0,
    ) -> None:
        """Initialize coverage planner.

        Args:
            swath_width_m: Effective swath width of ultrasound deterrent (meters).
            overlap_ratio: Overlap between adjacent swaths (0-1).
            turn_radius_m: Minimum turn radius of fixed-wing drone.
            altitude_m: Cruise altitude.
            speed_ms: Cruise speed.
        """
        self.swath_width = swath_width_m
        self.overlap = overlap_ratio
        self.turn_radius = turn_radius_m
        self.altitude = altitude_m
        self.speed = speed_ms

        self.effective_swath = swath_width_m * (1 - overlap_ratio)

    def boustrophedon_coverage(
        self,
        boundary: List[Tuple[float, float]],
        sweep_angle_deg: float = 0.0,
    ) -> List[Waypoint]:
        """Generate boustrophedon (ox-plow) coverage pattern.

        Systematically sweeps back and forth across the field,
        ideal for rectangular or near-rectangular zones.

        Args:
            boundary: List of (lat, lng) polygon vertices (clockwise).
            sweep_angle_deg: Sweep direction angle (0 = N-S, 90 = E-W).

        Returns:
            Ordered waypoints for complete coverage.
        """
        if len(boundary) < 3:
            return []

        bounds = self._compute_bounds(boundary)
        waypoints: List[Waypoint] = []

        # Rotate sweep lines
        angle_rad = math.radians(sweep_angle_deg)

        # Generate parallel sweep lines
        sweep_lines = self._generate_sweep_lines(bounds, angle_rad)

        # For each sweep line, find intersection with polygon
        toggle = True
        for line_y in sweep_lines:
            intersections = self._line_polygon_intersections(
                line_y, angle_rad, boundary, bounds
            )

            if len(intersections) >= 2:
                i1, i2 = sorted(intersections)
                if toggle:
                    wp1 = Waypoint(i1[0], i1[1], self.altitude, self.speed, "cruise")
                    wp2 = Waypoint(i2[0], i2[1], self.altitude, self.speed, "cruise")
                else:
                    wp2 = Waypoint(i1[0], i1[1], self.altitude, self.speed, "cruise")
                    wp1 = Waypoint(i2[0], i2[1], self.altitude, self.speed, "cruise")

                waypoints.append(wp1)
                waypoints.append(wp2)

                # Add turn waypoint at end of each swath
                if len(waypoints) >= 2:
                    turn_wp = self._add_turn_waypoint(wp2, angle_rad, toggle)
                    if turn_wp:
                        waypoints.append(turn_wp)

                toggle = not toggle

        return waypoints

    def spiral_coverage(
        self,
        center: Tuple[float, float],
        radius_m: float,
        clockwise: bool = True,
    ) -> List[Waypoint]:
        """Generate spiral coverage pattern from center outward.

        Suitable for circular or irregular zones.

        Args:
            center: (lat, lng) center point.
            radius_m: Coverage radius in meters.
            clockwise: Spiral direction.

        Returns:
            Ordered waypoints for spiral coverage.
        """
        waypoints: List[Waypoint] = []
        center_lat, center_lng = center

        # Number of spiral loops
        num_loops = int(radius_m / self.effective_swath) + 1
        points_per_loop = 36  # 10 degrees per point

        direction = -1 if clockwise else 1

        for loop in range(num_loops + 1):
            r = loop * self.effective_swath
            if r > radius_m:
                r = radius_m

            for i in range(points_per_loop):
                angle = direction * i * (2 * math.pi / points_per_loop)
                # Spiral angle advance per loop
                angle += loop * 0.2

                # Convert polar to GPS offset
                lat_offset = r * math.cos(angle) / 111320.0
                lng_offset = r * math.sin(angle) / (
                    111320.0 * math.cos(math.radians(center_lat))
                )

                wp = Waypoint(
                    center_lat + lat_offset,
                    center_lng + lng_offset,
                    self.altitude,
                    self.speed,
                    "cruise",
                )
                waypoints.append(wp)

        return waypoints

    def _compute_bounds(
        self, boundary: List[Tuple[float, float]]
    ) -> Tuple[float, float, float, float]:
        """Compute bounding box of polygon."""
        lats = [p[0] for p in boundary]
        lngs = [p[1] for p in boundary]
        return (min(lats), min(lngs), max(lats), max(lngs))

    def _generate_sweep_lines(
        self,
        bounds: Tuple[float, float, float, float],
        angle_rad: float,
    ) -> List[float]:
        """Generate parallel sweep line positions."""
        min_lat, min_lng, max_lat, max_lng = bounds
        span = haversine_distance(min_lat, min_lng, max_lat, max_lng)
        num_lines = int(span / self.effective_swath) + 2

        lines = []
        for i in range(num_lines):
            offset = i * self.effective_swath
            lines.append(offset)
        return lines

    def _line_polygon_intersections(
        self,
        line_pos: float,
        angle_rad: float,
        boundary: List[Tuple[float, float]],
        bounds: Tuple[float, float, float, float],
    ) -> List[Tuple[float, float]]:
        """Find intersections of a sweep line with polygon boundary."""
        min_lat, min_lng, max_lat, max_lng = bounds

        # Sweep line in lat-lng space
        cos_a = math.cos(angle_rad)
        sin_a = math.sin(angle_rad)

        # Line equation: sin_a * (lat - min_lat) - cos_a * (lng - min_lng) = line_pos / 111320
        # Simplified: sample points along the perpendicular direction
        intersections = []
        step = 0.0001  # ~11m resolution

        current_lat = min_lat - step
        while current_lat <= max_lat + step:
            current_lng = min_lng - step
            while current_lng <= max_lng + step:
                # Check if point is near sweep line
                dist = abs(
                    sin_a * (current_lat - min_lat)
                    - cos_a * (current_lng - min_lng)
                    - line_pos / 111320.0
                )
                if dist < 0.00005:  # ~5.5m tolerance
                    if self._point_in_polygon(current_lat, current_lng, boundary):
                        intersections.append((current_lat, current_lng))
                        break
                current_lng += step
            current_lat += step

        # Extract endpoints of intersection segments
        if len(intersections) >= 2:
            return [intersections[0], intersections[-1]]
        return intersections

    def _point_in_polygon(
        self, lat: float, lng: float, polygon: List[Tuple[float, float]]
    ) -> bool:
        """Ray-casting point-in-polygon test."""
        n = len(polygon)
        inside = False
        j = n - 1

        for i in range(n):
            lat_i, lng_i = polygon[i]
            lat_j, lng_j = polygon[j]

            if ((lng_i > lng) != (lng_j > lng)) and (
                lat < (lat_j - lat_i) * (lng - lng_i) / (lng_j - lng_i) + lat_i
            ):
                inside = not inside
            j = i

        return inside

    def _add_turn_waypoint(
        self,
        last_wp: Waypoint,
        sweep_angle_rad: float,
        toggle: bool,
    ) -> Waypoint:
        """Add a turn-around waypoint at end of swath."""
        turn_dist = self.turn_radius / 111320.0
        turn_dir = 1 if toggle else -1

        turn_lat = last_wp.lat + turn_dir * turn_dist * math.sin(sweep_angle_rad)
        turn_lng = last_wp.lng - turn_dir * turn_dist * math.cos(sweep_angle_rad)

        return Waypoint(turn_lat, turn_lng, self.altitude, self.speed * 0.6, "turn")


def main() -> None:
    """Demo coverage planning."""
    planner = ZoneCoveragePlanner(
        swath_width_m=30.0,
        overlap_ratio=0.2,
    )

    # Demo field boundary (Ningbo area)
    boundary = [
        (29.8620, 121.5400),
        (29.8620, 121.5420),
        (29.8635, 121.5420),
        (29.8635, 121.5400),
    ]

    waypoints = planner.boustrophedon_coverage(boundary, sweep_angle_deg=0)
    print(f"Generated {len(waypoints)} waypoints for coverage")
    for wp in waypoints[:5]:
        print(f"  {wp.lat:.6f}, {wp.lng:.6f} [{wp.action}]")


if __name__ == "__main__":
    main()
