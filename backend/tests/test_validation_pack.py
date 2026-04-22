from __future__ import annotations

import json
import unittest
from pathlib import Path

from backend.sim_core.engine import run_simulation
from backend.sim_core.validation_pack import resolve_validation_manifest, run_validation_pack
from backend.sim_core.validation_threshold_profiles import get_validation_threshold_profile
from backend.tests.helpers import make_config, make_profile, temporary_workspace_dir, write_truth_dataset


def _registry_metadata(dataset_id: str, display_name: str) -> dict[str, object]:
    return {
        "dataset_id": dataset_id,
        "display_name": display_name,
        "description": f"Synthetic validation dataset for {display_name}.",
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": 3.7,
        "nominal_capacity_ah": 3.0,
        "temperature_range_c": [20.0, 35.0],
        "current_profile_type": "synthetic_pulse",
        "tags": ["synthetic", "validation-pack"],
        "source": "backend-tests",
        "status": "canonical",
        "created_at": "2026-04-22T00:00:00Z",
        "notes": "Synthetic dataset used for validation pack tests.",
        "ambient_temp_c": 25.0
    }


class ValidationPackTests(unittest.TestCase):
    def test_default_threshold_profile_contains_v1_metrics(self) -> None:
        profile = get_validation_threshold_profile("single_cell_v1_nasa")
        self.assertEqual(
            profile.metrics,
            ("rmse_voltage", "final_voltage_error", "energy_error", "temp_rmse"),
        )
        self.assertIn("max_voltage_rmse_v", profile.thresholds)
        self.assertIn("Validated at single-cell level only.", profile.validation_limits)

    def test_manifest_resolution_loads_curated_nasa_manifest(self) -> None:
        manifest = resolve_validation_manifest("nasa_room_canonical")
        self.assertEqual(manifest.calibration_dataset_id, "nasa_ames_b0005_discharge_0001")
        self.assertEqual(len(manifest.dataset_ids), 12)
        self.assertIn("comparative trade-study support only", " ".join(manifest.validation_limits).lower())

    def test_run_validation_pack_emits_default_and_calibrated_artifacts(self) -> None:
        config = make_config(
            cells_in_series=1,
            cells_in_parallel=1,
            group_count=1,
            discharge_current_a=0.0,
            duration_s=60,
            current_profile=make_profile((0, 0.0), (5, 2.0), (20, 0.0), (35, 1.5), (50, 0.0)),
        )
        truth_result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            truth_dir = Path(temp_dir) / "truth_data"
            manifest_dir = Path(temp_dir) / "manifests"
            truth_dir.mkdir(parents=True, exist_ok=True)
            manifest_dir.mkdir(parents=True, exist_ok=True)
            write_truth_dataset(
                truth_dir / "seed_a.json",
                config,
                truth_result,
                extra_metadata=_registry_metadata("synthetic_pack_a", "Synthetic Pack A"),
            )
            write_truth_dataset(
                truth_dir / "seed_b.json",
                config,
                truth_result,
                extra_metadata=_registry_metadata("synthetic_pack_b", "Synthetic Pack B"),
            )
            (manifest_dir / "synthetic_pack.json").write_text(
                json.dumps(
                    {
                        "manifest_id": "synthetic_pack",
                        "display_name": "Synthetic Validation Pack",
                        "description": "Synthetic manifest for backend validation-pack tests.",
                        "dataset_ids": ["synthetic_pack_a", "synthetic_pack_b"],
                        "recommended_threshold_profile": "single_cell_v1_nasa",
                        "calibration_dataset_id": "synthetic_pack_a",
                        "validation_basis_label": "Synthetic single-cell basis",
                        "validation_basis_description": "Synthetic backend test basis.",
                        "validation_limits": [
                            "Validated at single-cell level only.",
                            "Intended for comparative trade-study support only."
                        ]
                    },
                    indent=2,
                ),
                encoding="utf-8",
            )

            artifact = run_validation_pack(
                "synthetic_pack",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )

        self.assertEqual(artifact.default_model.result_payload["validation_summary"]["dataset_count"], 2)
        self.assertIsNotNone(artifact.calibrated_model)
        self.assertIn("Validation Limits", artifact.comparison_markdown)


if __name__ == "__main__":
    unittest.main()
