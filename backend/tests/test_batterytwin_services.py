from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from backend.batterytwin import cli as batterytwin_cli
from backend.batterytwin.services import BatteryTwinProjectSummaryService
from backend.tests.test_twincore_services import create_graph, create_project, make_fast_preset, run_preset
from backend.twincore.services import AssetGraphService
from backend.twincore.storage import initialize_database


class BatteryTwinServiceTests(unittest.TestCase):
    def test_batterytwin_project_summary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            run_preset(db_path, project["project_id"])
            connection = initialize_database(db_path)
            summary = BatteryTwinProjectSummaryService(connection).get_project_summary(project["project_id"])
            connection.close()

        self.assertEqual(summary["battery_counts"]["packs"], 1)
        self.assertEqual(summary["battery_counts"]["modules"], 2)
        self.assertEqual(summary["battery_counts"]["cell_groups"], 4)
        self.assertTrue(summary["health"]["has_runs"])
        self.assertTrue(summary["health"]["has_reports"])

    def test_cli_project_summary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            create_graph(db_path, project["project_id"])
            result = batterytwin_cli.run_command(["project-summary", "--db", db_path, "--project-id", project["project_id"]])

        self.assertEqual(result["project_id"], project["project_id"])
        self.assertTrue(result["health"]["has_asset_graph"])

    def test_cli_graph_and_asset_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            create_graph(db_path, project["project_id"])
            graph = batterytwin_cli.run_command(["graph", "--db", db_path, "--project-id", project["project_id"]])
            tree = batterytwin_cli.run_command(["asset-tree", "--db", db_path, "--project-id", project["project_id"]])

        self.assertTrue(graph["nodes"])
        self.assertTrue(graph["edges"])
        self.assertEqual(tree["node_type"], "battery_pack")

    def test_cli_inspect_asset(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            create_graph(db_path, project["project_id"])
            connection = initialize_database(db_path)
            pack_asset = AssetGraphService(connection).list_assets_by_type(project["project_id"], "battery_pack")[0]
            connection.close()
            inspected = batterytwin_cli.run_command(["inspect-asset", "--db", db_path, "--asset-id", str(pack_asset["asset_id"])])

        self.assertEqual(inspected["display"]["title"], "Generic Cylindrical Pack")
        self.assertIn("Pack", inspected["display"]["badges"])

    def test_cli_run_detail(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = create_project(db_path)
            run_result = run_preset(db_path, project["project_id"])
            detail = batterytwin_cli.run_command(["run-detail", "--db", db_path, "--run-id", str(run_result["run_id"])])

        self.assertEqual(detail["solver_id"], "BatteryTwin.PackECM")
        self.assertEqual(detail["credibility"]["model_class"], "screening")
        self.assertIn("delivered_energy_wh", detail["result_summary"])

    def test_existing_cli_commands_still_work(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            presets = batterytwin_cli.run_command(["list-presets"])
            project = create_project(db_path)
            with patch("backend.batterytwin.cli.get_system_preset", return_value=make_fast_preset()):
                run_result = batterytwin_cli.run_command(
                    [
                        "run-preset",
                        "--db",
                        db_path,
                        "--project-id",
                        project["project_id"],
                        "--preset-id",
                        "generic_cylindrical_pack",
                    ]
                )
            loaded = batterytwin_cli.run_command(["get-run", "--db", db_path, "--run-id", str(run_result["run_id"])])

        self.assertIn("presets", presets)
        self.assertEqual(run_result["solver_id"], "BatteryTwin.PackECM")
        self.assertEqual(loaded["run_id"], run_result["run_id"])


if __name__ == "__main__":
    unittest.main()
