from __future__ import annotations

import sqlite3
import tempfile
import unittest
from pathlib import Path

from backend.batterytwin.schemas import BatteryScenario
from backend.twincore.schemas import AssetGraph, AssetNode, ResultPackage, RunManifest, SimulationArtifact
from backend.twincore.schemas.provenance import ProvenanceRecord
from backend.twincore.schemas.report import ReportRecord
from backend.twincore.schemas.validation import ValidationRecord
from backend.twincore.serialization import dataclass_to_dict, stable_hash
from backend.twincore.storage import (
    AssetGraphRepository,
    ProjectRepository,
    ProvenanceRepository,
    ReportRepository,
    ScenarioRepository,
    SimulationRunRepository,
    ValidationRepository,
    initialize_database,
)


class TwinCoreStorageTests(unittest.TestCase):
    def test_serialization_handles_nested_dataclasses_and_stable_hash(self) -> None:
        scenario = BatteryScenario()

        payload = dataclass_to_dict(scenario)
        first_hash = stable_hash(scenario)
        second_hash = stable_hash(payload)

        self.assertEqual(payload["schema_version"], "batterytwin.scenario.v1")
        self.assertEqual(payload["pack"]["schema_version"], "batterytwin.battery_pack.v1")
        self.assertEqual(first_hash, second_hash)

    def test_initialize_twincore_sqlite_db(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = Path(temp_dir) / "twincore.sqlite"
            connection = initialize_database(db_path)
            rows = connection.execute(
                "SELECT name FROM sqlite_master WHERE type = 'table'"
            ).fetchall()
            connection.close()

        table_names = {row["name"] for row in rows}
        self.assertIn("projects", table_names)
        self.assertIn("assets", table_names)
        self.assertIn("simulation_runs", table_names)
        self.assertIn("provenance_records", table_names)

    def test_project_repository_create_project(self) -> None:
        connection = sqlite3.connect(":memory:")
        connection.row_factory = sqlite3.Row
        from backend.twincore.storage.sqlite import execute_schema

        execute_schema(connection)
        repository = ProjectRepository(connection)

        project_id = repository.create_project("Storage Test", "local db")
        loaded = repository.get_project(project_id)

        connection.close()
        self.assertIsNotNone(loaded)
        self.assertEqual(loaded["name"], "Storage Test")

    def test_repositories_save_run_evidence_and_report(self) -> None:
        connection = sqlite3.connect(":memory:")
        connection.row_factory = sqlite3.Row
        from backend.twincore.storage.sqlite import execute_schema

        execute_schema(connection)
        project_id = ProjectRepository(connection).create_project("Run Test")
        graph = AssetGraph()
        pack = graph.add_node(AssetNode(node_type="battery_pack", label="Pack"))
        ScenarioRepository(connection).save_scenario(project_id, BatteryScenario())
        AssetGraphRepository(connection).save_asset_graph(project_id, graph)
        scenario = BatteryScenario()
        scenario_id = ScenarioRepository(connection).save_scenario(project_id, scenario)
        manifest = RunManifest(scenario=scenario, solver_id="BatteryTwin.PackECM")
        result = ResultPackage(
            solver_id="BatteryTwin.PackECM",
            solver_version="0.1.0",
            run_id=manifest.run_id,
            artifacts=(SimulationArtifact(artifact_type="test", payload={"ok": True}),),
        )

        runs = SimulationRunRepository(connection)
        runs.create_run(project_id, scenario_id, manifest)
        runs.complete_run(manifest.run_id, result)
        validation_id = ValidationRepository(connection).save_validation_record(
            ValidationRecord(run_id=manifest.run_id, target_id=scenario_id)
        )
        provenance_id = ProvenanceRepository(connection).save_provenance_record(
            ProvenanceRecord(run_id=manifest.run_id, activity_type="battery_pack_ecm_simulation")
        )
        report_id = ReportRepository(connection).save_report(
            ReportRecord(title="Report", project_id=project_id, run_id=manifest.run_id)
        )

        loaded_run = runs.get_run(manifest.run_id)
        connection.close()

        self.assertEqual(pack.id, graph.node_ids_by_type("battery_pack")[0])
        self.assertIsNotNone(loaded_run)
        self.assertEqual(loaded_run["status"], "completed")
        self.assertTrue(validation_id)
        self.assertTrue(provenance_id)
        self.assertTrue(report_id)


if __name__ == "__main__":
    unittest.main()
