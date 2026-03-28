from __future__ import annotations

import unittest

from backend.sim_core.system_presets import (
    SYSTEM_PRESETS,
    battery_system_preset_catalog_to_dict,
    build_system_preset_payload,
    get_system_preset,
    validate_system_preset,
)


class BatterySystemPresetTests(unittest.TestCase):
    def test_registry_contains_expected_presets(self) -> None:
        preset_ids = {preset.preset_id for preset in SYSTEM_PRESETS}
        self.assertIn("road_ev_generic_nmc_cylindrical", preset_ids)
        self.assertIn("road_ev_generic_lfp_prismatic", preset_ids)
        self.assertIn("frontier_lithium_sulfur_aerospace", preset_ids)

    def test_catalog_metadata_is_serializable(self) -> None:
        catalog = battery_system_preset_catalog_to_dict()
        self.assertIn("presets", catalog)
        self.assertIn("categories", catalog)
        self.assertTrue(catalog["categories"])

    def test_preset_generation_creates_coherent_simulation_defaults(self) -> None:
        payload = build_system_preset_payload(get_system_preset("road_ev_generic_nmc_cylindrical"))
        simulation = payload["simulation_defaults"]
        self.assertEqual(simulation["cells_in_series"], payload["cad_defaults"]["cells_in_series"])
        self.assertEqual(simulation["group_count"], simulation["cells_in_series"])
        self.assertEqual(len(simulation["group_zone_assignments"]), simulation["group_count"])

    def test_preset_generation_creates_coherent_cad_defaults(self) -> None:
        payload = build_system_preset_payload(get_system_preset("road_ev_generic_lfp_prismatic"))
        cad = payload["cad_defaults"]
        self.assertEqual(cad["cell_form_factor"], "prismatic")
        self.assertGreater(cad["module_count"], 0)
        self.assertGreater(cad["busbar_thickness_mm"], 0.0)

    def test_recommended_tests_are_valid(self) -> None:
        for preset in SYSTEM_PRESETS:
            validate_system_preset(preset)


if __name__ == "__main__":
    unittest.main()
