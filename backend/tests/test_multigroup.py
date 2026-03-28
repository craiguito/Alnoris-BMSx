from __future__ import annotations

import unittest

from backend.sim_core.engine import run_simulation
from backend.sim_core.types import GroupVariationConfig

from backend.tests.helpers import make_config, make_profile


class MultiGroupSimulationTests(unittest.TestCase):
    def test_multigroup_simulation_creates_soc_spread_with_variation(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=180,
            discharge_current_a=4.0,
            group_variation=GroupVariationConfig(
                capacity_variation_fraction=0.04,
                resistance_variation_fraction=0.02,
                initial_soc_variation_abs=0.02,
            ),
        )
        result = run_simulation(config)

        self.assertGreater(result.summary.soc_spread, 0.0)
        self.assertGreater(result.time_series[-1].soc_max, result.time_series[-1].soc_min)

    def test_higher_resistance_group_sags_more(self) -> None:
        config = make_config(
            cells_in_series=5,
            group_count=5,
            duration_s=20,
            discharge_current_a=8.0,
            group_variation=GroupVariationConfig(
                resistance_variation_fraction=0.20,
            ),
        )
        result = run_simulation(config)

        self.assertEqual(result.time_series[0].weakest_group_index, 4)

    def test_profile_driven_multigroup_run_does_not_crash(self) -> None:
        config = make_config(
            cells_in_series=6,
            cells_in_parallel=2,
            group_count=6,
            duration_s=120,
            current_profile=make_profile((0, 3.0), (30, 8.0), (60, -2.0), (90, 4.0)),
            group_variation=GroupVariationConfig(
                capacity_variation_fraction=0.03,
                resistance_variation_fraction=0.05,
                initial_soc_variation_abs=0.01,
            ),
        )
        result = run_simulation(config)

        self.assertGreater(len(result.time_series), 1)
        self.assertTrue(result.summary.termination_reason)
        self.assertGreaterEqual(result.summary.max_group_temp_c, config.ambient_temp_c)


if __name__ == "__main__":
    unittest.main()
