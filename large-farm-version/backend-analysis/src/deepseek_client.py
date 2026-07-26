"""DeepSeek API client for bird behavior pattern analysis.

Uses DeepSeek's chat API to analyze bird activity data and generate
actionable insights for optimizing deterrence strategies.
"""

import os
import json
import logging
from typing import List, Dict, Any, Optional
import httpx

logger = logging.getLogger(__name__)


class DeepSeekClient:
    """Client for DeepSeek API - bird activity analysis."""

    def __init__(
        self,
        api_key: Optional[str] = None,
        api_base: str = "https://api.deepseek.com/v1",
        model: str = "deepseek-chat",
        max_tokens: int = 2048,
        temperature: float = 0.3,
    ) -> None:
        """Initialize DeepSeek client.

        Args:
            api_key: API key (defaults to DEEPSEEK_API_KEY env var).
            api_base: API base URL.
            model: Model name.
            max_tokens: Max response tokens.
            temperature: Response randomness (0-2).
        """
        self.api_key = api_key or os.environ.get("DEEPSEEK_API_KEY", "")
        self.api_base = api_base.rstrip("/")
        self.model = model
        self.max_tokens = max_tokens
        self.temperature = temperature
        self._client = httpx.Client(timeout=30.0)

    def analyze_bird_patterns(
        self, statistics: Dict[str, Any]
    ) -> Dict[str, Any]:
        """Analyze bird activity patterns and generate recommendations.

        Args:
            statistics: Aggregated detection statistics including:
                - species_distribution: Per-species counts
                - hourly_activity: Activity levels by hour
                - spatial_hotspots: Hotspot coordinates
                - deter_success_rate: Overall success rate

        Returns:
            Analysis results with recommendations.
        """
        prompt = self._build_analysis_prompt(statistics)

        try:
            response = self._send_message(prompt)
            return self._parse_analysis(response)
        except Exception as e:
            logger.error(f"DeepSeek analysis failed: {e}")
            return {"error": str(e), "recommendations": []}

    def optimize_cruise_route(
        self,
        current_route: List[Dict[str, float]],
        density_map: List[Dict[str, Any]],
        constraints: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Use DeepSeek to suggest cruise route optimizations.

        Args:
            current_route: Current waypoint sequence.
            density_map: Bird density heatmap data.
            constraints: Flight constraints (battery, time, etc.).

        Returns:
            Route optimization suggestions.
        """
        prompt = f"""You are a drone cruise route optimizer for agricultural bird deterrence.

Current route has {len(current_route)} waypoints. Bird density hotspots (top 5):
{json.dumps(density_map[:5], indent=2)}

Flight constraints:
- Max flight time: {constraints.get('max_flight_min', 45)} minutes
- Battery capacity: {constraints.get('battery_mah', 6000)} mAh
- Swath width: {constraints.get('swath_m', 30)} meters

Suggest 3 specific route adjustments to maximize bird deterrence coverage.
For each suggestion, provide:
1. Which waypoints to add/remove/reorder
2. Rationale based on density data
3. Estimated improvement in coverage percentage

Respond in JSON format:
{{"suggestions": [{{"action": "add/remove/reorder", "waypoint_index": int, "new_coords": [lat, lng], "rationale": str, "improvement_pct": float}}]}}
"""
        try:
            response = self._send_message(prompt)
            return self._parse_json_response(response, "route_optimization")
        except Exception as e:
            logger.error(f"Route optimization failed: {e}")
            return {"error": str(e), "suggestions": []}

    def predict_bird_activity(
        self,
        historical_data: List[Dict[str, Any]],
        forecast_hours: int = 24,
    ) -> Dict[str, Any]:
        """Predict bird activity for upcoming period.

        Args:
            historical_data: Last 7 days of detection records.
            forecast_hours: Hours to forecast ahead.

        Returns:
            Activity forecast with confidence levels.
        """
        summary = self._summarize_historical(historical_data)

        prompt = f"""You are an agricultural bird behavior analyst.

Based on the following {len(historical_data)} historical detection records over 7 days,
predict bird activity patterns for the next {forecast_hours} hours.

Activity summary:
- Peak hours: {summary.get('peak_hours', [])}
- Top species: {summary.get('top_species', [])}
- Trend: {summary.get('trend', 'stable')}
- Weather correlation: {summary.get('weather_correlation', 'unknown')}

Predict:
1. Expected activity level (low/medium/high) for each 3-hour block
2. Likely species composition changes
3. Recommended patrol density (light/standard/intensive)
4. Confidence level (0-1) for each prediction

Respond in JSON. Include a "time_blocks" array with 3-hour intervals.
"""
        try:
            response = self._send_message(prompt)
            return self._parse_json_response(response, "activity_prediction")
        except Exception as e:
            logger.error(f"Activity prediction failed: {e}")
            return {"error": str(e)}

    # -------------------- Internal --------------------

    def _build_analysis_prompt(self, stats: Dict[str, Any]) -> str:
        """Build analysis prompt from statistics."""
        species = stats.get("species_distribution", {})
        hourly = stats.get("hourly_activity", [])
        hotspots = stats.get("spatial_hotspots", [])
        success_rate = stats.get("deter_success_rate", 0)

        return f"""You are an expert in agricultural bird behavior analysis and deterrence strategy optimization.

Analyze the following bird activity data from a farm monitoring system:

Species distribution (past 7 days):
{json.dumps(species, indent=2)}

Hourly activity profile:
{json.dumps(hourly[:24], indent=2)}

Top 5 spatial hotspots (lat, lng, density):
{json.dumps(hotspots[:5], indent=2)}

Overall deterrence success rate: {success_rate:.1%}

Provide:
1. Key behavioral patterns identified
2. 3-5 specific, actionable recommendations to improve deterrence effectiveness
3. Suggested optimal patrol schedule (time blocks)
4. Risk assessment for crop damage if no action taken

Respond in JSON format with keys: patterns, recommendations, patrol_schedule, risk_assessment.
Each recommendation should have: action, expected_impact, priority (high/medium/low)."""

    def _send_message(self, prompt: str) -> str:
        """Send message to DeepSeek API."""
        if not self.api_key:
            raise ValueError("DEEPSEEK_API_KEY not set")

        response = self._client.post(
            f"{self.api_base}/chat/completions",
            headers={
                "Authorization": f"Bearer {self.api_key}",
                "Content-Type": "application/json",
            },
            json={
                "model": self.model,
                "messages": [
                    {
                        "role": "system",
                        "content": "You are an agricultural AI assistant specializing in bird behavior analysis. Always respond with valid JSON."
                    },
                    {"role": "user", "content": prompt},
                ],
                "max_tokens": self.max_tokens,
                "temperature": self.temperature,
            },
        )
        response.raise_for_status()
        data = response.json()
        return data["choices"][0]["message"]["content"]

    def _parse_analysis(self, response: str) -> Dict[str, Any]:
        """Parse analysis response from DeepSeek."""
        return self._parse_json_response(response, "analysis")

    def _parse_json_response(
        self, response: str, context: str = ""
    ) -> Dict[str, Any]:
        """Extract JSON from model response."""
        # Try direct parse
        try:
            return json.loads(response)
        except json.JSONDecodeError:
            pass

        # Try extract JSON block from markdown
        import re
        json_match = re.search(r"```(?:json)?\s*([\s\S]*?)```", response)
        if json_match:
            try:
                return json.loads(json_match.group(1))
            except json.JSONDecodeError:
                pass

        logger.warning(f"Failed to parse JSON from {context} response")
        return {"raw_response": response, "error": "JSON parse failed"}

    def _summarize_historical(
        self, data: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Summarize historical detection data for prediction prompt."""
        from collections import Counter

        species_counts = Counter()
        hour_counts = Counter()
        trend = "stable"

        for record in data:
            species_counts[record.get("class_name", "unknown")] += record.get("count", 1)

            ts = record.get("timestamp", "")
            try:
                dt = __import__("datetime").datetime.fromisoformat(ts)
                hour_counts[dt.hour] += record.get("count", 1)
            except (ValueError, TypeError):
                pass

        # Simple trend detection
        recent = sum(1 for r in data[-100:] if r.get("count", 0) > 5)
        older = sum(1 for r in data[:100] if r.get("count", 0) > 5)
        if recent > older * 1.3:
            trend = "increasing"
        elif recent < older * 0.7:
            trend = "decreasing"

        peak_hours = [h for h, _ in hour_counts.most_common(4)]

        return {
            "top_species": species_counts.most_common(5),
            "peak_hours": sorted(peak_hours),
            "trend": trend,
            "weather_correlation": "unknown",
            "total_records": len(data),
        }
