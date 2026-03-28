from __future__ import annotations

from dataclasses import dataclass

from ..types import SimulationConfig, ThermalZoneConfig


@dataclass(frozen=True)
class GroupThermalContext:
    zone_id: int
    ambient_temp_c: float
    cooling_coeff_w_per_k: float


def compute_cooling_w(temp_c: float, ambient_temp_c: float, cooling_coeff_w_per_k: float) -> float:
    return cooling_coeff_w_per_k * (temp_c - ambient_temp_c)


def compute_next_temperature_c(
    temp_c: float,
    heat_w: float,
    ambient_temp_c: float,
    cooling_coeff_w_per_k: float,
    thermal_mass_j_per_k: float,
    dt_s: int,
) -> float:
    cooling_w = compute_cooling_w(
        temp_c=temp_c,
        ambient_temp_c=ambient_temp_c,
        cooling_coeff_w_per_k=cooling_coeff_w_per_k,
    )
    net_heat_w = heat_w - cooling_w
    delta_temp_c = net_heat_w * dt_s / max(thermal_mass_j_per_k, 1e-9)
    return temp_c + delta_temp_c


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
