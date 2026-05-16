from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from backend.batterytwin import cli as batterytwin_cli
from backend.sim_core.system_presets import BatterySystemPreset, CellPresetRef, ModulePreset, PackPreset
from backend.twincore.services import (
    AssetGraphService,
    ComponentInspectorService,
    ReportSummaryService,
    RunHistoryService,
    ScenarioService,
    credibility_card_to_summary,
)
from backend.twincore.storage import initialize_database


def make_fast_preset() -> BatterySystemPreset:
    return BatterySystemPreset(
        preset_id="generic_cylindrical_pack",
        display_name="Generic Cylindrical Pack",
        description="Fast service test preset.",
        category="Tests",
        chemistry_name="generic_liion",
        chemistry_display_name="Generic Li-ion",
        cell=CellPresetRef(key="samsung_30q", form_factor="cylindrical", radius_mm=10.5, height_mm=70.0, width_mm=21.0, depth_mm=21.0),
        module=ModulePreset(module_count=2),
        pack=PackPreset(series_count=4, parallel_count=1, x_spacing_mm=28.0, z_spacing_mm=28.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=1.0,
        pack_mass_kg=1.0,
        pack_heat_capacity_j_per_kgk=900.0,
        discharge_current_a=1.0,
        duration_s=5,
        time_step_s=1,
        initial_soc=1.0,
        thermal_zone_count=2,
        thermal_zone_cooling_multipliers=(1.0, 1.0),
        recommended_virtual_tests=("constant_current_discharge",),
        operating_limits={"max_discharge_current_a": 2.0},
    )


def create_project(db_path: str) -> dict[str, str]:
    return batterytwin_cli.run_command(["create-project", "--db", db_path, "--name", "Service Test"])


def create_graph(db_path: str, project_id: str) -> dict[str, object]:
    with patch("backend.batterytwin.cli.get_system_preset", return_value=make_fast_preset()):
        return batterytwin_cli.run_command(
            [
                "create-preset-graph",
                "--db",
                db_path,
                "--project-id",
                project_id,
                "--preset-id",
                "generic_cylindrical_pack",
            ]
        )


def run_preset(db_path: str, project_id: str) -> dict[str, object]:
    with patch("backend.batterytwin.cli.get_system_preset", return_value=make_fast_preset()):
        return batterytwin_cli.run_command(
            [
                "run-preset",
                "--db",
                db_path,
                "--project-id",
                project_id,
                "--preset-id",
                "generic_cylindrical_pack",
            ]
        )


class TwinCoreServiceTests(unittest.TestCase):
    def test_asset_graph_service_counts(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            create_graph(db_path, project["project_id"])
            connection = initialize_database(db_path)
            graph = AssetGraphService(connection).get_project_graph(project["project_id"])
            connection.close()

        self.assertEqual(graph["node_type_counts"]["battery_pack"], 1)
        self.assertEqual(graph["node_type_counts"]["battery_module"], 2)
        self.assertEqual(graph["node_type_counts"]["cell_group"], 4)
        self.assertEqual(graph["node_type_counts"]["cooling_channel"], 1)
        self.assertEqual(graph["node_type_counts"]["bms"], 1)
        self.assertEqual(graph["node_type_counts"]["enclosure"], 1)

    def test_asset_tree_contains_modules_and_groups(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            create_graph(db_path, project["project_id"])
            connection = initialize_database(db_path)
            tree = AssetGraphService(connection).get_asset_tree(project["project_id"])
            connection.close()

        modules = [child for child in tree["children"] if child["node_type"] == "battery_module"]
        self.assertEqual(tree["node_type"], "battery_pack")
        self.assertEqual(len(modules), 2)
        self.assertTrue(any(grandchild["node_type"] == "cell_group" for module in modules for grandchild in module["children"]))

    def test_component_inspector_pack_before_run(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            create_graph(db_path, project["project_id"])
            connection = initialize_database(db_path)
            service = ComponentInspectorService(connection)
            pack_asset = AssetGraphService(connection).list_assets_by_type(project["project_id"], "battery_pack")[0]
            inspected = service.inspect_asset(str(pack_asset["asset_id"]))
            connection.close()

        self.assertEqual(inspected["latest_state"], {})
        self.assertIn("no simulation run yet", inspected["display"]["badges"])

    def test_component_inspector_pack_after_run(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            run_preset(db_path, project["project_id"])
            connection = initialize_database(db_path)
            pack_asset = AssetGraphService(connection).list_assets_by_type(project["project_id"], "battery_pack")[0]
            inspected = ComponentInspectorService(connection).inspect_asset(str(pack_asset["asset_id"]))
            connection.close()

        self.assertTrue(inspected["latest_runs"])
        self.assertIn("final_soc_avg", inspected["latest_state"])
        self.assertEqual(inspected["validation_summary"]["validation_tier"], "regression_tested")

    def test_run_history_service_cards(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            run_preset(db_path, project["project_id"])
            connection = initialize_database(db_path)
            runs = RunHistoryService(connection).list_project_runs(project["project_id"])
            connection.close()

        self.assertEqual(runs[0]["solver_id"], "BatteryTwin.PackECM")
        self.assertEqual(runs[0]["status"], "completed")
        self.assertIn("delivered_energy_wh", runs[0]["summary"])
        self.assertEqual(runs[0]["credibility"]["model_class"], "screening")

    def test_scenario_summary_service(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            run_result = run_preset(db_path, project["project_id"])
            connection = initialize_database(db_path)
            summary = ScenarioService(connection).get_scenario_summary(str(run_result["scenario_id"]))
            connection.close()

        self.assertEqual(summary["preset_id"], "generic_cylindrical_pack")
        self.assertEqual(summary["pack"]["cells_in_series"], 4)
        self.assertEqual(summary["pack"]["cells_in_parallel"], 1)
        self.assertEqual(summary["pack"]["chemistry_name"], "generic_liion")

    def test_report_summary_service(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            run_preset(db_path, project["project_id"])
            connection = initialize_database(db_path)
            service = ReportSummaryService(connection)
            reports = service.list_project_reports(project["project_id"])
            detail = service.get_report_detail(str(reports[0]["report_id"]))
            connection.close()

        self.assertEqual(reports[0]["credibility_card"]["model_class"], "screening")
        self.assertIn("summary_metrics", detail)
        self.assertIn("warnings", detail)

    def test_credibility_projection_screening_warning(self) -> None:
        summary = credibility_card_to_summary(
            {
                "model_class": "screening",
                "approved_use_range": ["early design trade studies", "not certification-grade"],
                "validation_tier": "regression_tested",
                "uncertainty_class": "engineering_screening",
            }
        )

        self.assertEqual(summary["display"]["badge"], "Screening")
        self.assertEqual(summary["display"]["warning"], "Not certification-grade")


if __name__ == "__main__":
    unittest.main()
