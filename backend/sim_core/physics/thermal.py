from __future__ import annotations

from ..types import SimulationConfig


def compute_cooling_w(temp_c: float, ambient_temp_c: float, cooling_coeff_w_per_k: float) -> float:
    return cooling_coeff_w_per_k * (temp_c - ambient_temp_c)


def compute_next_temperature_c(
    temp_c: float,
    heat_w: float,
    config: SimulationConfig,
) -> float:
    cooling_w = compute_cooling_w(
        temp_c=temp_c,
        ambient_temp_c=config.ambient_temp_c,
        cooling_coeff_w_per_k=config.cooling_coeff_w_per_k,
    )
    net_heat_w = heat_w - cooling_w
    delta_temp_c = (
        net_heat_w * config.time_step_s
    ) / (config.pack_mass_kg * config.pack_heat_capacity_j_per_kgk)
    return temp_c + delta_temp_c
