from __future__ import annotations

import unittest

from backend.batterytwin.adapters import battery_scenario_to_legacy_simulation_config
from backend.batterytwin.templates import build_default_battery_scenario_from_preset_id, preset_to_battery_scenario
from backend.sim_core.system_presets import get_system_preset
from backend.sim_core.validation import validate_simulation_config


class BatteryTwinPresetScenarioTests(unittest.TestCase):
    def test_preset_to_battery_scenario_converts_to_legacy_config(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")

        scenario = preset_to_battery_scenario(preset)
        config = battery_scenario_to_legacy_simulation_config(scenario)

        self.assertEqual(config.cells_in_series, preset.pack.series_count)
        self.assertEqual(config.cells_in_parallel, preset.pack.parallel_count)
        self.assertEqual(config.group_count, preset.pack.series_count)
        self.assertEqual(config.chemistry_name, preset.chemistry_name)
        self.assertEqual(validate_simulation_config(config), config)

    def test_build_default_battery_scenario_from_preset_id(self) -> None:
        scenario = build_default_battery_scenario_from_preset_id("generic_cylindrical_pack")

        self.assertEqual(scenario.metadata["preset_id"], "generic_cylindrical_pack")
        self.assertEqual(scenario.pack.name, "Generic Cylindrical Pack")


if __name__ == "__main__":
    unittest.main()
