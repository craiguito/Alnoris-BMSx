from __future__ import annotations

import unittest

from backend.sim_core.engine import run_simulation

from backend.tests.helpers import make_2rc_model, make_config


class DegradationTests(unittest.TestCase):
    def test_degradation_reduces_effective_capacity(self) -> None:
        config = make_config(
            duration_s=20000,
            discharge_current_a=6.0,
        )
        result = run_simulation(config)

        self.assertLess(result.summary.estimated_capacity_retention, 1.0)
        self.assertLess(result.summary.capacity_retention, 1.0)

    def test_degradation_increases_effective_resistance(self) -> None:
        config = make_config(
            duration_s=20000,
            discharge_current_a=6.0,
            electrical_model=make_2rc_model(
                r0_ohm_per_cell=0.03,
                branch1_resistance_ohm=0.01,
                branch1_capacitance_f=2200.0,
                branch2_resistance_ohm=0.02,
                branch2_capacitance_f=14000.0,
            ),
        )
        result = run_simulation(config)

        self.assertGreater(result.summary.estimated_resistance_growth, 0.0)
        self.assertGreater(result.summary.resistance_growth, 0.0)


if __name__ == "__main__":
    unittest.main()
