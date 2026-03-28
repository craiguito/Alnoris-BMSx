from __future__ import annotations

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
    if not config.physics.two_node_thermal_enabled:
        next_temp_c = compute_next_temperature_c(
            temp_c=surface_temp_c,
            heat_w=heat_w,
            ambient_temp_c=ambient_temp_c,
            cooling_coeff_w_per_k=effective_cooling_coeff_w_per_k(surface_temp_c, ambient_temp_c, cooling_coeff_w_per_k, config),
            thermal_mass_j_per_k=thermal_mass_j_per_k,
            dt_s=dt_s,
            left_neighbor_temp_c=left_neighbor_surface_temp_c,
            right_neighbor_temp_c=right_neighbor_surface_temp_c,
            neighbor_coupling_w_per_k=config.physics.neighbor_thermal_coupling_w_per_k,
        )
        return GroupThermalStepResult(
            core_temp_c=next_temp_c,
            surface_temp_c=next_temp_c,
            representative_temp_c=next_temp_c,
        )

    surface_mass = max(thermal_mass_j_per_k * config.physics.surface_thermal_mass_fraction, 1e-9)
    core_mass = max(thermal_mass_j_per_k - surface_mass, 1e-9)
    coupling_w = config.physics.core_surface_thermal_coupling_w_per_k * (core_temp_c - surface_temp_c)
    cooling_coeff = effective_cooling_coeff_w_per_k(surface_temp_c, ambient_temp_c, cooling_coeff_w_per_k, config)
    cooling_w = cooling_coeff * (surface_temp_c - ambient_temp_c)
    neighbor_exchange_w = 0.0
    if left_neighbor_surface_temp_c is not None:
        neighbor_exchange_w += config.physics.neighbor_thermal_coupling_w_per_k * (left_neighbor_surface_temp_c - surface_temp_c)
    if right_neighbor_surface_temp_c is not None:
        neighbor_exchange_w += config.physics.neighbor_thermal_coupling_w_per_k * (right_neighbor_surface_temp_c - surface_temp_c)

    next_core_temp_c = core_temp_c + ((heat_w - coupling_w) * dt_s / core_mass)
    next_surface_temp_c = surface_temp_c + ((coupling_w - cooling_w + neighbor_exchange_w) * dt_s / surface_mass)
    return GroupThermalStepResult(
        core_temp_c=next_core_temp_c,
        surface_temp_c=next_surface_temp_c,
        representative_temp_c=next_surface_temp_c,
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
