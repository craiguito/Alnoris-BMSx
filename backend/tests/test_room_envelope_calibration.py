from __future__ import annotations

import json
import unittest
from pathlib import Path

from backend.sim_core.calibration_profiles import (
    CalibrationObjectiveWeights,
    get_calibration_profile,
    evaluate_calibration_objective,
)
from backend.sim_core.engine import run_simulation
from backend.sim_core.room_envelope_calibration import calibrate_room_envelope
from backend.tests.helpers import make_config, make_profile, temporary_workspace_dir, write_truth_dataset


def _registry_metadata(dataset_id: str, display_name: str, source_battery_id: str) -> dict[str, object]:
    return {
        "dataset_id": dataset_id,
        "display_name": display_name,
        "description": f"Synthetic room-envelope dataset for {display_name}.",
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": 3.7,
        "nominal_capacity_ah": 3.0,
        "temperature_range_c": [20.0, 35.0],
        "current_profile_type": "synthetic_pulse",
        "tags": ["synthetic", "room-envelope"],
        "source": "backend-tests",
        "status": "canonical",
        "created_at": "2026-04-22T00:00:00Z",
        "notes": "Synthetic dataset used for room-envelope calibration tests.",
        "ambient_temp_c": 25.0,
        "initial_soc": 1.0,
        "source_battery_id": source_battery_id,
        "source_cycle_index": 1,
    }


class RoomEnvelopeCalibrationTests(unittest.TestCase):
    def test_weighted_objective_prioritizes_voltage_over_temperature(self) -> None:
        scorecards = [
            {
                "metric_results": [
                    {"metric_id": "rmse_voltage", "value": 0.12, "threshold_value": 0.05},
                    {"metric_id": "final_voltage_error", "value": 0.16, "threshold_value": 0.08},
                    {"metric_id": "energy_error", "value": 0.04, "threshold_value": 0.08},
                    {"metric_id": "temp_rmse", "value": 0.5, "threshold_value": 2.5},
                ]
            }
        ]
        electrical_first = evaluate_calibration_objective(
            scorecards,
            weights=get_calibration_profile("electrical_first").objective_weights,
        )
        balanced = evaluate_calibration_objective(
            scorecards,
            weights=get_calibration_profile("balanced_electro_thermal").objective_weights,
        )

        self.assertGreater(electrical_first.total_score, balanced.total_score)
        self.assertIn("avg_metric_i / threshold_i", electrical_first.normalized_formula)

    def test_profile_selection_defaults_to_electrical_first(self) -> None:
        profile = get_calibration_profile()
        self.assertEqual(profile.profile_id, "electrical_first")
        self.assertIn(0, profile.rc_branch_count_options)
        self.assertTrue(profile.electro_blend_options)

    def test_room_envelope_calibration_emits_diagnostics_and_metadata(self) -> None:
        config = make_config(
            cells_in_series=1,
            cells_in_parallel=1,
            group_count=1,
            discharge_current_a=0.0,
            duration_s=45,
            current_profile=make_profile((0, 0.0), (5, 2.0), (20, 0.0), (30, 1.0)),
        )
        result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            truth_dir = Path(temp_dir) / "truth_data"
            manifest_dir = Path(temp_dir) / "manifests"
            truth_dir.mkdir(parents=True, exist_ok=True)
            manifest_dir.mkdir(parents=True, exist_ok=True)
            write_truth_dataset(
                truth_dir / "dataset_a.json",
                config,
                result,
                extra_metadata=_registry_metadata("synthetic_room_a", "Synthetic Room A", "B0005"),
            )
            write_truth_dataset(
                truth_dir / "dataset_b.json",
                config,
                result,
                extra_metadata=_registry_metadata("synthetic_room_b", "Synthetic Room B", "B0006"),
            )
            (manifest_dir / "synthetic_room.json").write_text(
                json.dumps(
                    {
                        "manifest_id": "synthetic_room",
                        "display_name": "Synthetic Room Envelope",
                        "description": "Synthetic manifest for room-envelope calibration tests.",
                        "dataset_ids": ["synthetic_room_a", "synthetic_room_b"],
                        "recommended_threshold_profile": "single_cell_v1_nasa",
                        "calibration_dataset_id": "synthetic_room_a",
                        "validation_basis_label": "Synthetic room basis",
                        "validation_basis_description": "Synthetic room basis used in tests.",
                        "validation_limits": [
                            "Validated at single-cell level only.",
                            "Intended for comparative trade-study support only."
                        ]
                    },
                    indent=2,
                ),
                encoding="utf-8",
            )

            artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )

        self.assertTrue(artifact.candidate_scores)
        self.assertEqual(len(artifact.per_dataset_diagnostics), 2)
        self.assertIn("calibration_profile_id", artifact.selected_validation["validation_summary"])
        self.assertIn("objective_weights", artifact.selected_validation["validation_summary"])
        self.assertIn("calibration_objective", artifact.selected_validation["validation_summary"])
        self.assertIn("Dataset", artifact.diagnostics_markdown)


if __name__ == "__main__":
    unittest.main()
