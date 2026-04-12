from __future__ import annotations

import math
from dataclasses import replace

from .chemistry import available_chemistries
from .physics.electrical import validate_electrical_model
from .types import BalancingConfig, CurrentProfile, DegradationConfig, FaultConfig, PhysicsConfig, SimulationConfig, ThermalZoneConfig


def require_integral_seconds(value: object, field_name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{field_name} must be a numeric whole number of seconds.")

    numeric_value = float(value)
    if not math.isfinite(numeric_value):
        raise ValueError(f"{field_name} must be finite.")
    if numeric_value <= 0.0:
        raise ValueError(f"{field_name} must be > 0.")
    if not numeric_value.is_integer():
        raise ValueError(f"{field_name} must be an integer number of seconds; sub-second steps are not supported.")
    return int(numeric_value)


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
    if config.throughput_capacity_fade_per_ah is not None and config.throughput_capacity_fade_per_ah < 0.0:
        raise ValueError("throughput_capacity_fade_per_ah must be >= 0.")
    if config.throughput_resistance_growth_per_ah is not None and config.throughput_resistance_growth_per_ah < 0.0:
        raise ValueError("throughput_resistance_growth_per_ah must be >= 0.")
    if config.calendar_capacity_fade_per_hour < 0.0:
        raise ValueError("calendar_capacity_fade_per_hour must be >= 0.")
    if config.calendar_resistance_growth_per_hour < 0.0:
        raise ValueError("calendar_resistance_growth_per_hour must be >= 0.")
    if config.high_soc_capacity_accel < 0.0:
        raise ValueError("high_soc_capacity_accel must be >= 0.")
    if config.high_soc_resistance_accel < 0.0:
        raise ValueError("high_soc_resistance_accel must be >= 0.")
    if config.dod_stress_factor < 0.0:
        raise ValueError("dod_stress_factor must be >= 0.")
    if config.charge_stress_factor < 0.0:
        raise ValueError("charge_stress_factor must be >= 0.")
    if config.reference_temp_c is not None and config.reference_temp_c < -273.15:
        raise ValueError("reference_temp_c must be above absolute zero.")
    if config.temperature_accel_per_c is not None and config.temperature_accel_per_c < 0.0:
        raise ValueError("temperature_accel_per_c must be >= 0.")
    if not 0.0 <= config.high_soc_threshold <= 1.0:
        raise ValueError("high_soc_threshold must be within [0, 1].")
    if config.charge_current_stress_threshold_a is not None and config.charge_current_stress_threshold_a < 0.0:
        raise ValueError("charge_current_stress_threshold_a must be >= 0.")
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


def validate_thermal_zones(
    zones: tuple[ThermalZoneConfig, ...],
    group_count: int,
    group_zone_assignments: tuple[int, ...],
    group_labels: tuple[str, ...],
    group_entity_ids: tuple[str, ...],
) -> None:
    seen_zone_ids: set[int] = set()
    for zone in zones:
        if zone.zone_id in seen_zone_ids:
            raise ValueError(f"Thermal zone id {zone.zone_id} is duplicated.")
        if zone.cooling_coeff_w_per_k is not None and zone.cooling_coeff_w_per_k < 0.0:
            raise ValueError("Thermal zone cooling_coeff_w_per_k must be >= 0.")
        if zone.cooling_coeff_multiplier is not None and zone.cooling_coeff_multiplier <= 0.0:
            raise ValueError("Thermal zone cooling_coeff_multiplier must be > 0.")
        seen_zone_ids.add(zone.zone_id)

    if group_zone_assignments and len(group_zone_assignments) != group_count:
        raise ValueError("group_zone_assignments length must match group_count.")
    valid_zone_ids = {0, *seen_zone_ids}
    for zone_id in group_zone_assignments:
        if zone_id not in valid_zone_ids:
            raise ValueError(f"group_zone_assignments references unknown zone id {zone_id}.")

    if group_labels and len(group_labels) != group_count:
        raise ValueError("group_labels length must match group_count.")
    if group_entity_ids and len(group_entity_ids) != group_count:
        raise ValueError("group_entity_ids length must match group_count.")


def validate_physics(config: PhysicsConfig) -> PhysicsConfig:
    if not 0.0 < config.discharge_efficiency <= 1.0:
        raise ValueError("physics.discharge_efficiency must be within (0, 1].")
    if not 0.0 < config.charge_efficiency <= 1.0:
        raise ValueError("physics.charge_efficiency must be within (0, 1].")
    if config.capacity_cold_derate_per_c < 0.0:
        raise ValueError("physics.capacity_cold_derate_per_c must be >= 0.")
    if not 0.0 < config.min_capacity_scale <= 1.0:
        raise ValueError("physics.min_capacity_scale must be within (0, 1].")
    if config.self_discharge_per_day < 0.0:
        raise ValueError("physics.self_discharge_per_day must be >= 0.")
    if config.interconnect_resistance_ohm_per_group < 0.0:
        raise ValueError("physics.interconnect_resistance_ohm_per_group must be >= 0.")
    if config.pack_interconnect_resistance_ohm < 0.0:
        raise ValueError("physics.pack_interconnect_resistance_ohm must be >= 0.")
    if config.neighbor_thermal_coupling_w_per_k < 0.0:
        raise ValueError("physics.neighbor_thermal_coupling_w_per_k must be >= 0.")
    if config.ocv_curve:
        if len(config.ocv_curve) < 2:
            raise ValueError("physics.ocv_curve must contain at least two points when provided.")
        previous_soc = -1.0
        for point in config.ocv_curve:
            if not 0.0 <= point.soc <= 1.0:
                raise ValueError("physics.ocv_curve SOC values must be within [0, 1].")
            if point.soc <= previous_soc:
                raise ValueError("physics.ocv_curve SOC values must be strictly increasing.")
            previous_soc = point.soc
    if config.resistance_vs_soc_enabled:
        if len(config.resistance_soc_curve) < 2:
            raise ValueError("physics.resistance_soc_curve must contain at least two points when enabled.")
        previous_soc = -1.0
        for point in config.resistance_soc_curve:
            if not 0.0 <= point.soc <= 1.0:
                raise ValueError("physics.resistance_soc_curve SOC values must be within [0, 1].")
            if point.multiplier <= 0.0:
                raise ValueError("physics.resistance_soc_curve multipliers must be > 0.")
            if point.soc <= previous_soc:
                raise ValueError("physics.resistance_soc_curve SOC values must be strictly increasing.")
            previous_soc = point.soc
    if config.hysteresis_max_voltage_v < 0.0:
        raise ValueError("physics.hysteresis_max_voltage_v must be >= 0.")
    if config.hysteresis_response_rate_per_s < 0.0:
        raise ValueError("physics.hysteresis_response_rate_per_s must be >= 0.")
    if config.hysteresis_relaxation_tau_s <= 0.0:
        raise ValueError("physics.hysteresis_relaxation_tau_s must be > 0.")
    if config.hysteresis_current_scale_a <= 0.0:
        raise ValueError("physics.hysteresis_current_scale_a must be > 0.")
    if config.rc_low_soc_multiplier < 0.0:
        raise ValueError("physics.rc_low_soc_multiplier must be >= 0.")
    if config.rc_high_temp_multiplier_per_c < 0.0:
        raise ValueError("physics.rc_high_temp_multiplier_per_c must be >= 0.")
    if config.diffusion_stress_max_v < 0.0:
        raise ValueError("physics.diffusion_stress_max_v must be >= 0.")
    if config.diffusion_stress_build_rate_per_s < 0.0:
        raise ValueError("physics.diffusion_stress_build_rate_per_s must be >= 0.")
    if config.diffusion_stress_decay_tau_s <= 0.0:
        raise ValueError("physics.diffusion_stress_decay_tau_s must be > 0.")
    if config.diffusion_stress_current_scale_a <= 0.0:
        raise ValueError("physics.diffusion_stress_current_scale_a must be > 0.")
    if config.diffusion_stress_resistance_coeff < 0.0:
        raise ValueError("physics.diffusion_stress_resistance_coeff must be >= 0.")
    if config.core_surface_thermal_coupling_w_per_k < 0.0:
        raise ValueError("physics.core_surface_thermal_coupling_w_per_k must be >= 0.")
    if not 0.0 < config.surface_thermal_mass_fraction < 1.0:
        raise ValueError("physics.surface_thermal_mass_fraction must be within (0, 1).")
    if config.core_thermal_mass_j_per_k is not None and config.core_thermal_mass_j_per_k <= 0.0:
        raise ValueError("physics.core_thermal_mass_j_per_k must be > 0 when provided.")
    if config.surface_thermal_mass_j_per_k is not None and config.surface_thermal_mass_j_per_k <= 0.0:
        raise ValueError("physics.surface_thermal_mass_j_per_k must be > 0 when provided.")
    if config.nonlinear_cooling_delta_threshold_c < 0.0:
        raise ValueError("physics.nonlinear_cooling_delta_threshold_c must be >= 0.")
    if config.nonlinear_cooling_gain_per_c < 0.0:
        raise ValueError("physics.nonlinear_cooling_gain_per_c must be >= 0.")
    if config.reversible_heat_coeff_v_per_k < 0.0:
        raise ValueError("physics.reversible_heat_coeff_v_per_k must be >= 0.")
    if config.charge_resistance_multiplier <= 0.0:
        raise ValueError("physics.charge_resistance_multiplier must be > 0.")
    if config.discharge_resistance_multiplier <= 0.0:
        raise ValueError("physics.discharge_resistance_multiplier must be > 0.")
    return config


def validate_simulation_config(config: SimulationConfig) -> SimulationConfig:
    if config.duration_s <= 0:
        raise ValueError("duration_s must be > 0.")
    config = replace(config, time_step_s=require_integral_seconds(config.time_step_s, "time_step_s"))
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
    if config.chemistry_name not in available_chemistries():
        raise ValueError(
            f"chemistry_name must be one of {', '.join(available_chemistries())}."
        )

    validate_electrical_model(config.electrical_model)
    validate_current_profile(config.current_profile)
    validate_degradation(config.degradation)
    validate_balancing(config.balancing)
    validate_physics(config.physics)

    group_count = max(1, config.group_count or config.cells_in_series)
    if config.cells_in_series % group_count != 0:
        raise ValueError(
            "group_count must evenly divide cells_in_series so each group represents an equal "
            "number of series cells."
        )
    validate_faults(config.faults, group_count)
    validate_thermal_zones(
        config.thermal_zones,
        group_count,
        config.group_zone_assignments,
        config.group_labels,
        config.group_entity_ids,
    )
    return config
