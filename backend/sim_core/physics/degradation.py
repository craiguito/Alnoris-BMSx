from __future__ import annotations

from ..types import CellGroupState, DegradationConfig, DegradationState


def step_degradation(
    state: DegradationState,
    config: DegradationConfig,
    current_a: float,
    dt_s: int,
    temp_c: float,
    soc: float,
) -> DegradationState:
    throughput_ah = abs(current_a) * dt_s / 3600.0
    temp_acceleration = 1.0 + max(0.0, temp_c - config.temperature_reference_c) * config.temperature_acceleration_per_c
    depth_of_discharge_factor = 1.0 + config.depth_of_discharge_weight * max(0.0, 1.0 - soc)

    capacity_loss = state.capacity_loss_fraction + (
        throughput_ah * config.capacity_fade_per_throughput_ah * temp_acceleration * depth_of_discharge_factor
    )
    resistance_growth = state.resistance_growth_fraction + (
        throughput_ah * config.resistance_growth_per_throughput_ah * temp_acceleration
    )

    return DegradationState(
        cumulative_throughput_ah=state.cumulative_throughput_ah + throughput_ah,
        capacity_loss_fraction=min(capacity_loss, 0.35),
        resistance_growth_fraction=min(resistance_growth, 2.0),
    )


def effective_capacity_scale(group: CellGroupState) -> float:
    return max(group.capacity_scale * (1.0 - group.degradation.capacity_loss_fraction), 0.1)


def effective_resistance_scale(group: CellGroupState) -> float:
    return max(group.resistance_scale * (1.0 + group.degradation.resistance_growth_fraction), 0.05)
