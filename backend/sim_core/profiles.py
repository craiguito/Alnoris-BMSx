from __future__ import annotations

from .types import CurrentProfile


def current_for_time(profile: CurrentProfile | None, fallback_current_a: float, time_s: int) -> float:
    """Return the current active at time_s using piecewise-constant profile segments."""

    if profile is None or not profile.points:
        return fallback_current_a

    active_current = profile.points[0].current_a
    for point in profile.points:
        if point.time_s > time_s:
            break
        active_current = point.current_a
    return active_current
