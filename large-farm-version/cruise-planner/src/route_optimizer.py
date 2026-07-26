"""Route optimizer using genetic algorithm and simulated annealing.

Optimizes cruise waypoint sequences based on bird density heatmaps,
flight time constraints, and coverage requirements.
"""

import random
import math
from typing import List, Tuple, Optional, Callable
from dataclasses import dataclass
import numpy as np
from .path_planner import Waypoint, haversine_distance
from .bird_density_map import BirdDensityMapper, DensityCell


@dataclass
class RouteCandidate:
    """A candidate route for optimization."""

    waypoints: List[Waypoint]
    fitness: float = 0.0

    @property
    def total_distance(self) -> float:
        """Total route distance in meters."""
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
    def flight_time_minutes(self) -> float:
        """Estimated flight time in minutes."""
        avg_speed = np.mean([wp.speed for wp in self.waypoints]) if self.waypoints else 15.0
        return self.total_distance / (avg_speed * 60)


@dataclass
class OptimizerConfig:
    """Configuration for route optimizer."""

    population_size: int = 100
    generations: int = 200
    mutation_rate: float = 0.15
    crossover_rate: float = 0.7
    elite_count: int = 5
    max_flight_time_min: float = 45.0  # Battery constraint
    bird_density_weight: float = 0.6   # Weight for bird density in fitness
    distance_weight: float = 0.3       # Weight for distance in fitness
    coverage_weight: float = 0.1       # Weight for coverage completeness


class RouteOptimizer:
    """Route optimizer using genetic algorithm."""

    def __init__(
        self,
        config: Optional[OptimizerConfig] = None,
        density_mapper: Optional[BirdDensityMapper] = None,
    ) -> None:
        """Initialize optimizer.

        Args:
            config: Optimizer configuration.
            density_mapper: Bird density mapper for fitness evaluation.
        """
        self.config = config or OptimizerConfig()
        self.density_mapper = density_mapper or BirdDensityMapper()

    def optimize(
        self,
        initial_waypoints: List[Waypoint],
        progress_callback: Optional[Callable[[int, float], None]] = None,
    ) -> RouteCandidate:
        """Optimize route using genetic algorithm.

        Args:
            initial_waypoints: Starting waypoint sequence.
            progress_callback: Optional callback(generation, best_fitness).

        Returns:
            Best route candidate found.
        """
        if len(initial_waypoints) < 3:
            return RouteCandidate(waypoints=initial_waypoints)

        # Initialize population
        population = self._init_population(initial_waypoints)

        best: Optional[RouteCandidate] = None

        for gen in range(self.config.generations):
            # Evaluate fitness
            for candidate in population:
                candidate.fitness = self._evaluate_fitness(candidate)

            # Sort by fitness (higher is better)
            population.sort(key=lambda c: c.fitness, reverse=True)

            if best is None or population[0].fitness > best.fitness:
                best = population[0]

            if progress_callback:
                progress_callback(gen, best.fitness)

            # Elitism: keep best individuals
            new_population = population[:self.config.elite_count]

            # Generate rest through crossover and mutation
            while len(new_population) < self.config.population_size:
                parent1 = self._tournament_select(population)
                parent2 = self._tournament_select(population)

                if random.random() < self.config.crossover_rate:
                    child_wps = self._ordered_crossover(parent1.waypoints, parent2.waypoints)
                else:
                    child_wps = parent1.waypoints.copy()

                if random.random() < self.config.mutation_rate:
                    child_wps = self._mutate(child_wps)

                new_population.append(RouteCandidate(waypoints=child_wps))

            population = new_population

        return best or RouteCandidate(waypoints=initial_waypoints)

    def _init_population(self, waypoints: List[Waypoint]) -> List[RouteCandidate]:
        """Initialize population with variations of the initial route."""
        population = [RouteCandidate(waypoints=waypoints.copy())]

        for _ in range(self.config.population_size - 1):
            # Shuffle middle waypoints (keep start/end fixed)
            shuffled = waypoints.copy()
            middle = shuffled[1:-1]
            random.shuffle(middle)
            population.append(RouteCandidate(waypoints=shuffled))

        return population

    def _evaluate_fitness(self, candidate: RouteCandidate) -> float:
        """Evaluate route fitness (higher = better).

        Components:
        - Bird density: reward routes that pass through high-density areas
        - Distance: penalize excessive flight distance
        - Coverage: reward diverse spatial coverage
        """
        if len(candidate.waypoints) < 2:
            return 0.0

        # 1. Bird density score
        density_score = 0.0
        for wp in candidate.waypoints:
            # Query density at waypoint location
            bounds = (
                wp.lat - 0.001, wp.lng - 0.001,
                wp.lat + 0.001, wp.lng + 0.001,
            )
            hotspots = self.density_mapper.get_hotspots(bounds, threshold=0.1, top_k=1)
            if hotspots:
                density_score += hotspots[0].density

        density_score = density_score / max(len(candidate.waypoints), 1)

        # 2. Distance penalty (normalized, max ~5000m for 25 mu)
        max_dist = 5000.0
        distance_score = 1.0 - min(candidate.total_distance / max_dist, 1.0)

        # 3. Coverage diversity (std dev of waypoint positions)
        lats = [wp.lat for wp in candidate.waypoints]
        lngs = [wp.lng for wp in candidate.waypoints]
        coverage_score = min(np.std(lats) * np.std(lngs) * 1e8, 1.0)

        # 4. Flight time constraint penalty
        if candidate.flight_time_minutes > self.config.max_flight_time_min:
            time_penalty = (candidate.flight_time_minutes - self.config.max_flight_time_min) / 60.0
            time_penalty = min(time_penalty, 0.5)
        else:
            time_penalty = 0.0

        fitness = (
            self.config.bird_density_weight * density_score
            + self.config.distance_weight * distance_score
            + self.config.coverage_weight * coverage_score
            - time_penalty
        )

        return max(fitness, 0.0)

    def _tournament_select(
        self, population: List[RouteCandidate], tournament_size: int = 3
    ) -> RouteCandidate:
        """Tournament selection."""
        tournament = random.sample(population, min(tournament_size, len(population)))
        return max(tournament, key=lambda c: c.fitness)

    def _ordered_crossover(
        self, parent1: List[Waypoint], parent2: List[Waypoint]
    ) -> List[Waypoint]:
        """Ordered crossover (OX) for waypoint sequences."""
        if len(parent1) < 4:
            return parent1.copy()

        # Keep start/end fixed
        start, end = parent1[0], parent1[-1]
        middle1 = parent1[1:-1]
        middle2 = parent2[1:-1]

        # Select crossover segment
        size = len(middle1)
        cx1, cx2 = sorted(random.sample(range(size), 2))

        child_middle = [None] * size
        child_middle[cx1:cx2] = middle1[cx1:cx2]

        # Fill remaining from parent2
        p2_idx = 0
        for i in range(size):
            if child_middle[i] is None:
                while p2_idx < size and middle2[p2_idx] in child_middle:
                    p2_idx += 1
                if p2_idx < size:
                    child_middle[i] = middle2[p2_idx]
                    p2_idx += 1

        # Fill any remaining None
        for i in range(size):
            if child_middle[i] is None:
                for wp in middle1:
                    if wp not in child_middle:
                        child_middle[i] = wp
                        break

        return [start] + child_middle + [end]

    def _mutate(self, waypoints: List[Waypoint]) -> List[Waypoint]:
        """Apply random mutation to waypoint sequence."""
        mutated = waypoints.copy()

        if len(mutated) < 4:
            return mutated

        mutation_type = random.choice(["swap", "reverse", "shift"])

        if mutation_type == "swap":
            # Swap two random middle waypoints
            i = random.randint(1, len(mutated) - 2)
            j = random.randint(1, len(mutated) - 2)
            mutated[i], mutated[j] = mutated[j], mutated[i]

        elif mutation_type == "reverse":
            # Reverse a subsequence
            i = random.randint(1, len(mutated) - 3)
            j = random.randint(i + 1, len(mutated) - 2)
            mutated[i:j+1] = reversed(mutated[i:j+1])

        elif mutation_type == "shift":
            # Shift a waypoint to new position
            i = random.randint(1, len(mutated) - 2)
            wp = mutated.pop(i)
            j = random.randint(1, len(mutated) - 1)
            mutated.insert(j, wp)

        return mutated


def main() -> None:
    """Demo route optimization."""
    # Create sample waypoints
    waypoints = [
        Waypoint(29.862, 121.540, action="start"),
        Waypoint(29.863, 121.541, action="cruise"),
        Waypoint(29.864, 121.540, action="cruise"),
        Waypoint(29.863, 121.542, action="cruise"),
        Waypoint(29.865, 121.541, action="cruise"),
        Waypoint(29.864, 121.543, action="cruise"),
        Waypoint(29.862, 121.543, action="return"),
    ]

    optimizer = RouteOptimizer()
    result = optimizer.optimize(waypoints)

    print(f"Optimized route: {len(result.waypoints)} waypoints")
    print(f"Fitness: {result.fitness:.4f}")
    print(f"Distance: {result.total_distance:.1f}m")
    print(f"Flight time: {result.flight_time_minutes:.1f}min")


if __name__ == "__main__":
    main()
