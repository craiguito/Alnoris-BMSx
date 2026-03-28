from __future__ import annotations

from .physics.electrical import validate_electrical_model
from .types import CurrentProfile, DegradationConfig, SimulationConfig


def validate_current_profile(profile: CurrentProfile | None) -> CurrentProfile | None:
    if profile is None:
        return None
    if not profile.points:
        raise ValueError("Current profile must contain at least one point.")

    seen_times: set[int] = set()
    previous_time: int | None = None
    for point in profile.points:
        if point.time_s < 0:
            raise ValueError("Current profile timestamps must be >= 0.")
        if point.time_s in seen_times:
            raise ValueError("Current profile timestamps must be unique.")
        if previous_time is not None and point.time_s <= previous_time:
            raise ValueError("Current profile timestamps must be strictly increasing.")
        seen_times.add(point.time_s)
        previous_time = point.time_s
    return profile


def validate_degradation(config: DegradationConfig) -> DegradationConfig:
    if config.capacity_fade_per_throughput_ah < 0.0:
        raise ValueError("capacity_fade_per_throughput_ah must be >= 0.")
    if config.resistance_growth_per_throughput_ah < 0.0:
        raise ValueError("resistance_growth_per_throughput_ah must be >= 0.")
    if config.temperature_acceleration_per_c < 0.0:
        raise ValueError("temperature_acceleration_per_c must be >= 0.")
    if config.depth_of_discharge_weight < 0.0:
        raise ValueError("depth_of_discharge_weight must be >= 0.")
    return config


def validate_simulation_config(config: SimulationConfig) -> SimulationConfig:
    if config.duration_s <= 0:
        raise ValueError("duration_s must be > 0.")
    if config.time_step_s <= 0:
        raise ValueError("time_step_s must be > 0.")
    if config.group_count is not None and config.group_count < 1:
        raise ValueError("group_count must be >= 1 when provided.")
    if config.cells_in_series < 1:
        raise ValueError("cells_in_series must be >= 1.")
    if config.cells_in_parallel < 1:
        raise ValueError("cells_in_parallel must be >= 1.")
    if config.cell_capacity_ah <= 0.0:
        raise ValueError("cell_capacity_ah must be > 0.")
    if config.internal_resistance_ohm_per_cell <= 0.0:
        raise ValueError("internal_resistance_ohm_per_cell must be > 0.")
    if config.pack_mass_kg <= 0.0:
        raise ValueError("pack_mass_kg must be > 0.")
    if config.pack_heat_capacity_j_per_kgk <= 0.0:
        raise ValueError("pack_heat_capacity_j_per_kgk must be > 0.")
    if config.cooling_coeff_w_per_k < 0.0:
        raise ValueError("cooling_coeff_w_per_k must be >= 0.")
    if not 0.0 <= config.initial_soc <= 1.0:
        raise ValueError("initial_soc must be within [0, 1].")

    validate_electrical_model(config.electrical_model)
    validate_current_profile(config.current_profile)
    validate_degradation(config.degradation)

    group_count = max(1, config.group_count or config.cells_in_series)
    if config.cells_in_series % group_count != 0:
        raise ValueError(
            "group_count must evenly divide cells_in_series so each group represents an equal "
            "number of series cells."
        )
    return config
