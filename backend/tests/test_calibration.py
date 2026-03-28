from __future__ import annotations

import unittest

from backend.sim_core.engine import run_simulation
from backend.sim_core.physics.electrical import compute_open_circuit_voltage
from backend.sim_core.physics.pack import derive_pack_properties
from backend.sim_core.reference_cells import REFERENCE_CELLS

from backend.tests.helpers import make_config


class CalibrationTests(unittest.TestCase):
    def test_open_circuit_curve_matches_full_nominal_and_empty_metrics(self) -> None:
        for key, cell in REFERENCE_CELLS.items():
            config = make_config(key)

            full_v = compute_open_circuit_voltage(1.0, config)
            nominal_v = compute_open_circuit_voltage(0.5, config)
            empty_v = compute_open_circuit_voltage(0.0, config)

            self.assertAlmostEqual(full_v, cell.cell_full_voltage, places=6, msg=cell.name)
            self.assertAlmostEqual(nominal_v, cell.cell_nominal_voltage, places=6, msg=cell.name)
            self.assertAlmostEqual(empty_v, cell.cell_empty_voltage, places=6, msg=cell.name)

    def test_one_c_discharge_runs_close_to_expected_capacity_window(self) -> None:
        for key, cell in REFERENCE_CELLS.items():
            config = make_config(
                key,
                discharge_current_a=cell.cell_capacity_ah,
                duration_s=5400,
            )
            result = run_simulation(config)
            elapsed_s = result.time_series[-1].time_s
            delivered_ah = cell.cell_capacity_ah * elapsed_s / 3600.0

            self.assertGreater(delivered_ah, 0.70 * cell.cell_capacity_ah, cell.name)
            self.assertLess(delivered_ah, 1.10 * cell.cell_capacity_ah, cell.name)

    def test_higher_power_cell_sags_less_than_energy_cell_at_same_current(self) -> None:
        energy_cell = make_config("panasonic_ncr18650b")
        power_cell = make_config("samsung_30q")
        energy_pack = derive_pack_properties(energy_cell)
        power_pack = derive_pack_properties(power_cell)

        current_a = 5.0
        soc = 0.8
        energy_v = compute_open_circuit_voltage(soc, energy_cell, energy_pack.series_factor) - (
            current_a * energy_pack.group_base_resistance_ohm
        )
        power_v = compute_open_circuit_voltage(soc, power_cell, power_pack.series_factor) - (
            current_a * power_pack.group_base_resistance_ohm
        )

        self.assertGreater(power_v, energy_v)

    def test_summary_contains_backward_compatible_and_new_fields(self) -> None:
        config = make_config(
            "panasonic_ncr18650b",
            discharge_current_a=10.0,
            duration_s=1800,
        )
        result = run_simulation(config)

        self.assertGreaterEqual(result.summary.runtime_s, 0)
        self.assertGreaterEqual(result.summary.delivered_energy_wh, 0.0)
        self.assertTrue(result.summary.termination_reason)
        self.assertIsInstance(result.summary.warnings, list)
        self.assertEqual(result.summary.electrical_model_type, "rint")
        self.assertGreaterEqual(result.summary.estimated_capacity_retention, 0.0)
        self.assertGreaterEqual(result.summary.final_soc_avg, 0.0)
        self.assertGreaterEqual(result.time_series[-1].pack_voltage_v, 0.0)
        self.assertIn("current_a", result.time_series[-1].__dict__)


if __name__ == "__main__":
    unittest.main()
