from __future__ import annotations

from bisect import bisect_right
from functools import lru_cache

from .types import CurrentProfile


@lru_cache(maxsize=128)
def _profile_timestamps(points: tuple[object, ...]) -> tuple[int, ...]:
    return tuple(point.time_s for point in points)


def current_for_time(profile: CurrentProfile | None, fallback_current_a: float, time_s: int) -> float:
    """Return the current active at time_s using piecewise-constant profile segments."""

    if profile is None or not profile.points:
        return fallback_current_a

    timestamps = _profile_timestamps(profile.points)
    index = bisect_right(timestamps, time_s) - 1
    if index < 0:
        return profile.points[0].current_a
    return profile.points[index].current_a
