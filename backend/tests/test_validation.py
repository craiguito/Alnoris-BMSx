from __future__ import annotations

import os
import tempfile
import unittest

from backend.sim_core.bridge import simulation_config_from_dict
from backend.sim_core.engine import run_simulation
from backend.sim_core.physics.electrical import compute_heat_w
from backend.sim_core.types import DegradationConfig, PhysicsConfig
from backend.tests.helpers import make_1rc_model, make_config


class ValidationAndSafetyTests(unittest.TestCase):
    def test_invalid_duration_raises(self) -> None:
        config = make_config(duration_s=0)
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_non_divisible_group_count_raises(self) -> None:
        config = make_config(cells_in_series=5, group_count=4)
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_bridge_rejects_duplicate_profile_timestamps(self) -> None:
        payload = {
            "cell_nominal_voltage": 3.6,
            "cell_full_voltage": 4.2,
            "cell_empty_voltage": 3.0,
            "cell_cutoff_voltage": 3.0,
            "cell_capacity_ah": 3.35,
            "cells_in_series": 4,
            "cells_in_parallel": 1,
            "internal_resistance_ohm_per_cell": 0.035,
            "ambient_temp_c": 25.0,
            "discharge_current_a": 5.0,
            "duration_s": 10,
            "time_step_s": 1,
            "initial_soc": 0.8,
            "pack_mass_kg": 1.0,
            "pack_heat_capacity_j_per_kgk": 900.0,
            "cooling_coeff_w_per_k": 1.0,
            "current_profile": [
                {"time_s": 0, "current_a": 2.0},
                {"time_s": 0, "current_a": 3.0},
            ],
        }
        with self.assertRaises(ValueError):
            simulation_config_from_dict(payload)

    def test_csv_profile_validation_rejects_unsorted_times(self) -> None:
        with tempfile.NamedTemporaryFile("w", delete=False, suffix=".csv", encoding="utf-8") as handle:
            handle.write("time_s,current_a\n5,2.0\n0,1.0\n")
            path = handle.name
        try:
            payload = {
                "cell_nominal_voltage": 3.6,
                "cell_full_voltage": 4.2,
                "cell_empty_voltage": 3.0,
                "cell_cutoff_voltage": 3.0,
                "cell_capacity_ah": 3.35,
                "cells_in_series": 4,
                "cells_in_parallel": 1,
                "internal_resistance_ohm_per_cell": 0.035,
                "ambient_temp_c": 25.0,
                "discharge_current_a": 5.0,
                "duration_s": 10,
                "time_step_s": 1,
                "initial_soc": 0.8,
                "pack_mass_kg": 1.0,
                "pack_heat_capacity_j_per_kgk": 900.0,
                "cooling_coeff_w_per_k": 1.0,
                "current_profile": path,
            }
            with self.assertRaises(ValueError):
                simulation_config_from_dict(payload)
        finally:
            os.unlink(path)

    def test_heat_only_counts_ohmic_loss(self) -> None:
        current_a = 4.0
        config = make_config(
            discharge_current_a=current_a,
            electrical_model=make_1rc_model(
                r0_ohm_per_cell=0.03,
                branch_resistance_ohm=0.05,
                capacitance_f=3000.0,
            )
        )
        self.assertAlmostEqual(compute_heat_w(current_a, 0.03), current_a**2 * 0.03, places=9)
        result = run_simulation(config)
        self.assertAlmostEqual(result.time_series[0].pack_heat_w, current_a**2 * 0.03, places=4)

    def test_extreme_but_valid_config_does_not_crash(self) -> None:
        config = make_config(
            cells_in_series=8,
            group_count=8,
            cells_in_parallel=3,
            duration_s=30,
            discharge_current_a=12.0,
            cooling_coeff_w_per_k=8.0,
        )
        result = run_simulation(config)
        self.assertGreater(len(result.time_series), 0)

    def test_invalid_degradation_config_raises(self) -> None:
        config = make_config(
            degradation=DegradationConfig(
                calendar_capacity_fade_per_hour=-1.0,
            )
        )
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_invalid_physics_config_raises(self) -> None:
        config = make_config(
            physics=PhysicsConfig(discharge_efficiency=1.5),
        )
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_invalid_soc_resistance_curve_raises(self) -> None:
        config = make_config(
            physics=PhysicsConfig(
                resistance_vs_soc_enabled=True,
                resistance_soc_curve=(),
            ),
        )
        with self.assertRaises(ValueError):
            run_simulation(config)


if __name__ == "__main__":
    unittest.main()
