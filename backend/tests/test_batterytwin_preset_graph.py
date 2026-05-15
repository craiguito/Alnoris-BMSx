from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from backend.batterytwin.templates import deterministic_edge_id, preset_to_asset_graph, preset_to_component_twins, preset_to_geometry_refs
from backend.sim_core.system_presets import get_system_preset
from backend.twincore.storage import AssetGraphRepository, initialize_database


class BatteryTwinPresetGraphTests(unittest.TestCase):
    def _project_id(self, suffix: str) -> str:
        return f"alnoris:project:{suffix}"

    def test_preset_to_asset_graph_generic_cylindrical(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")
        project_id = self._project_id("00000000000000000000000000000000")

        graph = preset_to_asset_graph(preset, project_id)

        pack_ids = graph.node_ids_by_type("battery_pack")
        module_ids = graph.node_ids_by_type("battery_module")
        group_ids = graph.node_ids_by_type("cell_group")
        self.assertEqual(pack_ids, (f"batterytwin:{project_id}:generic_cylindrical_pack:pack",))
        self.assertEqual(len(module_ids), preset.module.module_count)
        self.assertEqual(len(group_ids), preset.pack.series_count)
        self.assertTrue(any(edge.relationship == "contains" for edge in graph.edges))
        self.assertLess(len(graph.nodes), preset.pack.series_count * preset.pack.parallel_count)

    def test_preset_to_component_twins(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")
        graph = preset_to_asset_graph(preset, self._project_id("00000000000000000000000000000000"))

        components = preset_to_component_twins(preset, graph)

        self.assertGreater(len(components), preset.module.module_count)
        for component in components:
            self.assertTrue(component.asset_id)
            self.assertTrue(component.instance_id)
            self.assertTrue(component.revision_id)
        self.assertIn("operating_limits", components[0].metadata)
        self.assertEqual(preset_to_geometry_refs(preset, graph)[0].geometry_type, "generated_parametric_battery_layout")

    def test_project_scoped_preset_ids(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")
        project_a = self._project_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
        project_b = self._project_id("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")

        graph_a = preset_to_asset_graph(preset, project_a)
        graph_b = preset_to_asset_graph(preset, project_b)

        self.assertNotEqual(graph_a.node_ids_by_type("battery_pack"), graph_b.node_ids_by_type("battery_pack"))
        with tempfile.TemporaryDirectory() as temp_dir:
            connection = initialize_database(Path(temp_dir) / "twincore.sqlite")
            AssetGraphRepository(connection).save_asset_graph(project_a, graph_a)
            AssetGraphRepository(connection).save_asset_graph(project_b, graph_b)
            assets_a = AssetGraphRepository(connection).list_assets(project_a)
            assets_b = AssetGraphRepository(connection).list_assets(project_b)
            connection.close()

        self.assertEqual(len(assets_a), len(graph_a.nodes))
        self.assertEqual(len(assets_b), len(graph_b.nodes))
        self.assertTrue(all(str(asset["asset_id"]).startswith(f"batterytwin:{project_a}:") for asset in assets_a))
        self.assertTrue(all(str(asset["asset_id"]).startswith(f"batterytwin:{project_b}:") for asset in assets_b))

    def test_deterministic_edge_ids(self) -> None:
        preset = get_system_preset("generic_cylindrical_pack")
        project_id = self._project_id("cccccccccccccccccccccccccccccccc")

        first = preset_to_asset_graph(preset, project_id)
        second = preset_to_asset_graph(preset, project_id)
        first_edge = first.edges[0]

        self.assertEqual([edge.id for edge in first.edges], [edge.id for edge in second.edges])
        self.assertEqual(
            first_edge.id,
            deterministic_edge_id(project_id, first_edge.source_id, first_edge.relationship, first_edge.target_id),
        )


if __name__ == "__main__":
    unittest.main()
