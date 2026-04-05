from __future__ import annotations

import math
import unittest

from backend.sim_core.bridge import run_simulation_from_dict, simulation_config_from_dict, virtual_test_catalog_to_dict
from backend.sim_core.engine import run_simulation
from backend.sim_core.profiles import current_for_time
from backend.sim_core.types import BalancingConfig, FaultConfig, FaultSpec, PhysicsConfig
from backend.tests.helpers import make_config, make_profile


class EngineRegressionTests(unittest.TestCase):
    def _payload(self, **overrides: object) -> dict[str, object]:
        config = make_config(cells_in_series=4, group_count=4, **overrides)
        return {
            "cell_nominal_voltage": config.cell_nominal_voltage,
            "cell_full_voltage": config.cell_full_voltage,
            "cell_empty_voltage": config.cell_empty_voltage,
            "cell_cutoff_voltage": config.cell_cutoff_voltage,
            "cell_capacity_ah": config.cell_capacity_ah,
            "cells_in_series": config.cells_in_series,
            "cells_in_parallel": config.cells_in_parallel,
            "internal_resistance_ohm_per_cell": config.internal_resistance_ohm_per_cell,
            "ambient_temp_c": config.ambient_temp_c,
            "discharge_current_a": config.discharge_current_a,
            "duration_s": config.duration_s,
            "time_step_s": config.time_step_s,
            "initial_soc": config.initial_soc,
            "pack_mass_kg": config.pack_mass_kg,
            "pack_heat_capacity_j_per_kgk": config.pack_heat_capacity_j_per_kgk,
            "cooling_coeff_w_per_k": config.cooling_coeff_w_per_k,
            "group_count": config.group_count,
        }

    def test_time_axis_starts_at_true_initial_state_and_ends_at_duration(self) -> None:
        duration_s = 95
        for time_step_s in (1, 10, 20):
            with self.subTest(time_step_s=time_step_s):
                result = run_simulation(
                    make_config(
                        duration_s=duration_s,
                        time_step_s=time_step_s,
                        discharge_current_a=3.0,
                        initial_soc=1.0,
                    )
                )
                self.assertEqual(result.time_series[0].time_s, 0)
                self.assertAlmostEqual(result.time_series[0].soc_avg, 1.0, places=9)
                self.assertAlmostEqual(result.time_series[0].group_true_soc[0], 1.0, places=9)
                self.assertEqual(result.time_series[-1].time_s, duration_s)
                self.assertEqual(result.summary.runtime_s, result.time_series[-1].time_s)
                self.assertLessEqual(result.summary.runtime_s, duration_s)
                self.assertAlmostEqual(result.summary.delivered_capacity_ah, 3.0 * duration_s / 3600.0, places=9)

    def test_total_balance_throughput_uses_actual_elapsed_time(self) -> None:
        result = run_simulation(
            make_config(
                cells_in_series=1,
                group_count=1,
                duration_s=95,
                time_step_s=20,
                discharge_current_a=0.0,
                initial_soc=0.95,
                balancing=BalancingConfig(enabled=True, soc_threshold=0.9, bleed_current_a=0.2),
            )
        )
        self.assertEqual(result.time_series[-1].time_s, 95)
        self.assertAlmostEqual(result.summary.total_balance_ah, 0.2 * 95.0 / 3600.0, places=9)

    def test_core_overheat_still_emits_warning_when_surface_is_cool(self) -> None:
        result = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=60,
                discharge_current_a=8.0,
                cooling_coeff_w_per_k=8.0,
                physics=PhysicsConfig(
                    two_node_thermal_enabled=True,
                    core_surface_thermal_coupling_w_per_k=0.15,
                    core_thermal_mass_j_per_k=12.0,
                    surface_thermal_mass_j_per_k=180.0,
                ),
                faults=FaultConfig(
                    faults=(FaultSpec("elevated_self_heating_group", group_index=0, factor=18.0),)
                ),
            )
        )
        warning_codes = {warning.code for warning in result.summary.warnings}
        self.assertGreater(result.summary.max_core_temp_c, 60.0)
        self.assertLess(result.summary.max_surface_temp_c, 45.0)
        self.assertTrue({"thermal_limit_exceeded", "thermal_margin_low"} & warning_codes)

    def test_delayed_stuck_high_soc_fault_changes_reported_soc_and_balancing(self) -> None:
        no_balance = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=10,
                discharge_current_a=0.0,
                initial_soc=0.6,
                faults=FaultConfig(
                    faults=(FaultSpec("stuck_high_soc_group", group_index=1, factor=0.3, start_time_s=5),)
                ),
            )
        )
        self.assertAlmostEqual(no_balance.time_series[0].group_soc[1], 0.6, places=9)
        self.assertAlmostEqual(no_balance.time_series[5].group_soc[1], 0.9, places=9)
        self.assertAlmostEqual(no_balance.time_series[5].group_true_soc[1], 0.6, places=9)
        self.assertAlmostEqual(
            no_balance.time_series[6].group_soc[1] - no_balance.time_series[6].group_true_soc[1],
            0.3,
            places=9,
        )
        self.assertAlmostEqual(no_balance.summary.final_soc_avg, 0.6, places=9)

        with_balance = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=10,
                discharge_current_a=0.0,
                initial_soc=0.6,
                faults=FaultConfig(
                    faults=(FaultSpec("stuck_high_soc_group", group_index=1, factor=0.3, start_time_s=5),)
                ),
                balancing=BalancingConfig(enabled=True, soc_threshold=0.85, bleed_current_a=0.2),
            )
        )
        self.assertIn(1, with_balance.time_series[5].balancing_active_groups)
        self.assertGreater(with_balance.time_series[6].group_soc[1], with_balance.time_series[6].group_true_soc[1])
        self.assertGreater(with_balance.summary.total_balance_ah, 0.0)

    def test_pack_level_summary_metrics_are_not_group_summed(self) -> None:
        throughput = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=1800,
                discharge_current_a=4.0,
                initial_soc=1.0,
            )
        )
        self.assertAlmostEqual(throughput.summary.cumulative_discharge_throughput_ah, 2.0, places=9)
        self.assertAlmostEqual(throughput.summary.cumulative_charge_throughput_ah, 0.0, places=9)

        high_soc = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=3600,
                discharge_current_a=0.0,
                initial_soc=0.9,
            )
        )
        self.assertAlmostEqual(high_soc.summary.cumulative_high_soc_time_h, 1.0, places=9)

    def test_low_usable_energy_warning_is_gated_to_real_utilization_attempts(self) -> None:
        short_run = run_simulation(make_config(duration_s=10, discharge_current_a=5.0, initial_soc=1.0))
        self.assertNotIn("low_usable_energy", {warning.code for warning in short_run.summary.warnings})

        underperforming = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=7200,
                discharge_current_a=8.0,
                initial_soc=1.0,
                faults=FaultConfig(
                    faults=(FaultSpec("low_capacity_group", group_index=0, factor=0.25),)
                ),
            )
        )
        self.assertIn("low_usable_energy", {warning.code for warning in underperforming.summary.warnings})

    def test_current_profile_lookup_preserves_boundary_behavior(self) -> None:
        profile = make_profile((0, 1.0), (5, 4.0), (10, 2.0))
        self.assertEqual(current_for_time(profile, 9.0, 0), 1.0)
        self.assertEqual(current_for_time(profile, 9.0, 4), 1.0)
        self.assertEqual(current_for_time(profile, 9.0, 5), 4.0)
        self.assertEqual(current_for_time(profile, 9.0, 9), 4.0)
        self.assertEqual(current_for_time(profile, 9.0, 10), 2.0)
        self.assertEqual(current_for_time(profile, 9.0, 100), 2.0)

    def test_bridge_can_omit_or_downsample_time_series(self) -> None:
        payload = self._payload(duration_s=20, time_step_s=1)

        without_trace = run_simulation_from_dict({**payload, "include_time_series": False})
        self.assertNotIn("time_series", without_trace)
        self.assertEqual(without_trace["time_series_metadata"]["original_point_count"], 21)
        self.assertEqual(without_trace["time_series_metadata"]["returned_point_count"], 0)

        downsampled = run_simulation_from_dict({**payload, "max_time_series_points": 5})
        returned_times = [point["time_s"] for point in downsampled["time_series"]]
        self.assertEqual(returned_times[0], 0)
        self.assertEqual(returned_times[-1], 20)
        self.assertEqual(returned_times, sorted(returned_times))
        self.assertEqual(len(downsampled["time_series"]), 5)
        self.assertEqual(downsampled["time_series_metadata"]["original_point_count"], 21)
        self.assertEqual(downsampled["time_series_metadata"]["returned_point_count"], 5)

    def test_time_step_validation_accepts_integral_floats_and_rejects_fractional_seconds(self) -> None:
        config = simulation_config_from_dict({**self._payload(), "time_step_s": 1.0})
        self.assertEqual(config.time_step_s, 1)

        with self.assertRaisesRegex(ValueError, "integer number of seconds"):
            simulation_config_from_dict({**self._payload(), "time_step_s": 0.1})

    def test_virtual_test_catalog_advertises_integer_timesteps(self) -> None:
        catalog = virtual_test_catalog_to_dict()
        for definition in catalog["tests"]:
            time_step_param = next((param for param in definition["parameters"] if param["key"] == "time_step_s"), None)
            if time_step_param is None:
                continue
            self.assertEqual(time_step_param["param_type"], "int")
            self.assertGreaterEqual(time_step_param["min_value"], 1)

    def test_coarse_thermal_timestep_warns_but_stays_finite(self) -> None:
        result = run_simulation(
            make_config(
                duration_s=240,
                time_step_s=60,
                discharge_current_a=8.0,
                cooling_coeff_w_per_k=10.0,
                physics=PhysicsConfig(
                    two_node_thermal_enabled=True,
                    core_surface_thermal_coupling_w_per_k=6.0,
                    core_thermal_mass_j_per_k=12.0,
                    surface_thermal_mass_j_per_k=18.0,
                ),
            )
        )
        self.assertIn("thermal_timestep_coarse", {warning.code for warning in result.summary.warnings})
        self.assertTrue(math.isfinite(result.summary.max_core_temp_c))
        self.assertTrue(math.isfinite(result.summary.max_surface_temp_c))
        self.assertLess(result.summary.max_core_temp_c, 200.0)


if __name__ == "__main__":
    unittest.main()
