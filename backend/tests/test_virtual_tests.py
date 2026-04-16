from __future__ import annotations

import json
import os
import unittest
from pathlib import Path
from unittest.mock import patch

from backend.sim_core.bridge import run_virtual_test_from_dict, vet_virtual_test_from_dict, virtual_test_catalog_to_dict
from backend.sim_core.engine import run_simulation
from backend.tests.helpers import make_config, make_profile, temporary_workspace_dir, write_truth_dataset


def _registry_metadata(dataset_id: str, display_name: str, *, status: str = "canonical") -> dict[str, object]:
    return {
        "dataset_id": dataset_id,
        "display_name": display_name,
        "description": f"Synthetic validation dataset for {display_name}.",
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": 14.8,
        "nominal_capacity_ah": 3.2,
        "temperature_range_c": [20.0, 35.0],
        "current_profile_type": "synthetic_pulse",
        "tags": ["synthetic", "validation"],
        "source": "backend-tests",
        "status": status,
        "created_at": "2026-04-16T00:00:00Z",
        "notes": "Synthetic dataset used for model validation tests.",
    }


class VirtualTestFrameworkTests(unittest.TestCase):
    def _base_payload(self) -> dict[str, object]:
        config = make_config(cells_in_series=4, group_count=4)
        return {
            "chemistry_name": config.chemistry_name,
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
        self.assertEqual(
            test_ids,
            {"rate_capability", "thermal_stress", "thermal_zone_comparison", "model_validation"},
        )
        self.assertIn("rate_capability", test_ids)
        self.assertIn("model_validation", test_ids)
        experimental_ids = {item["test_id"] for item in catalog["experimental_tests"]}
        self.assertIn("pulse_power", experimental_ids)
        self.assertIn("constant_current_discharge", experimental_ids)

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

    def test_model_validation_replays_truth_dataset_and_reports_metrics(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            discharge_current_a=0.0,
            duration_s=60,
            current_profile=make_profile((0, 0.0), (5, 2.0), (20, 0.0), (35, 1.5), (50, 0.0)),
        )
        truth_result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            dataset_path = write_truth_dataset(f"{temp_dir}/truth.json", config, truth_result)
            payload = {
                "test_id": "model_validation",
                "base_config": self._base_payload(),
                "parameters": {
                    "dataset_path": str(dataset_path),
                    "metrics": ["rmse_voltage", "energy_error", "temp_rmse"],
                    "max_voltage_rmse_v": 0.01,
                    "max_energy_error_fraction": 0.01,
                    "max_temp_rmse_c": 0.01,
                },
            }
            result = run_virtual_test_from_dict(payload)

        self.assertIn("rmse_voltage_v", result["summary_metrics"])
        self.assertIn("energy_error_fraction", result["summary_metrics"])
        self.assertIn("rmse_temp_c", result["summary_metrics"])
        self.assertLess(result["summary_metrics"]["rmse_voltage_v"], 1e-6)
        self.assertLess(result["summary_metrics"]["energy_error_fraction"], 1e-6)
        self.assertLess(result["summary_metrics"]["rmse_temp_c"], 1e-6)
        self.assertTrue(result["pass_fail_indicators"]["validation_passed"])
        self.assertEqual(len(result["validation_scorecards"]), 1)
        self.assertEqual(result["validation_summary"]["overall_status"], "pass")

    def test_model_validation_supports_multiple_registered_truth_datasets(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            discharge_current_a=0.0,
            duration_s=60,
            current_profile=make_profile((0, 0.0), (5, 2.0), (20, 0.0), (35, 1.5), (50, 0.0)),
        )
        truth_result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            truth_dir = Path(temp_dir) / "truth_data"
            truth_dir.mkdir(parents=True, exist_ok=True)
            write_truth_dataset(
                truth_dir / "canonical.json",
                config,
                truth_result,
                extra_metadata=_registry_metadata("canonical_validation", "Canonical Validation", status="canonical"),
            )
            shifted_dataset_path = write_truth_dataset(
                truth_dir / "experimental.json",
                config,
                truth_result,
                extra_metadata=_registry_metadata("experimental_validation", "Experimental Validation", status="experimental"),
            )

            payload = json.loads(shifted_dataset_path.read_text(encoding="utf-8"))
            for row in payload["data"]:
                row["voltage_v"] = float(row["voltage_v"]) + 0.35
            shifted_dataset_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

            with patch.dict(os.environ, {"ALNORIS_TRUTH_DATA_DIR": str(truth_dir)}, clear=False):
                result = run_virtual_test_from_dict(
                    {
                        "test_id": "model_validation",
                        "base_config": self._base_payload(),
                        "parameters": {
                            "dataset_ids": ["canonical_validation", "experimental_validation"],
                            "metrics": ["rmse_voltage", "energy_error", "temp_rmse"],
                            "max_voltage_rmse_v": 0.02,
                            "max_abs_voltage_error_v": 0.05,
                            "max_energy_error_fraction": 0.02,
                            "max_temp_rmse_c": 0.02,
                            "max_final_soc_error": 0.02,
                            "max_final_voltage_error_v": 0.05,
                        },
                    }
                )

        self.assertEqual(len(result["validation_scorecards"]), 2)
        self.assertEqual(result["validation_summary"]["dataset_count"], 2)
        self.assertEqual(result["validation_summary"]["passed_count"], 1)
        self.assertEqual(result["validation_summary"]["failed_count"], 1)
        self.assertEqual(result["validation_summary"]["overall_status"], "warning")
        self.assertFalse(result["pass_fail_indicators"]["validation_passed"])
        self.assertIn("primary_result", result)


if __name__ == "__main__":
    unittest.main()
