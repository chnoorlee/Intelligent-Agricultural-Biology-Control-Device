"""Bird density heatmap generation from historical detection data.

Uses kernel density estimation (KDE) and inverse distance weighting (IDW)
to generate spatial heatmaps of bird activity for cruise route optimization.
"""

import math
from typing import List, Tuple, Dict, Optional
from dataclasses import dataclass, field
from datetime import datetime, timedelta
import numpy as np


@dataclass
class BirdSighting:
    """A single bird sighting record."""

    lat: float
    lng: float
    species: str
    count: int
    timestamp: datetime
    is_pest: bool = True

    @property
    def weight(self) -> float:
        """Weight based on count and recency."""
        hours_ago = (datetime.now() - self.timestamp).total_seconds() / 3600
        recency = max(0.1, 1.0 - hours_ago / 168.0)  # Decay over 7 days
        return self.count * recency


@dataclass
class DensityCell:
    """Grid cell with density value."""

    lat: float
    lng: float
    density: float  # 0-1 normalized
    confidence: float  # 0-1 based on sample count
    species_breakdown: Dict[str, float] = field(default_factory=dict)


class BirdDensityMapper:
    """Generates bird density heatmaps for informed path planning."""

    def __init__(
        self,
        grid_resolution_m: float = 20.0,
        kernel_bandwidth_m: float = 100.0,
        decay_hours: float = 168.0,  # 7 days
        max_density_cap: float = 100.0,
    ) -> None:
        """Initialize density mapper.

        Args:
            grid_resolution_m: Grid cell size in meters.
            kernel_bandwidth_m: KDE kernel bandwidth (meters).
            decay_hours: Time decay half-life for historical data (hours).
            max_density_cap: Maximum sightings before capping density.
        """
        self.grid_resolution_m = grid_resolution_m
        self.kernel_bandwidth = kernel_bandwidth_m
        self.decay_hours = decay_hours
        self.max_density_cap = max_density_cap
        self.sightings: List[BirdSighting] = []

    def add_sighting(
        self,
        lat: float,
        lng: float,
        species: str,
        count: int,
        timestamp: Optional[datetime] = None,
        is_pest: bool = True,
    ) -> None:
        """Record a bird sighting.

        Args:
            lat, lng: GPS coordinates.
            species: Bird species name.
            count: Number of birds observed.
            timestamp: Observation time (defaults to now).
            is_pest: Whether this species is a pest.
        """
        self.sightings.append(BirdSighting(
            lat=lat,
            lng=lng,
            species=species,
            count=count,
            timestamp=timestamp or datetime.now(),
            is_pest=is_pest,
        ))

    def generate_heatmap(
        self,
        bounds: Tuple[float, float, float, float],
        time_window_hours: Optional[float] = None,
    ) -> List[DensityCell]:
        """Generate density heatmap for a geographical region.

        Args:
            bounds: (min_lat, min_lng, max_lat, max_lng).
            time_window_hours: Optional time window filter (None = all data).

        Returns:
            List of DensityCell sorted by density descending.
        """
        min_lat, min_lng, max_lat, max_lng = bounds

        # Filter sightings by time window
        sightings = self.sightings
        if time_window_hours is not None:
            cutoff = datetime.now() - timedelta(hours=time_window_hours)
            sightings = [s for s in sightings if s.timestamp >= cutoff]

        if not sightings:
            return []

        # Create grid
        lat_step = self.grid_resolution_m / 111320.0
        lng_step = self.grid_resolution_m / (111320.0 * math.cos(math.radians(min_lat)))

        grid_cells: List[DensityCell] = []
        lat = min_lat
        while lat <= max_lat:
            lng = min_lng
            while lng <= max_lng:
                cell = self._compute_cell_density(lat, lng, sightings)
                if cell.density > 0.001:
                    grid_cells.append(cell)
                lng += lng_step
            lat += lat_step

        # Normalize densities
        if grid_cells:
            max_density = max(c.density for c in grid_cells)
            for cell in grid_cells:
                cell.density = min(cell.density / max_density, 1.0)

        return sorted(grid_cells, key=lambda c: c.density, reverse=True)

    def _compute_cell_density(
        self,
        cell_lat: float,
        cell_lng: float,
        sightings: List[BirdSighting],
    ) -> DensityCell:
        """Compute density for a single grid cell using KDE.

        Args:
            cell_lat, cell_lng: Cell center coordinates.
            sightings: List of bird sightings.

        Returns:
            DensityCell with computed values.
        """
        species_count: Dict[str, float] = {}
        total_density = 0.0
        total_weight = 0.0

        for sighting in sightings:
            dist_m = self._haversine_distance(
                cell_lat, cell_lng, sighting.lat, sighting.lng
            )

            # Gaussian kernel
            kernel_val = math.exp(
                -0.5 * (dist_m / self.kernel_bandwidth) ** 2
            )

            weight = sighting.weight * kernel_val
            total_density += weight
            total_weight += kernel_val

            species_count[sighting.species] = (
                species_count.get(sighting.species, 0) + weight
            )

        # Normalize species breakdown
        if total_density > 0:
            for species in species_count:
                species_count[species] /= total_density

        return DensityCell(
            lat=cell_lat,
            lng=cell_lng,
            density=total_density,
            confidence=min(total_weight / 10.0, 1.0),  # At least 10 kernel contributions
            species_breakdown=species_count,
        )

    def get_hotspots(
        self,
        bounds: Tuple[float, float, float, float],
        threshold: float = 0.5,
        top_k: int = 10,
    ) -> List[DensityCell]:
        """Get top bird activity hotspots.

        Args:
            bounds: Geographic bounds.
            threshold: Minimum density threshold.
            top_k: Maximum number of hotspots to return.

        Returns:
            Top hotspots above threshold.
        """
        heatmap = self.generate_heatmap(bounds)
        hotspots = [c for c in heatmap if c.density >= threshold]
        return hotspots[:top_k]

    def get_time_profile(
        self,
        lat: float,
        lng: float,
        radius_m: float = 100.0,
    ) -> List[Tuple[int, float]]:
        """Get bird activity profile by hour of day for a location.

        Args:
            lat, lng: Location center.
            radius_m: Search radius in meters.

        Returns:
            List of (hour, avg_density) for hours 0-23.
        """
        nearby = [
            s for s in self.sightings
            if self._haversine_distance(lat, lng, s.lat, s.lng) <= radius_m
        ]

        hour_counts = [0.0] * 24
        hour_weights = [0.0] * 24

        for sighting in nearby:
            hour = sighting.timestamp.hour
            hour_counts[hour] += sighting.count
            hour_weights[hour] += sighting.weight

        total = max(hour_counts) if max(hour_counts) > 0 else 1
        return [(h, hour_counts[h] / total) for h in range(24)]

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


def main() -> None:
    """Demo density mapping."""
    mapper = BirdDensityMapper(grid_resolution_m=30, kernel_bandwidth_m=150)

    # Simulate sightings
    import random
    center_lat, center_lng = 29.863, 121.541

    for _ in range(100):
        mapper.add_sighting(
            lat=center_lat + random.uniform(-0.002, 0.002),
            lng=center_lng + random.uniform(-0.002, 0.002),
            species=random.choice(["sparrow", "pigeon", "crow"]),
            count=random.randint(1, 20),
            timestamp=datetime.now() - timedelta(hours=random.randint(0, 72)),
        )

    bounds = (center_lat - 0.003, center_lng - 0.003, center_lat + 0.003, center_lng + 0.003)
    hotspots = mapper.get_hotspots(bounds, threshold=0.3, top_k=5)

    print(f"Found {len(hotspots)} hotspots:")
    for h in hotspots:
        print(f"  {h.lat:.6f}, {h.lng:.6f} density={h.density:.3f}")


if __name__ == "__main__":
    main()
