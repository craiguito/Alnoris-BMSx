from __future__ import annotations

import unittest

from backend.sim_core.bridge import run_virtual_test_from_dict, vet_virtual_test_from_dict, virtual_test_catalog_to_dict
from backend.tests.helpers import make_config


class VirtualTestFrameworkTests(unittest.TestCase):
    def _base_payload(self) -> dict[str, object]:
        config = make_config(cells_in_series=4, group_count=4)
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

    def test_catalog_contains_expected_tests(self) -> None:
        catalog = virtual_test_catalog_to_dict()
        test_ids = {item["test_id"] for item in catalog["tests"]}
        self.assertIn("constant_current_discharge", test_ids)
        self.assertIn("pulse_power", test_ids)
        self.assertIn("rate_capability", test_ids)

    def test_vetting_rejects_missing_required_parameter(self) -> None:
        payload = {
            "test_id": "constant_current_discharge",
            "base_config": self._base_payload(),
            "parameters": {
                "current_a": "",
                "initial_soc": 1.0,
            },
        }
        result = vet_virtual_test_from_dict(payload)
        self.assertFalse(result["vetting_result"]["is_valid"])

    def test_vetting_warns_for_coarse_pulse_timestep(self) -> None:
        payload = {
            "test_id": "pulse_power",
            "base_config": self._base_payload(),
            "parameters": {
                "pulse_current_a": 10.0,
                "pulse_duration_s": 2.0,
                "rest_duration_s": 5.0,
                "pulse_count": 3,
                "initial_soc": 0.8,
                "ambient_temp_c": 25.0,
                "time_step_s": 2.0,
            },
        }
        result = vet_virtual_test_from_dict(payload)
        self.assertTrue(result["vetting_result"]["warnings"])

    def test_constant_discharge_test_runs(self) -> None:
        payload = {
            "test_id": "constant_current_discharge",
            "base_config": self._base_payload(),
            "parameters": {
                "current_a": 2.0,
                "initial_soc": 1.0,
                "ambient_temp_c": 25.0,
                "time_step_s": 1.0,
                "max_duration_s": 600.0,
            },
        }
        result = run_virtual_test_from_dict(payload)
        self.assertIn("runtime_s", result["summary_metrics"])
        self.assertIn("primary_result", result)

    def test_rate_capability_returns_multiple_subresults(self) -> None:
        payload = {
            "test_id": "rate_capability",
            "base_config": self._base_payload(),
            "parameters": {
                "current_list_a": [2.0, 4.0, 6.0],
                "initial_soc": 1.0,
                "ambient_temp_c": 25.0,
                "time_step_s": 1.0,
                "max_duration_s": 600.0,
            },
        }
        result = run_virtual_test_from_dict(payload)
        self.assertEqual(len(result["sub_results"]), 3)

    def test_storage_test_shows_soc_loss(self) -> None:
        payload = {
            "test_id": "storage_self_discharge",
            "base_config": self._base_payload(),
            "parameters": {
                "storage_duration_s": 86400.0 * 3.0,
                "initial_soc": 0.85,
                "ambient_temp_c": 30.0,
                "self_discharge_per_day": 0.01,
                "time_step_s": 3600.0,
            },
        }
        result = run_virtual_test_from_dict(payload)
        self.assertGreater(result["summary_metrics"]["soc_loss"], 0.0)

    def test_balancing_effectiveness_test_reduces_spread(self) -> None:
        payload = {
            "test_id": "balancing_effectiveness",
            "base_config": self._base_payload(),
            "parameters": {
                "initial_soc": 0.92,
                "initial_soc_spread": 0.08,
                "run_duration_s": 1200.0,
                "ambient_temp_c": 25.0,
                "bleed_current_a": 0.2,
                "soc_threshold": 0.9,
            },
        }
        result = run_virtual_test_from_dict(payload)
        self.assertIn("spread_reduced", result["pass_fail_indicators"])


if __name__ == "__main__":
    unittest.main()
