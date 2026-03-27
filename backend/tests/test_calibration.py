from __future__ import annotations

import unittest

from backend.sim_core.engine import run_simulation
from backend.sim_core.physics.electrical import compute_open_circuit_voltage
from backend.sim_core.physics.pack import derive_pack_properties
from backend.sim_core.reference_cells import REFERENCE_CELLS
from backend.sim_core.types import SimulationConfig


def make_config(cell_key: str) -> SimulationConfig:
    cell = REFERENCE_CELLS[cell_key]
    return SimulationConfig(
        cell_nominal_voltage=cell.cell_nominal_voltage,
        cell_full_voltage=cell.cell_full_voltage,
        cell_empty_voltage=cell.cell_empty_voltage,
        cell_cutoff_voltage=cell.cell_cutoff_voltage,
        cell_capacity_ah=cell.cell_capacity_ah,
        cells_in_series=1,
        cells_in_parallel=1,
        internal_resistance_ohm_per_cell=cell.internal_resistance_ohm_per_cell,
        ambient_temp_c=25.0,
        discharge_current_a=cell.recommended_discharge_current_a,
        duration_s=7200,
        time_step_s=1,
        initial_soc=1.0,
        pack_mass_kg=cell.pack_mass_kg,
        pack_heat_capacity_j_per_kgk=cell.pack_heat_capacity_j_per_kgk,
        cooling_coeff_w_per_k=cell.cooling_coeff_w_per_k,
    )


class CalibrationTests(unittest.TestCase):
    def test_open_circuit_curve_matches_full_nominal_and_empty_metrics(self) -> None:
        for key, cell in REFERENCE_CELLS.items():
            config = make_config(key)
            pack = derive_pack_properties(config)

            full_v = compute_open_circuit_voltage(1.0, config, pack)
            nominal_v = compute_open_circuit_voltage(0.5, config, pack)
            empty_v = compute_open_circuit_voltage(0.0, config, pack)

            self.assertAlmostEqual(full_v, cell.cell_full_voltage, places=6, msg=cell.name)
            self.assertAlmostEqual(nominal_v, cell.cell_nominal_voltage, places=6, msg=cell.name)
            self.assertAlmostEqual(empty_v, cell.cell_empty_voltage, places=6, msg=cell.name)

    def test_one_c_discharge_runs_close_to_expected_capacity_window(self) -> None:
        for key, cell in REFERENCE_CELLS.items():
            config = make_config(key)
            one_c_current = cell.cell_capacity_ah
            config = SimulationConfig(
                **{
                    **config.__dict__,
                    "discharge_current_a": one_c_current,
                    "duration_s": 5400,
                }
            )

            result = run_simulation(config)
            elapsed_s = result.time_series[-1].time_s
            delivered_ah = one_c_current * elapsed_s / 3600.0

            self.assertGreater(delivered_ah, 0.75 * cell.cell_capacity_ah, cell.name)
            self.assertLess(delivered_ah, 1.05 * cell.cell_capacity_ah, cell.name)

    def test_higher_power_cell_sags_less_than_energy_cell_at_same_current(self) -> None:
        energy_cell = make_config("panasonic_ncr18650b")
        power_cell = make_config("samsung_30q")
        pack_energy = derive_pack_properties(energy_cell)
        pack_power = derive_pack_properties(power_cell)

        current_a = 5.0
        soc = 0.8
        energy_v = compute_open_circuit_voltage(soc, energy_cell, pack_energy) - (
            current_a * pack_energy.resistance_ohm
        )
        power_v = compute_open_circuit_voltage(soc, power_cell, pack_power) - (
            current_a * pack_power.resistance_ohm
        )

        self.assertGreater(power_v, energy_v)

    def test_summary_contains_runtime_and_warnings_fields(self) -> None:
        config = make_config("panasonic_ncr18650b")
        config = SimulationConfig(
            **{
                **config.__dict__,
                "discharge_current_a": 10.0,
                "duration_s": 1800,
            }
        )

        result = run_simulation(config)

        self.assertGreaterEqual(result.summary.runtime_s, 0)
        self.assertGreaterEqual(result.summary.delivered_energy_wh, 0.0)
        self.assertTrue(result.summary.termination_reason)
        self.assertIsInstance(result.summary.warnings, list)


if __name__ == "__main__":
    unittest.main()
