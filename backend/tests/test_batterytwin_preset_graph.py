from __future__ import annotations

import unittest

from backend.batterytwin.templates import preset_to_asset_graph, preset_to_component_twins, preset_to_geometry_refs
from backend.sim_core.system_presets import get_system_preset


class BatteryTwinPresetGraphTests(unittest.TestCase):
    def test_preset_to_asset_graph_generic_cylindrical(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")

        graph = preset_to_asset_graph(preset, "project:test:00000000000000000000000000000000")

        pack_ids = graph.node_ids_by_type("battery_pack")
        module_ids = graph.node_ids_by_type("battery_module")
        group_ids = graph.node_ids_by_type("cell_group")
        self.assertEqual(pack_ids, ("batterytwin:generic_cylindrical_pack:pack",))
        self.assertEqual(len(module_ids), preset.module.module_count)
        self.assertEqual(len(group_ids), preset.pack.series_count)
        self.assertTrue(any(edge.relationship == "contains" for edge in graph.edges))
        self.assertLess(len(graph.nodes), preset.pack.series_count * preset.pack.parallel_count)

    def test_preset_to_component_twins(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")
        graph = preset_to_asset_graph(preset, "project:test:00000000000000000000000000000000")

        components = preset_to_component_twins(preset, graph)

        self.assertGreater(len(components), preset.module.module_count)
        for component in components:
            self.assertTrue(component.asset_id)
            self.assertTrue(component.instance_id)
            self.assertTrue(component.revision_id)
        self.assertIn("operating_limits", components[0].metadata)
        self.assertEqual(preset_to_geometry_refs(preset, graph)[0].geometry_type, "generated_parametric_battery_layout")


if __name__ == "__main__":
    unittest.main()
