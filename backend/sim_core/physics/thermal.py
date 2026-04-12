from __future__ import annotations

import math
from dataclasses import dataclass

from ..types import SimulationConfig, ThermalZoneConfig


@dataclass(frozen=True)
class GroupThermalContext:
    zone_id: int
    ambient_temp_c: float
    cooling_coeff_w_per_k: float


@dataclass(frozen=True)
class GroupThermalStepResult:
    core_temp_c: float
    surface_temp_c: float
    representative_temp_c: float
    substep_count: int = 1


def compute_cooling_w(temp_c: float, ambient_temp_c: float, cooling_coeff_w_per_k: float) -> float:
    return cooling_coeff_w_per_k * (temp_c - ambient_temp_c)


def compute_next_temperature_c(
    temp_c: float,
    heat_w: float,
    ambient_temp_c: float,
    cooling_coeff_w_per_k: float,
    thermal_mass_j_per_k: float,
    dt_s: int,
    left_neighbor_temp_c: float | None = None,
    right_neighbor_temp_c: float | None = None,
    neighbor_coupling_w_per_k: float = 0.0,
) -> float:
    cooling_w = compute_cooling_w(
        temp_c=temp_c,
        ambient_temp_c=ambient_temp_c,
        cooling_coeff_w_per_k=cooling_coeff_w_per_k,
    )
    neighbor_exchange_w = 0.0
    if left_neighbor_temp_c is not None:
        neighbor_exchange_w += neighbor_coupling_w_per_k * (left_neighbor_temp_c - temp_c)
    if right_neighbor_temp_c is not None:
        neighbor_exchange_w += neighbor_coupling_w_per_k * (right_neighbor_temp_c - temp_c)
    net_heat_w = heat_w - cooling_w + neighbor_exchange_w
    delta_temp_c = net_heat_w * dt_s / max(thermal_mass_j_per_k, 1e-9)
    return temp_c + delta_temp_c


def effective_cooling_coeff_w_per_k(
    temp_c: float,
    ambient_temp_c: float,
    cooling_coeff_w_per_k: float,
    config: SimulationConfig,
) -> float:
    if not config.physics.nonlinear_cooling_enabled:
        return cooling_coeff_w_per_k

    delta_c = abs(temp_c - ambient_temp_c)
    excess_c = max(0.0, delta_c - config.physics.nonlinear_cooling_delta_threshold_c)
    return cooling_coeff_w_per_k * (1.0 + excess_c * config.physics.nonlinear_cooling_gain_per_c)


def _resolved_node_masses(thermal_mass_j_per_k: float, config: SimulationConfig) -> tuple[float, float]:
    if config.physics.core_thermal_mass_j_per_k is not None or config.physics.surface_thermal_mass_j_per_k is not None:
        surface_mass = max(
            config.physics.surface_thermal_mass_j_per_k
            if config.physics.surface_thermal_mass_j_per_k is not None
            else thermal_mass_j_per_k * config.physics.surface_thermal_mass_fraction,
            1e-9,
        )
        core_mass = max(
            config.physics.core_thermal_mass_j_per_k
            if config.physics.core_thermal_mass_j_per_k is not None
            else max(thermal_mass_j_per_k - surface_mass, 1e-9),
            1e-9,
        )
        return core_mass, surface_mass

    surface_mass = max(thermal_mass_j_per_k * config.physics.surface_thermal_mass_fraction, 1e-9)
    core_mass = max(thermal_mass_j_per_k - surface_mass, 1e-9)
    return core_mass, surface_mass


def _neighbor_count(left_neighbor_temp_c: float | None, right_neighbor_temp_c: float | None) -> int:
    return int(left_neighbor_temp_c is not None) + int(right_neighbor_temp_c is not None)


def _recommended_internal_dt_s(
    *,
    core_temp_c: float,
    surface_temp_c: float,
    ambient_temp_c: float,
    cooling_coeff_w_per_k: float,
    thermal_mass_j_per_k: float,
    config: SimulationConfig,
    left_neighbor_surface_temp_c: float | None,
    right_neighbor_surface_temp_c: float | None,
) -> float:
    neighbor_conductance = config.physics.neighbor_thermal_coupling_w_per_k * _neighbor_count(
        left_neighbor_surface_temp_c,
        right_neighbor_surface_temp_c,
    )
    cooling_conductance = effective_cooling_coeff_w_per_k(
        surface_temp_c,
        ambient_temp_c,
        cooling_coeff_w_per_k,
        config,
    )

    if not config.physics.two_node_thermal_enabled:
        total_conductance = cooling_conductance + neighbor_conductance
        if total_conductance <= 0.0:
            return math.inf
        return 0.25 * thermal_mass_j_per_k / total_conductance

    core_mass, surface_mass = _resolved_node_masses(thermal_mass_j_per_k, config)
    core_conductance = max(config.physics.core_surface_thermal_coupling_w_per_k, 0.0)
    surface_conductance = core_conductance + cooling_conductance + neighbor_conductance
    stable_core_dt = math.inf if core_conductance <= 0.0 else 0.25 * core_mass / core_conductance
    stable_surface_dt = math.inf if surface_conductance <= 0.0 else 0.25 * surface_mass / surface_conductance
    return min(stable_core_dt, stable_surface_dt)


def _thermal_substep_count(dt_s: float, recommended_dt_s: float) -> int:
    if not math.isfinite(recommended_dt_s) or recommended_dt_s <= 0.0:
        return 1
    return max(1, math.ceil(dt_s / max(recommended_dt_s, 1e-6)))


def _assert_finite_temperature(temp_c: float, label: str) -> None:
    if not math.isfinite(temp_c):
        raise FloatingPointError(f"{label} became non-finite during thermal integration.")
    if temp_c < -273.15 or temp_c > 2000.0:
        raise FloatingPointError(f"{label} reached an implausible value during thermal integration: {temp_c:.2f} C")


def _clamp_sub_ambient_overshoot(temp_c: float, ambient_temp_c: float) -> float:
    return max(temp_c, ambient_temp_c)


def compute_next_group_temperatures(
    core_temp_c: float,
    surface_temp_c: float,
    heat_w: float,
    ambient_temp_c: float,
    cooling_coeff_w_per_k: float,
    thermal_mass_j_per_k: float,
    dt_s: int,
    config: SimulationConfig,
    left_neighbor_surface_temp_c: float | None = None,
    right_neighbor_surface_temp_c: float | None = None,
) -> GroupThermalStepResult:
    recommended_dt_s = _recommended_internal_dt_s(
        core_temp_c=core_temp_c,
        surface_temp_c=surface_temp_c,
        ambient_temp_c=ambient_temp_c,
        cooling_coeff_w_per_k=cooling_coeff_w_per_k,
        thermal_mass_j_per_k=thermal_mass_j_per_k,
        config=config,
        left_neighbor_surface_temp_c=left_neighbor_surface_temp_c,
        right_neighbor_surface_temp_c=right_neighbor_surface_temp_c,
    )
    substep_count = _thermal_substep_count(float(dt_s), recommended_dt_s)
    sub_dt_s = float(dt_s) / substep_count

    if not config.physics.two_node_thermal_enabled:
        next_temp_c = surface_temp_c
        for _ in range(substep_count):
            next_temp_c = compute_next_temperature_c(
                temp_c=next_temp_c,
                heat_w=heat_w,
                ambient_temp_c=ambient_temp_c,
                cooling_coeff_w_per_k=effective_cooling_coeff_w_per_k(next_temp_c, ambient_temp_c, cooling_coeff_w_per_k, config),
                thermal_mass_j_per_k=thermal_mass_j_per_k,
                dt_s=sub_dt_s,
                left_neighbor_temp_c=left_neighbor_surface_temp_c,
                right_neighbor_temp_c=right_neighbor_surface_temp_c,
                neighbor_coupling_w_per_k=config.physics.neighbor_thermal_coupling_w_per_k,
            )
            next_temp_c = _clamp_sub_ambient_overshoot(next_temp_c, ambient_temp_c)
            _assert_finite_temperature(next_temp_c, "surface_temp_c")
        return GroupThermalStepResult(
            core_temp_c=next_temp_c,
            surface_temp_c=next_temp_c,
            representative_temp_c=next_temp_c,
            substep_count=substep_count,
        )

    core_mass, surface_mass = _resolved_node_masses(thermal_mass_j_per_k, config)
    next_core_temp_c = core_temp_c
    next_surface_temp_c = surface_temp_c
    for _ in range(substep_count):
        coupling_w = config.physics.core_surface_thermal_coupling_w_per_k * (next_core_temp_c - next_surface_temp_c)
        cooling_coeff = effective_cooling_coeff_w_per_k(next_surface_temp_c, ambient_temp_c, cooling_coeff_w_per_k, config)
        cooling_w = cooling_coeff * (next_surface_temp_c - ambient_temp_c)
        neighbor_exchange_w = 0.0
        if left_neighbor_surface_temp_c is not None:
            neighbor_exchange_w += config.physics.neighbor_thermal_coupling_w_per_k * (
                left_neighbor_surface_temp_c - next_surface_temp_c
            )
        if right_neighbor_surface_temp_c is not None:
            neighbor_exchange_w += config.physics.neighbor_thermal_coupling_w_per_k * (
                right_neighbor_surface_temp_c - next_surface_temp_c
            )

        next_core_temp_c = next_core_temp_c + ((heat_w - coupling_w) * sub_dt_s / core_mass)
        next_surface_temp_c = next_surface_temp_c + (
            (coupling_w - cooling_w + neighbor_exchange_w) * sub_dt_s / surface_mass
        )
        next_core_temp_c = _clamp_sub_ambient_overshoot(next_core_temp_c, ambient_temp_c)
        next_surface_temp_c = _clamp_sub_ambient_overshoot(next_surface_temp_c, ambient_temp_c)
        _assert_finite_temperature(next_core_temp_c, "core_temp_c")
        _assert_finite_temperature(next_surface_temp_c, "surface_temp_c")

    return GroupThermalStepResult(
        core_temp_c=next_core_temp_c,
        surface_temp_c=next_surface_temp_c,
        representative_temp_c=next_surface_temp_c,
        substep_count=substep_count,
    )


def resolve_group_thermal_context(config: SimulationConfig, group_index: int) -> GroupThermalContext:
    zone_id = 0
    if config.group_zone_assignments:
        zone_id = config.group_zone_assignments[group_index]

    zones_by_id: dict[int, ThermalZoneConfig] = {zone.zone_id: zone for zone in config.thermal_zones}
    zone = zones_by_id.get(zone_id)

    ambient_temp_c = config.ambient_temp_c if zone is None or zone.ambient_temp_c is None else zone.ambient_temp_c
    cooling_coeff_w_per_k = config.cooling_coeff_w_per_k
    if zone is not None:
        if zone.cooling_coeff_w_per_k is not None:
            cooling_coeff_w_per_k = zone.cooling_coeff_w_per_k
        elif zone.cooling_coeff_multiplier is not None:
            cooling_coeff_w_per_k *= zone.cooling_coeff_multiplier

    return GroupThermalContext(
        zone_id=zone_id,
        ambient_temp_c=ambient_temp_c,
        cooling_coeff_w_per_k=cooling_coeff_w_per_k,
    )
