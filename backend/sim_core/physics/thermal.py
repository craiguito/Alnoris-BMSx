from __future__ import annotations


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
