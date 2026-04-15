from __future__ import annotations

from dataclasses import replace
import unittest

from backend.sim_core.engine import run_simulation
from backend.sim_core.system_presets import (
    SYSTEM_PRESETS,
    battery_system_preset_catalog_to_dict,
    build_calibrated_simulation_config_for_preset,
    build_simulation_config_for_preset,
    build_system_preset_payload,
    get_system_preset,
    validate_system_preset,
)
from backend.tests.helpers import temporary_workspace_dir, write_truth_dataset


class BatterySystemPresetTests(unittest.TestCase):
    def test_registry_contains_expected_presets(self) -> None:
        preset_ids = {preset.preset_id for preset in SYSTEM_PRESETS}
        self.assertEqual(
            preset_ids,
            {"generic_cylindrical_pack", "generic_prismatic_pack", "high_power_pack"},
        )

    def test_catalog_metadata_is_serializable(self) -> None:
        catalog = battery_system_preset_catalog_to_dict()
        self.assertIn("presets", catalog)
        self.assertIn("categories", catalog)
        self.assertTrue(catalog["categories"])
        self.assertEqual(len(catalog["presets"]), 3)
        self.assertIn("experimental_presets", catalog)

    def test_preset_generation_creates_coherent_simulation_defaults(self) -> None:
        payload = build_system_preset_payload(get_system_preset("generic_cylindrical_pack"))
        simulation = payload["simulation_defaults"]
        self.assertEqual(simulation["cells_in_series"], payload["cad_defaults"]["cells_in_series"])
        self.assertEqual(simulation["group_count"], simulation["cells_in_series"])
        self.assertEqual(len(simulation["group_zone_assignments"]), simulation["group_count"])

    def test_preset_generation_creates_coherent_cad_defaults(self) -> None:
        payload = build_system_preset_payload(get_system_preset("generic_prismatic_pack"))
        cad = payload["cad_defaults"]
        self.assertEqual(cad["cell_form_factor"], "prismatic")
        self.assertGreater(cad["module_count"], 0)
        self.assertGreater(cad["busbar_thickness_mm"], 0.0)

    def test_recommended_tests_are_valid(self) -> None:
        for preset in SYSTEM_PRESETS:
            validate_system_preset(preset)

    def test_calibrated_preset_helper_returns_calibration_aware_config(self) -> None:
        preset_id = "high_power_pack"
        base_config = build_simulation_config_for_preset(preset_id)
        truth_config = replace(base_config, duration_s=90)
        truth_result = run_simulation(truth_config)

        with temporary_workspace_dir() as temp_dir:
            dataset_path = write_truth_dataset(f"{temp_dir}/truth.json", truth_config, truth_result)
            calibrated_config = build_calibrated_simulation_config_for_preset(
                preset_id,
                str(dataset_path),
                rc_branch_count=1,
            )

        self.assertEqual(calibrated_config.chemistry_name, base_config.chemistry_name)
        self.assertEqual(calibrated_config.group_count, base_config.group_count)
        self.assertTrue(calibrated_config.physics.ocv_curve)
        self.assertTrue(calibrated_config.physics.resistance_vs_soc_enabled)
        self.assertEqual(calibrated_config.electrical_model.model_type, "1rc")

    def test_experimental_presets_remain_resolvable_for_archived_workflows(self) -> None:
        preset = get_system_preset("consumer_power_tool_high_discharge")
        self.assertEqual(preset.preset_id, "consumer_power_tool_high_discharge")


if __name__ == "__main__":
    unittest.main()
