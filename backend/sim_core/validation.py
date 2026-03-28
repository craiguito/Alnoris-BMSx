from __future__ import annotations

from .physics.electrical import validate_electrical_model
from .types import BalancingConfig, CurrentProfile, DegradationConfig, FaultConfig, SimulationConfig


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


def validate_balancing(config: BalancingConfig) -> BalancingConfig:
    if not config.enabled:
        return config
    if config.mode != "passive":
        raise ValueError("Only passive balancing mode is currently supported.")
    if config.bleed_current_a <= 0.0:
        raise ValueError("balancing.bleed_current_a must be > 0 when balancing is enabled.")
    if config.soc_threshold is None and config.voltage_threshold_v is None:
        raise ValueError("Balancing requires soc_threshold and/or voltage_threshold_v.")
    if config.soc_threshold is not None and not 0.0 <= config.soc_threshold <= 1.0:
        raise ValueError("balancing.soc_threshold must be within [0, 1].")
    if config.voltage_threshold_v is not None and config.voltage_threshold_v <= 0.0:
        raise ValueError("balancing.voltage_threshold_v must be > 0.")
    if config.max_active_groups is not None and config.max_active_groups < 1:
        raise ValueError("balancing.max_active_groups must be >= 1 when provided.")
    return config


def validate_faults(config: FaultConfig, group_count: int) -> FaultConfig:
    supported_faults = {
        "high_resistance_group",
        "low_capacity_group",
        "elevated_self_heating_group",
        "stuck_high_soc_group",
        "cooling_loss_group",
    }
    for fault in config.faults:
        if fault.fault_type not in supported_faults:
            raise ValueError(f"Unsupported fault type: {fault.fault_type}")
        if not 0 <= fault.group_index < group_count:
            raise ValueError(f"Fault group_index {fault.group_index} is out of range for {group_count} groups.")
        if fault.factor <= 0.0:
            raise ValueError(f"Fault factor for {fault.fault_type} must be > 0.")
        if fault.fault_type in {"high_resistance_group", "elevated_self_heating_group"} and fault.factor < 1.0:
            raise ValueError(f"Fault factor for {fault.fault_type} must be >= 1.")
        if fault.fault_type in {"low_capacity_group", "cooling_loss_group"} and fault.factor > 1.0:
            raise ValueError(f"Fault factor for {fault.fault_type} must be <= 1.")
        if fault.fault_type == "stuck_high_soc_group" and fault.factor > 1.0:
            raise ValueError("Fault factor for stuck_high_soc_group must be <= 1.")
        if fault.start_time_s < 0.0:
            raise ValueError("Fault start_time_s must be >= 0.")
        if fault.end_time_s is not None and fault.end_time_s < fault.start_time_s:
            raise ValueError("Fault end_time_s must be >= start_time_s.")
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
    validate_balancing(config.balancing)

    group_count = max(1, config.group_count or config.cells_in_series)
    if config.cells_in_series % group_count != 0:
        raise ValueError(
            "group_count must evenly divide cells_in_series so each group represents an equal "
            "number of series cells."
        )
    validate_faults(config.faults, group_count)
    return config
