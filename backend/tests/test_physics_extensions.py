from __future__ import annotations

import unittest

from backend.sim_core.engine import run_simulation
from backend.sim_core.types import CurrentProfile, CurrentProfilePoint, PhysicsConfig
from backend.tests.helpers import make_config


class PhysicsExtensionTests(unittest.TestCase):
    def test_charge_efficiency_reduces_soc_gain(self) -> None:
        ideal = run_simulation(
            make_config(
                duration_s=300,
                initial_soc=0.3,
                current_profile=CurrentProfile(points=(CurrentProfilePoint(0, -2.0),)),
                physics=PhysicsConfig(charge_efficiency=1.0),
            )
        )
        lossy = run_simulation(
            make_config(
                duration_s=300,
                initial_soc=0.3,
                current_profile=CurrentProfile(points=(CurrentProfilePoint(0, -2.0),)),
                physics=PhysicsConfig(charge_efficiency=0.9),
            )
        )
        self.assertGreater(ideal.summary.final_soc_avg, lossy.summary.final_soc_avg)

    def test_temperature_dependent_resistance_increases_heat(self) -> None:
        cool = run_simulation(make_config(duration_s=60, ambient_temp_c=10.0, discharge_current_a=4.0, physics=PhysicsConfig(resistance_temperature_alpha_per_c=0.02)))
        hot = run_simulation(make_config(duration_s=60, ambient_temp_c=45.0, discharge_current_a=4.0, physics=PhysicsConfig(resistance_temperature_alpha_per_c=0.02)))
        self.assertGreater(hot.time_series[0].pack_heat_w, cool.time_series[0].pack_heat_w)

    def test_low_temperature_derates_capacity(self) -> None:
        warm = run_simulation(make_config(duration_s=1200, ambient_temp_c=25.0, discharge_current_a=2.0, physics=PhysicsConfig(capacity_cold_derate_per_c=0.01)))
        cold = run_simulation(make_config(duration_s=1200, ambient_temp_c=-10.0, discharge_current_a=2.0, physics=PhysicsConfig(capacity_cold_derate_per_c=0.01)))
        self.assertLess(cold.summary.final_soc_avg, warm.summary.final_soc_avg)

    def test_self_discharge_reduces_soc_during_rest(self) -> None:
        result = run_simulation(make_config(duration_s=86400, discharge_current_a=0.0, physics=PhysicsConfig(self_discharge_per_day=0.01)))
        self.assertLess(result.summary.final_soc_avg, 1.0)

    def test_neighbor_thermal_coupling_limits_temp_spread(self) -> None:
        uncoupled = run_simulation(make_config(cells_in_series=4, group_count=4, duration_s=600, discharge_current_a=6.0, ambient_temp_c=25.0))
        coupled = run_simulation(make_config(cells_in_series=4, group_count=4, duration_s=600, discharge_current_a=6.0, ambient_temp_c=25.0, physics=PhysicsConfig(neighbor_thermal_coupling_w_per_k=0.5)))
        uncoupled_spread = uncoupled.time_series[-1].pack_temp_max_c - uncoupled.time_series[-1].pack_temp_avg_c
        coupled_spread = coupled.time_series[-1].pack_temp_max_c - coupled.time_series[-1].pack_temp_avg_c
        self.assertLessEqual(coupled_spread, uncoupled_spread + 1e-6)


if __name__ == "__main__":
    unittest.main()
