"""AI-powered cruise path planning for fixed-wing agricultural drones.

Implements A* pathfinding, RRT (Rapidly-exploring Random Tree), and
hybrid path planning for automated bird-deterrence cruise routes.
"""

import math
import random
import heapq
from typing import List, Tuple, Optional, Set, Dict
from dataclasses import dataclass, field
import numpy as np


@dataclass
class Waypoint:
    """GPS waypoint with metadata."""

    lat: float
    lng: float
    altitude: float = 50.0  # meters
    speed: float = 15.0  # m/s
    action: str = "cruise"  # cruise, deter, return, hover
    id: str = ""

    def __post_init__(self):
        if not self.id:
            self.id = f"WP_{self.lat:.4f}_{self.lng:.4f}"

    def distance_to(self, other: "Waypoint") -> float:
        """Haversine distance in meters."""
        return haversine_distance(
            self.lat, self.lng, other.lat, other.lng
        )

    def to_tuple(self) -> Tuple[float, float]:
        return (self.lat, self.lng)


def haversine_distance(
    lat1: float, lng1: float, lat2: float, lng2: float
) -> float:
    """Calculate haversine distance between two GPS coordinates in meters.

    Args:
        lat1, lng1: First coordinate.
        lat2, lng2: Second coordinate.

    Returns:
        Distance in meters.
    """
    R = 6371000  # Earth radius in meters
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lng2 - lng1)

    a = math.sin(dphi / 2) ** 2 + \
        math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

    return R * c


@dataclass
class GridCell:
    """Grid cell for A* pathfinding."""

    lat: float
    lng: float
    g_cost: float = float("inf")  # Cost from start
    h_cost: float = 0.0  # Heuristic to goal
    parent: Optional["GridCell"] = None
    is_obstacle: bool = False
    bird_density: float = 0.0  # 0-1 bird density for risk-aware planning

    @property
    def f_cost(self) -> float:
        return self.g_cost + self.h_cost

    def __lt__(self, other: "GridCell") -> bool:
        return self.f_cost < other.f_cost


class AStarPlanner:
    """A* path planner for drone waypoint navigation.

    Finds optimal path through a grid of GPS coordinates,
    avoiding obstacles and high bird-density zones.
    """

    def __init__(
        self,
        grid_resolution: float = 0.0001,  # ~11m at equator
        safety_margin: float = 30.0,  # meters from obstacles
        bird_risk_weight: float = 0.3,
    ) -> None:
        """Initialize A* planner.

        Args:
            grid_resolution: Grid cell size in degrees.
            safety_margin: Minimum distance from obstacles (meters).
            bird_risk_weight: Weight for bird density in cost function.
        """
        self.grid_resolution = grid_resolution
        self.safety_margin = safety_margin
        self.bird_risk_weight = bird_risk_weight
        self.obstacles: List[Tuple[float, float, float]] = []  # (lat, lng, radius_m)
        self.bird_density_grid: Dict[Tuple[int, int], float] = {}

    def add_obstacle(self, lat: float, lng: float, radius_m: float) -> None:
        """Add a no-fly zone obstacle."""
        self.obstacles.append((lat, lng, radius_m))

    def set_bird_density(self, lat: float, lng: float, density: float) -> None:
        """Set bird density at a grid cell (0-1)."""
        grid_lat = int(lat / self.grid_resolution)
        grid_lng = int(lng / self.grid_resolution)
        self.bird_density_grid[(grid_lat, grid_lng)] = density

    def _is_obstacle(self, lat: float, lng: float) -> bool:
        """Check if a point is within any obstacle."""
        for o_lat, o_lng, radius in self.obstacles:
            if haversine_distance(lat, lng, o_lat, o_lng) < radius + self.safety_margin:
                return True
        return False

    def _get_bird_density(self, lat: float, lng: float) -> float:
        """Get bird density at a grid location."""
        grid_lat = int(lat / self.grid_resolution)
        grid_lng = int(lng / self.grid_resolution)
        return self.bird_density_grid.get((grid_lat, grid_lng), 0.0)

    def _get_neighbors(self, cell: GridCell) -> List[Tuple[float, float]]:
        """Get 8-connected neighbors of a grid cell."""
        neighbors = []
        for dlat in [-1, 0, 1]:
            for dlng in [-1, 0, 1]:
                if dlat == 0 and dlng == 0:
                    continue
                n_lat = cell.lat + dlat * self.grid_resolution
                n_lng = cell.lng + dlng * self.grid_resolution
                neighbors.append((n_lat, n_lng))
        return neighbors

    def find_path(
        self,
        start: Waypoint,
        goal: Waypoint,
        max_iterations: int = 10000,
    ) -> List[Waypoint]:
        """Find optimal path using A* algorithm.

        Args:
            start: Starting waypoint.
            goal: Destination waypoint.
            max_iterations: Maximum search iterations.

        Returns:
            Ordered list of waypoints from start to goal.
        """
        # Snap to grid
        start_lat = round(start.lat / self.grid_resolution) * self.grid_resolution
        start_lng = round(start.lng / self.grid_resolution) * self.grid_resolution
        goal_lat = round(goal.lat / self.grid_resolution) * self.grid_resolution
        goal_lng = round(goal.lng / self.grid_resolution) * self.grid_resolution

        # Initialize
        start_cell = GridCell(lat=start_lat, lng=start_lng, g_cost=0)
        start_cell.h_cost = haversine_distance(start_lat, start_lng, goal_lat, goal_lng)

        open_set: List[GridCell] = [start_cell]
        closed_set: Set[Tuple[float, float]] = set()
        cell_map: Dict[Tuple[float, float], GridCell] = {
            (start_lat, start_lng): start_cell
        }

        iterations = 0
        while open_set and iterations < max_iterations:
            iterations += 1
            current = heapq.heappop(open_set)
            current_key = (current.lat, current.lng)

            # Goal check
            if haversine_distance(
                current.lat, current.lng, goal_lat, goal_lng
            ) < self.safety_margin:
                return self._reconstruct_path(current)

            closed_set.add(current_key)

            for n_lat, n_lng in self._get_neighbors(current):
                n_key = (n_lat, n_lng)

                if n_key in closed_set or self._is_obstacle(n_lat, n_lng):
                    continue

                # Movement cost (diagonal costs more)
                move_cost = haversine_distance(current.lat, current.lng, n_lat, n_lng)

                # Bird density penalty
                bird_penalty = self._get_bird_density(n_lat, n_lng) * self.bird_risk_weight * move_cost

                # Risk penalty: prefer low bird density
                move_cost += bird_penalty

                tentative_g = current.g_cost + move_cost

                neighbor = cell_map.get(n_key)
                if neighbor is None:
                    neighbor = GridCell(lat=n_lat, lng=n_lng)
                    cell_map[n_key] = neighbor

                if tentative_g < neighbor.g_cost:
                    neighbor.g_cost = tentative_g
                    neighbor.h_cost = haversine_distance(n_lat, n_lng, goal_lat, goal_lng)
                    neighbor.parent = current

                    # Update or add to open set
                    if neighbor not in open_set:
                        heapq.heappush(open_set, neighbor)

        return []  # No path found

    def _reconstruct_path(self, cell: GridCell) -> List[Waypoint]:
        """Reconstruct path from goal to start following parent pointers."""
        path = []
        current: Optional[GridCell] = cell
        while current is not None:
            path.append(Waypoint(
                lat=current.lat,
                lng=current.lng,
                altitude=50.0,
                action="cruise",
            ))
            current = current.parent
        path.reverse()
        return path


class RRTPlanner:
    """RRT (Rapidly-exploring Random Tree) path planner.

    Suitable for complex environments with many obstacles where
    A* grid search becomes expensive.
    """

    def __init__(
        self,
        max_iterations: int = 5000,
        step_size: float = 20.0,  # meters
        goal_sample_rate: float = 0.1,
    ) -> None:
        """Initialize RRT planner.

        Args:
            max_iterations: Maximum tree expansion iterations.
            step_size: Step size for tree expansion (meters).
            goal_sample_rate: Probability of sampling goal point.
        """
        self.max_iterations = max_iterations
        self.step_size = step_size
        self.goal_sample_rate = goal_sample_rate
        self.nodes: List["RRTNode"] = []
        self.obstacles: List[Tuple[float, float, float]] = []

    def add_obstacle(self, lat: float, lng: float, radius_m: float) -> None:
        self.obstacles.append((lat, lng, radius_m))

    def _is_collision_free(self, lat: float, lng: float) -> bool:
        for o_lat, o_lng, radius in self.obstacles:
            if haversine_distance(lat, lng, o_lat, o_lng) < radius:
                return False
        return True

    def find_path(self, start: Waypoint, goal: Waypoint) -> List[Waypoint]:
        """Find path using RRT algorithm."""
        self.nodes = [RRTNode(start.lat, start.lng)]

        for _ in range(self.max_iterations):
            # Sample random point (with goal bias)
            if random.random() < self.goal_sample_rate:
                sample_lat, sample_lng = goal.lat, goal.lng
            else:
                sample_lat, sample_lng = self._random_sample(start, goal)

            # Find nearest node
            nearest = self._nearest(sample_lat, sample_lng)

            # Extend toward sample
            new_lat, new_lng = self._steer(
                nearest.lat, nearest.lng, sample_lat, sample_lng
            )

            if self._is_collision_free(new_lat, new_lng):
                new_node = RRTNode(new_lat, new_lng, parent=nearest)
                self.nodes.append(new_node)

                # Check goal reached
                if haversine_distance(new_lat, new_lng, goal.lat, goal.lng) < self.step_size:
                    return self._extract_path(new_node, start)

        return []

    def _random_sample(
        self, start: Waypoint, goal: Waypoint
    ) -> Tuple[float, float]:
        """Generate random sample within bounding box of start-goal."""
        min_lat = min(start.lat, goal.lat) - 0.005
        max_lat = max(start.lat, goal.lat) + 0.005
        min_lng = min(start.lng, goal.lng) - 0.005
        max_lng = max(start.lng, goal.lng) + 0.005
        return (
            random.uniform(min_lat, max_lat),
            random.uniform(min_lng, max_lng),
        )

    def _nearest(self, lat: float, lng: float) -> "RRTNode":
        """Find nearest node in tree."""
        return min(
            self.nodes,
            key=lambda n: haversine_distance(n.lat, n.lng, lat, lng),
        )

    def _steer(
        self, from_lat: float, from_lng: float, to_lat: float, to_lng: float
    ) -> Tuple[float, float]:
        """Steer from nearest toward sample by step_size."""
        dist = haversine_distance(from_lat, from_lng, to_lat, to_lng)
        if dist < self.step_size:
            return to_lat, to_lng

        ratio = self.step_size / dist
        new_lat = from_lat + (to_lat - from_lat) * ratio
        new_lng = from_lng + (to_lng - from_lng) * ratio
        return new_lat, new_lng

    def _extract_path(self, node: "RRTNode", start: Waypoint) -> List[Waypoint]:
        """Extract path from tree."""
        path = []
        current: Optional[RRTNode] = node
        while current is not None:
            path.append(Waypoint(
                lat=current.lat,
                lng=current.lng,
                altitude=50.0,
                action="cruise",
            ))
            current = current.parent
        path.reverse()
        return path


@dataclass
class RRTNode:
    """Node in RRT tree."""

    lat: float
    lng: float
    parent: Optional["RRTNode"] = None


def main() -> None:
    """Demo path planning."""
    planner = AStarPlanner(
        grid_resolution=0.0001,
        safety_margin=30.0,
    )

    # Add some obstacles (buildings, trees)
    planner.add_obstacle(29.8630, 121.5410, 50)  # 50m radius

    start = Waypoint(29.8620, 121.5400)
    goal = Waypoint(29.8650, 121.5430)

    path = planner.find_path(start, goal)
    print(f"Found path with {len(path)} waypoints:")
    for wp in path:
        print(f"  {wp.lat:.6f}, {wp.lng:.6f}")


if __name__ == "__main__":
    main()
