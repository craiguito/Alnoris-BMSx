from __future__ import annotations

import json
import unittest
from pathlib import Path

from backend.sim_core.engine import run_simulation
from backend.sim_core.truth_data_manager import TruthDatasetRegistry
from backend.tests.helpers import make_config, make_profile, temporary_workspace_dir, write_truth_dataset


def _registry_metadata(dataset_id: str, display_name: str, *, status: str = "canonical") -> dict[str, object]:
    return {
        "dataset_id": dataset_id,
        "display_name": display_name,
        "description": f"Synthetic registry seed for {display_name}.",
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": 14.8,
        "nominal_capacity_ah": 3.2,
        "temperature_range_c": [20.0, 35.0],
        "current_profile_type": "synthetic_pulse",
        "tags": ["synthetic", "unit-test"],
        "source": "backend-tests",
        "status": status,
        "created_at": "2026-04-16T00:00:00Z",
        "notes": "Synthetic dataset used for registry tests.",
    }


class TruthDataManagerTests(unittest.TestCase):
    def test_registry_lists_registered_truth_datasets(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            discharge_current_a=0.0,
            duration_s=60,
            current_profile=make_profile((0, 0.0), (10, 2.0), (30, 0.0), (45, 1.0)),
        )
        truth_result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            truth_dir = Path(temp_dir) / "truth_data"
            truth_dir.mkdir(parents=True, exist_ok=True)
            write_truth_dataset(
                truth_dir / "canonical.json",
                config,
                truth_result,
                extra_metadata=_registry_metadata("canonical_pack", "Canonical Pack", status="canonical"),
            )
            write_truth_dataset(
                truth_dir / "trusted.json",
                config,
                truth_result,
                extra_metadata=_registry_metadata("trusted_pack", "Trusted Pack", status="trusted"),
            )

            registry = TruthDatasetRegistry(truth_dir)
            datasets = registry.list_truth_datasets()

        self.assertEqual([item.dataset_id for item in datasets], ["canonical_pack", "trusted_pack"])
        self.assertEqual(datasets[0].status, "canonical")
        resolved = registry.get_truth_dataset("canonical_pack")
        self.assertEqual(resolved.dataset_display_name, "Canonical Pack")
        self.assertEqual(resolved.source_type, "registry")

    def test_validate_truth_dataset_file_rejects_malformed_records(self) -> None:
        with temporary_workspace_dir() as temp_dir:
            dataset_path = Path(temp_dir) / "broken.json"
            dataset_path.write_text(
                json.dumps(
                    {
                        "metadata": _registry_metadata("broken_dataset", "Broken Dataset"),
                        "records": [
                            {"time_s": 5, "current_a": 1.0, "voltage_v": 4.0, "temp_c": 25.0, "soc": 0.9},
                            {"time_s": 5, "current_a": 1.0, "voltage_v": 3.9, "temp_c": 25.2, "soc": 0.8},
                        ],
                    },
                    indent=2,
                ),
                encoding="utf-8",
            )

            result = TruthDatasetRegistry().validate_truth_dataset_file(dataset_path)

        self.assertFalse(result.is_valid)
        self.assertFalse(result.registry_ready)
        self.assertTrue(any("strictly increasing" in error for error in result.errors))

    def test_direct_path_resolution_preserves_legacy_dataset_compatibility(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            discharge_current_a=0.0,
            duration_s=40,
            current_profile=make_profile((0, 0.0), (10, 1.5), (20, 0.0), (30, 0.8)),
        )
        truth_result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            dataset_path = write_truth_dataset(Path(temp_dir) / "legacy_truth.json", config, truth_result)
            registry = TruthDatasetRegistry(Path(temp_dir) / "truth_data")
            validation = registry.validate_truth_dataset_file(dataset_path)
            resolved = registry.resolve_truth_dataset_input(str(dataset_path))

        self.assertTrue(validation.is_valid)
        self.assertFalse(validation.registry_ready)
        self.assertTrue(any("registry-ready" in warning for warning in validation.warnings))
        self.assertEqual(resolved.source_type, "path")
        self.assertEqual(len(resolved.dataset.records), len(truth_result.time_series))


if __name__ == "__main__":
    unittest.main()
