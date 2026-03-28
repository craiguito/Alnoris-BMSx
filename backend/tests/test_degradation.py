from __future__ import annotations

import unittest

from backend.sim_core.bridge import run_simulation_from_dict
from backend.sim_core.engine import run_simulation
from backend.sim_core.types import CurrentProfile, CurrentProfilePoint, DegradationConfig
from backend.tests.helpers import make_config


class DegradationTests(unittest.TestCase):
    def test_calendar_aging_occurs_at_idle_like_conditions(self) -> None:
        config = make_config(
            duration_s=48 * 3600,
            discharge_current_a=0.0,
            initial_soc=0.7,
        )
        result = run_simulation(config)
        self.assertLess(result.summary.capacity_retention, 1.0)

    def test_higher_temperature_accelerates_degradation(self) -> None:
        cool = run_simulation(make_config(duration_s=24 * 3600, discharge_current_a=3.0, ambient_temp_c=25.0))
        hot = run_simulation(make_config(duration_s=24 * 3600, discharge_current_a=3.0, ambient_temp_c=45.0))
        self.assertLess(hot.summary.capacity_retention, cool.summary.capacity_retention)

    def test_high_soc_storage_accelerates_calendar_fade(self) -> None:
        medium_soc = run_simulation(make_config(duration_s=72 * 3600, discharge_current_a=0.0, initial_soc=0.55))
        high_soc = run_simulation(make_config(duration_s=72 * 3600, discharge_current_a=0.0, initial_soc=0.95))
        self.assertLess(high_soc.summary.capacity_retention, medium_soc.summary.capacity_retention)

    def test_deeper_dod_usage_produces_more_degradation_than_shallow_cycling(self) -> None:
        shallow = run_simulation(
            make_config(
                duration_s=3600,
                current_profile=CurrentProfile(points=(
                    CurrentProfilePoint(0, 1.5),
                    CurrentProfilePoint(900, -1.5),
                    CurrentProfilePoint(1800, 1.5),
                    CurrentProfilePoint(2700, -1.5),
                )),
                initial_soc=0.55,
            )
        )
        deep = run_simulation(
            make_config(
                duration_s=3600,
                current_profile=CurrentProfile(points=(
                    CurrentProfilePoint(0, 4.0),
                    CurrentProfilePoint(900, -4.0),
                    CurrentProfilePoint(1800, 4.0),
                    CurrentProfilePoint(2700, -4.0),
                )),
                initial_soc=0.55,
            )
        )
        self.assertLess(deep.summary.capacity_retention, shallow.summary.capacity_retention)
        self.assertGreater(deep.summary.estimated_cycle_stress, shallow.summary.estimated_cycle_stress)

    def test_charging_at_high_soc_increases_degradation_stress(self) -> None:
        low_soc_charge = run_simulation(
            make_config(duration_s=120, initial_soc=0.45, current_profile=CurrentProfile(points=(CurrentProfilePoint(0, -3.0),)))
        )
        high_soc_charge = run_simulation(
            make_config(duration_s=120, initial_soc=0.92, current_profile=CurrentProfile(points=(CurrentProfilePoint(0, -3.0),)))
        )
        self.assertLess(high_soc_charge.summary.capacity_retention, low_soc_charge.summary.capacity_retention)

    def test_capacity_retention_decreases_over_time(self) -> None:
        result = run_simulation(make_config(duration_s=20000, discharge_current_a=6.0))
        self.assertLess(result.summary.capacity_retention, 1.0)
        self.assertLess(result.time_series[-1].estimated_capacity_retention, 1.0)

    def test_resistance_growth_increases_over_time(self) -> None:
        result = run_simulation(make_config(duration_s=20000, discharge_current_a=6.0))
        self.assertGreater(result.summary.resistance_growth, 0.0)
        self.assertGreater(result.time_series[-1].estimated_resistance_multiplier, 1.0)

    def test_long_run_remains_numerically_stable(self) -> None:
        result = run_simulation(make_config(duration_s=7 * 24 * 3600, discharge_current_a=1.0, initial_soc=0.8))
        self.assertGreaterEqual(result.summary.capacity_retention, 0.0)
        self.assertGreaterEqual(result.summary.resistance_growth, 0.0)
        self.assertLessEqual(result.summary.capacity_retention, 1.0)

    def test_outputs_remain_serializable(self) -> None:
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
            "discharge_current_a": 2.0,
            "duration_s": 3600,
            "time_step_s": 1,
            "initial_soc": 0.85,
            "pack_mass_kg": 1.0,
            "pack_heat_capacity_j_per_kgk": 900.0,
            "cooling_coeff_w_per_k": 1.0,
            "degradation": {
                "calendar_capacity_fade_per_hour": 5.0e-7,
                "high_soc_threshold": 0.8,
            },
        }
        result = run_simulation_from_dict(payload)
        self.assertIn("degradation_model_version", result["summary"])
        self.assertIn("group_capacity_retention", result["summary"])
        self.assertIn("estimated_capacity_retention", result["time_series"][-1])


if __name__ == "__main__":
    unittest.main()
