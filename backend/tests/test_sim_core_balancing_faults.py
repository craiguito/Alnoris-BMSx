from __future__ import annotations

import unittest

from backend.sim_core.engine import run_simulation
from backend.sim_core.types import BalancingConfig, FaultConfig, FaultSpec, GroupVariationConfig
from backend.tests.helpers import make_config


class BalancingAndFaultTests(unittest.TestCase):
    def test_balancing_disabled_preserves_behavior(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=60,
            discharge_current_a=4.0,
            group_variation=GroupVariationConfig(initial_soc_variation_abs=0.02),
        )
        baseline = run_simulation(config)
        disabled = run_simulation(
            make_config(
                cells_in_series=4,
                group_count=4,
                duration_s=60,
                discharge_current_a=4.0,
                group_variation=GroupVariationConfig(initial_soc_variation_abs=0.02),
                balancing=BalancingConfig(enabled=False, bleed_current_a=0.1),
            )
        )
        self.assertAlmostEqual(baseline.summary.final_soc_avg, disabled.summary.final_soc_avg, places=9)

    def test_passive_balancing_reduces_soc_spread(self) -> None:
        base = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=1200,
            discharge_current_a=-1.5,
            initial_soc=0.85,
            group_variation=GroupVariationConfig(initial_soc_variation_abs=0.06),
        )
        balanced = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=1200,
            discharge_current_a=-1.5,
            initial_soc=0.85,
            group_variation=GroupVariationConfig(initial_soc_variation_abs=0.06),
            balancing=BalancingConfig(
                enabled=True,
                soc_threshold=0.84,
                bleed_current_a=0.4,
                max_active_groups=2,
            ),
        )

        base_result = run_simulation(base)
        balanced_result = run_simulation(balanced)
        self.assertLess(balanced_result.summary.soc_spread, base_result.summary.soc_spread)
        self.assertTrue(balanced_result.summary.balancing_used)

    def test_high_resistance_fault_causes_lower_group_voltage(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=10,
            discharge_current_a=5.0,
            faults=FaultConfig(
                faults=(FaultSpec("high_resistance_group", group_index=2, factor=2.0),)
            ),
        )
        result = run_simulation(config)
        point = result.time_series[0]
        self.assertLess(point.group_voltage[2], point.group_voltage[0])

    def test_low_capacity_fault_drops_soc_faster(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=300,
            discharge_current_a=4.0,
            faults=FaultConfig(
                faults=(FaultSpec("low_capacity_group", group_index=1, factor=0.6),)
            ),
        )
        result = run_simulation(config)
        point = result.time_series[-1]
        self.assertLess(point.group_soc[1], point.group_soc[0])

    def test_cooling_loss_fault_raises_group_temperature(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=300,
            discharge_current_a=5.0,
            faults=FaultConfig(
                faults=(FaultSpec("cooling_loss_group", group_index=3, factor=0.2),)
            ),
        )
        result = run_simulation(config)
        point = result.time_series[-1]
        self.assertGreater(point.group_temp[3], point.group_temp[0])

    def test_time_windowed_fault_only_activates_during_window(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=40,
            discharge_current_a=5.0,
            faults=FaultConfig(
                faults=(FaultSpec("high_resistance_group", group_index=1, factor=2.0, start_time_s=10, end_time_s=20),)
            ),
        )
        result = run_simulation(config)
        self.assertNotIn(1, result.time_series[0].fault_active_groups)
        self.assertIn(1, result.time_series[10].fault_active_groups)
        self.assertNotIn(1, result.time_series[-1].fault_active_groups)

    def test_invalid_fault_index_raises(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            faults=FaultConfig(
                faults=(FaultSpec("high_resistance_group", group_index=8, factor=2.0),)
            ),
        )
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_invalid_balancing_config_raises(self) -> None:
        config = make_config(
            balancing=BalancingConfig(enabled=True, bleed_current_a=0.0, soc_threshold=0.9),
        )
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_outputs_include_balancing_and_fault_fields(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=20,
            discharge_current_a=4.0,
            balancing=BalancingConfig(enabled=True, soc_threshold=0.95, bleed_current_a=0.2),
            faults=FaultConfig(
                faults=(FaultSpec("high_resistance_group", group_index=0, factor=1.5),)
            ),
        )
        result = run_simulation(config)
        point = result.time_series[0]
        self.assertIn("balancing_active_groups", point.__dict__)
        self.assertIn("group_balance_current_a", point.__dict__)
        self.assertIn("group_fault_flags", point.__dict__)
        self.assertIn("balancing_used", result.summary.__dict__)
        self.assertIn("fault_count", result.summary.__dict__)

    def test_balancing_plus_fault_run_does_not_crash(self) -> None:
        config = make_config(
            cells_in_series=6,
            group_count=6,
            cells_in_parallel=2,
            duration_s=120,
            discharge_current_a=4.0,
            balancing=BalancingConfig(enabled=True, soc_threshold=0.92, bleed_current_a=0.25, max_active_groups=2),
            faults=FaultConfig(
                faults=(FaultSpec("elevated_self_heating_group", group_index=4, factor=1.8),)
            ),
            group_variation=GroupVariationConfig(initial_soc_variation_abs=0.03, resistance_variation_fraction=0.04),
        )
        result = run_simulation(config)
        self.assertGreater(len(result.time_series), 0)
        self.assertEqual(result.summary.fault_count, 1)


if __name__ == "__main__":
    unittest.main()
