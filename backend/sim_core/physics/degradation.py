from __future__ import annotations

"""Pragmatic degradation model for early-stage battery engineering analysis.

The model combines:
- throughput aging from charge/discharge processing
- calendar aging from time at temperature
- high-SOC storage stress
- simple cycle/depth-of-discharge stress
- charging stress near high SOC

It is intentionally interpretable and lightweight, not an electrochemical life model.
"""

from dataclasses import dataclass

from ..types import CellGroupState, DegradationConfig, DegradationState


@dataclass(frozen=True)
class DegradationStepResult:
    next_state: DegradationState
    capacity_loss_increment: float
    resistance_growth_increment: float


def _throughput_capacity_coeff(config: DegradationConfig) -> float:
    return (
        config.throughput_capacity_fade_per_ah
        if config.throughput_capacity_fade_per_ah is not None
        else config.capacity_fade_per_throughput_ah
    )


def _throughput_resistance_coeff(config: DegradationConfig) -> float:
    return (
        config.throughput_resistance_growth_per_ah
        if config.throughput_resistance_growth_per_ah is not None
        else config.resistance_growth_per_throughput_ah
    )


def _reference_temp_c(config: DegradationConfig) -> float:
    return config.reference_temp_c if config.reference_temp_c is not None else config.temperature_reference_c


def _temperature_accel_per_c(config: DegradationConfig) -> float:
    return (
        config.temperature_accel_per_c
        if config.temperature_accel_per_c is not None
        else config.temperature_acceleration_per_c
    )


def _dod_stress_factor(config: DegradationConfig) -> float:
    return config.dod_stress_factor if config.dod_stress_factor > 0.0 else config.depth_of_discharge_weight


def _temperature_acceleration(config: DegradationConfig, temp_c: float) -> float:
    return 1.0 + max(0.0, temp_c - _reference_temp_c(config)) * _temperature_accel_per_c(config)


def _high_soc_fraction(config: DegradationConfig, soc: float) -> float:
    if soc <= config.high_soc_threshold:
        return 0.0
    remaining_span = max(1.0 - config.high_soc_threshold, 1e-9)
    return min((soc - config.high_soc_threshold) / remaining_span, 1.0)


def step_degradation(
    state: DegradationState,
    config: DegradationConfig,
    current_a: float,
    dt_s: int,
    temp_c: float,
    soc: float,
) -> DegradationStepResult:
    throughput_ah = abs(current_a) * dt_s / 3600.0
    charge_throughput_ah = abs(min(current_a, 0.0)) * dt_s / 3600.0
    discharge_throughput_ah = max(current_a, 0.0) * dt_s / 3600.0
    dt_hours = dt_s / 3600.0

    temp_acceleration = _temperature_acceleration(config, temp_c)
    high_soc_fraction = _high_soc_fraction(config, soc)
    high_soc_storage_factor = 1.0 + high_soc_fraction * config.high_soc_capacity_accel
    high_soc_resistance_factor = 1.0 + high_soc_fraction * config.high_soc_resistance_accel

    if state.cumulative_time_s <= 0.0:
        previous_min_soc = soc
        previous_max_soc = soc
    else:
        previous_min_soc = min(state.soc_window_min, soc)
        previous_max_soc = max(state.soc_window_max, soc)
    cycle_swing = max(previous_max_soc - previous_min_soc, 0.0)
    cycle_stress_increment = throughput_ah * (1.0 + _dod_stress_factor(config) * cycle_swing)

    charge_stress_factor = 1.0
    if current_a < 0.0:
        charge_stress_factor += config.charge_stress_factor * high_soc_fraction
        if config.charge_current_stress_threshold_a is not None and abs(current_a) > config.charge_current_stress_threshold_a:
            current_excess = abs(current_a) - config.charge_current_stress_threshold_a
            charge_stress_factor += config.charge_stress_factor * (
                current_excess / max(config.charge_current_stress_threshold_a, 1e-9)
            )

    throughput_capacity_increment = (
        throughput_ah
        * _throughput_capacity_coeff(config)
        * temp_acceleration
        * (1.0 + _dod_stress_factor(config) * cycle_swing)
        * charge_stress_factor
    )
    throughput_resistance_increment = (
        throughput_ah
        * _throughput_resistance_coeff(config)
        * temp_acceleration
        * (1.0 + 0.5 * _dod_stress_factor(config) * cycle_swing)
        * charge_stress_factor
    )

    calendar_capacity_increment = (
        dt_hours
        * config.calendar_capacity_fade_per_hour
        * temp_acceleration
        * high_soc_storage_factor
    )
    calendar_resistance_increment = (
        dt_hours
        * config.calendar_resistance_growth_per_hour
        * temp_acceleration
        * high_soc_resistance_factor
    )

    total_capacity_increment = throughput_capacity_increment + calendar_capacity_increment
    total_resistance_increment = throughput_resistance_increment + calendar_resistance_increment

    return DegradationStepResult(
        next_state=DegradationState(
            cumulative_throughput_ah=state.cumulative_throughput_ah + throughput_ah,
            cumulative_time_s=state.cumulative_time_s + dt_s,
            cumulative_charge_throughput_ah=state.cumulative_charge_throughput_ah + charge_throughput_ah,
            cumulative_discharge_throughput_ah=state.cumulative_discharge_throughput_ah + discharge_throughput_ah,
            cumulative_high_soc_time_s=state.cumulative_high_soc_time_s + (dt_s if soc >= config.high_soc_threshold else 0.0),
            cumulative_cycle_stress=state.cumulative_cycle_stress + cycle_stress_increment,
            capacity_loss_fraction=min(state.capacity_loss_fraction + total_capacity_increment, 0.35),
            resistance_growth_fraction=min(state.resistance_growth_fraction + total_resistance_increment, 2.0),
            soc_window_min=previous_min_soc,
            soc_window_max=previous_max_soc,
        ),
        capacity_loss_increment=total_capacity_increment,
        resistance_growth_increment=total_resistance_increment,
    )


def effective_capacity_scale(group: CellGroupState) -> float:
    return max(group.capacity_scale * (1.0 - group.degradation.capacity_loss_fraction), 0.1)


def effective_resistance_scale(group: CellGroupState) -> float:
    return max(group.resistance_scale * (1.0 + group.degradation.resistance_growth_fraction), 0.05)
