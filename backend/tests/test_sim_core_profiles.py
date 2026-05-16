from __future__ import annotations

import os
import tempfile
import math
import unittest

from backend.sim_core.bridge import run_simulation_from_dict
from backend.sim_core.engine import run_simulation
from backend.sim_core.types import CurrentProfile

from backend.tests.helpers import make_1rc_model, make_2rc_model, make_config, make_profile


class ProfileAndElectricalModelTests(unittest.TestCase):
    def test_rint_model_remains_similar_to_simple_behavior(self) -> None:
        config = make_config(discharge_current_a=3.0, duration_s=10)
        result = run_simulation(config)
        first_point = result.time_series[0]
        self.assertAlmostEqual(first_point.pack_voltage_v, first_point.terminal_voltage_v, places=9)
        self.assertAlmostEqual(first_point.pack_power_w, first_point.power_w, places=9)

    def test_1rc_model_shows_transient_sag_beyond_rint(self) -> None:
        base = make_config(discharge_current_a=5.0, duration_s=30, initial_soc=0.8)
        rc = make_config(
            discharge_current_a=5.0,
            duration_s=30,
            initial_soc=0.8,
            electrical_model=make_1rc_model(
                r0_ohm_per_cell=base.internal_resistance_ohm_per_cell,
                branch_resistance_ohm=0.015,
                capacitance_f=2400.0,
            ),
        )

        rint_result = run_simulation(base)
        rc_result = run_simulation(rc)

        self.assertLess(rc_result.time_series[5].pack_voltage_v, rint_result.time_series[5].pack_voltage_v)

    def test_2rc_model_is_numerically_stable(self) -> None:
        config = make_config(
            discharge_current_a=4.0,
            duration_s=120,
            electrical_model=make_2rc_model(
                r0_ohm_per_cell=0.03,
                branch1_resistance_ohm=0.01,
                branch1_capacitance_f=1800.0,
                branch2_resistance_ohm=0.02,
                branch2_capacitance_f=12000.0,
            ),
        )
        result = run_simulation(config)

        for point in result.time_series:
            self.assertTrue(math.isfinite(point.pack_voltage_v))
            self.assertTrue(math.isfinite(point.pack_heat_w))
            self.assertGreaterEqual(point.pack_voltage_v, 0.0)

    def test_current_profile_changes_current_over_time(self) -> None:
        config = make_config(
            discharge_current_a=1.0,
            duration_s=12,
            current_profile=make_profile((0, 1.0), (5, 4.0), (10, 2.0)),
        )
        result = run_simulation(config)

        self.assertEqual(result.time_series[0].current_a, 1.0)
        self.assertEqual(result.time_series[5].current_a, 4.0)
        self.assertEqual(result.time_series[10].current_a, 2.0)

    def test_current_profile_can_load_from_csv(self) -> None:
        with tempfile.NamedTemporaryFile("w", delete=False, suffix=".csv", encoding="utf-8") as handle:
            handle.write("time_s,current_a\n0,1.0\n5,4.0\n10,2.0\n")
            csv_path = handle.name
        try:
            profile = CurrentProfile.from_csv(csv_path)
            config = make_config(discharge_current_a=9.0, duration_s=12, current_profile=profile)
            result = run_simulation(config)

            self.assertEqual(result.time_series[0].current_a, 1.0)
            self.assertEqual(result.time_series[5].current_a, 4.0)
            self.assertEqual(result.time_series[10].current_a, 2.0)
        finally:
            os.unlink(csv_path)

    def test_negative_current_increases_soc(self) -> None:
        config = make_config(
            discharge_current_a=2.0,
            duration_s=20,
            initial_soc=0.4,
            current_profile=make_profile((0, -3.0)),
        )
        result = run_simulation(config)

        self.assertGreater(result.time_series[-1].soc_avg, 0.4)

    def test_result_remains_serializable_through_bridge(self) -> None:
        payload = {
            "cell_nominal_voltage": 3.6,
            "cell_full_voltage": 4.2,
            "cell_empty_voltage": 3.0,
            "cell_cutoff_voltage": 3.0,
            "cell_capacity_ah": 3.35,
            "cells_in_series": 4,
            "cells_in_parallel": 2,
            "internal_resistance_ohm_per_cell": 0.035,
            "ambient_temp_c": 25.0,
            "discharge_current_a": 5.0,
            "duration_s": 15,
            "time_step_s": 1,
            "initial_soc": 0.9,
            "pack_mass_kg": 1.2,
            "pack_heat_capacity_j_per_kgk": 900.0,
            "cooling_coeff_w_per_k": 1.5,
            "electrical_model": {
                "type": "1rc",
                "r0_ohm_per_cell": 0.035,
                "rc_branches": [{"resistance_ohm": 0.012, "capacitance_f": 2600.0}],
            },
            "current_profile": [{"time_s": 0, "current_a": 5.0}, {"time_s": 8, "current_a": 3.0}],
            "group_count": 4,
        }

        result = run_simulation_from_dict(payload)
        self.assertEqual(result["summary"]["electrical_model_type"], "1rc")
        self.assertTrue(result["summary"]["profile_used"])
        self.assertIn("current_a", result["time_series"][0])


if __name__ == "__main__":
    unittest.main()
