from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class SimulationConfig:
    cell_nominal_voltage: float
    cell_full_voltage: float
    cell_empty_voltage: float
    cell_cutoff_voltage: float
    cell_capacity_ah: float
    cells_in_series: int
    cells_in_parallel: int
    internal_resistance_ohm_per_cell: float
    ambient_temp_c: float
    discharge_current_a: float
    duration_s: int
    time_step_s: int
    initial_soc: float
    pack_mass_kg: float
    pack_heat_capacity_j_per_kgk: float
    cooling_coeff_w_per_k: float


@dataclass(frozen=True)
class SimulationWarning:
    code: str
    message: str
    severity: str


@dataclass(frozen=True)
class PackProperties:
    nominal_voltage_v: float
    capacity_ah: float
    theoretical_energy_wh: float
    resistance_ohm: float
    capacity_as: float


@dataclass(frozen=True)
class SimulationPoint:
    time_s: int
    soc: float
    terminal_voltage_v: float
    power_w: float
    heat_w: float
    temp_c: float


@dataclass(frozen=True)
class SimulationSummary:
    runtime_s: int
    delivered_energy_wh: float
    delivered_capacity_ah: float
    min_terminal_voltage_v: float
    peak_temp_c: float
    final_soc: float
    termination_reason: str
    warnings: list[SimulationWarning]


@dataclass(frozen=True)
class SimulationResult:
    pack_nominal_voltage_v: float
    pack_capacity_ah: float
    theoretical_energy_wh: float
    summary: SimulationSummary
    time_series: list[SimulationPoint]
