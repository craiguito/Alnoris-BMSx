from __future__ import annotations

import math

from ..types import PackProperties, SimulationConfig


def compute_open_circuit_voltage(soc: float, config: SimulationConfig, pack: PackProperties) -> float:
    empty_pack_voltage_v = config.cell_empty_voltage * config.cells_in_series
    full_pack_voltage_v = config.cell_full_voltage * config.cells_in_series
    nominal_pack_voltage_v = pack.nominal_voltage_v
    usable_span_v = max(full_pack_voltage_v - empty_pack_voltage_v, 1e-9)

    reference_fraction = (nominal_pack_voltage_v - empty_pack_voltage_v) / usable_span_v
    reference_fraction = min(max(reference_fraction, 1e-6), 1 - 1e-6)
    nominal_soc_anchor = 0.5
    ocv_exponent = math.log(reference_fraction) / math.log(nominal_soc_anchor)

    clamped_soc = min(max(soc, 0.0), 1.0)
    return empty_pack_voltage_v + usable_span_v * (clamped_soc**ocv_exponent)


def compute_terminal_voltage(
    open_circuit_voltage_v: float,
    current_a: float,
    pack_resistance_ohm: float,
) -> float:
    return max(open_circuit_voltage_v - current_a * pack_resistance_ohm, 0.0)


def compute_power_w(terminal_voltage_v: float, current_a: float) -> float:
    return terminal_voltage_v * current_a


def compute_heat_w(current_a: float, pack_resistance_ohm: float) -> float:
    return (current_a**2) * pack_resistance_ohm


def compute_next_soc(current_soc: float, config: SimulationConfig, capacity_as: float) -> float:
    used_charge_as = config.discharge_current_a * config.time_step_s
    return current_soc - (used_charge_as / capacity_as)
