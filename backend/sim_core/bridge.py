from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .engine import run_simulation
from .types import SimulationConfig, SimulationResult


def simulation_config_from_dict(payload: dict[str, Any]) -> SimulationConfig:
    return SimulationConfig(
        cell_nominal_voltage=float(payload["cell_nominal_voltage"]),
        cell_full_voltage=float(payload["cell_full_voltage"]),
        cell_empty_voltage=float(payload["cell_empty_voltage"]),
        cell_cutoff_voltage=float(payload["cell_cutoff_voltage"]),
        cell_capacity_ah=float(payload["cell_capacity_ah"]),
        cells_in_series=int(payload["cells_in_series"]),
        cells_in_parallel=int(payload["cells_in_parallel"]),
        internal_resistance_ohm_per_cell=float(payload["internal_resistance_ohm_per_cell"]),
        ambient_temp_c=float(payload["ambient_temp_c"]),
        discharge_current_a=float(payload["discharge_current_a"]),
        duration_s=int(payload["duration_s"]),
        time_step_s=int(payload["time_step_s"]),
        initial_soc=float(payload["initial_soc"]),
        pack_mass_kg=float(payload["pack_mass_kg"]),
        pack_heat_capacity_j_per_kgk=float(payload["pack_heat_capacity_j_per_kgk"]),
        cooling_coeff_w_per_k=float(payload["cooling_coeff_w_per_k"]),
    )


def run_simulation_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    config = simulation_config_from_dict(payload)
    result = run_simulation(config)
    return simulation_result_to_dict(result)


def simulation_result_to_dict(result: SimulationResult) -> dict[str, Any]:
    return asdict(result)
