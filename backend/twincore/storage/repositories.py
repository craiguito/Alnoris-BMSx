from __future__ import annotations

import json
import sqlite3
from typing import Any

from backend.twincore.ids import new_id
from backend.twincore.schemas.asset_graph import AssetGraph
from backend.twincore.schemas.component import ComponentTwin
from backend.twincore.schemas.geometry import GeometryRef
from backend.twincore.schemas.report import ReportRecord
from backend.twincore.schemas.simulation import ResultPackage, RunManifest
from backend.twincore.schemas.validation import ValidationRecord
from backend.twincore.schemas.provenance import ProvenanceRecord
from backend.twincore.serialization import dataclass_to_dict, dataclass_to_json, stable_hash, utc_now_iso

from .sqlite import row_to_dict, transaction


def _raw_dict(row: dict[str, Any] | None) -> dict[str, Any] | None:
    if row is None:
        return None
    enriched = dict(row)
    raw_json = enriched.get("raw_json")
    if isinstance(raw_json, str):
        enriched["raw"] = json.loads(raw_json)
    return enriched


def _fetch_one(connection: sqlite3.Connection, sql: str, params: tuple[Any, ...]) -> dict[str, Any] | None:
    return _raw_dict(row_to_dict(connection.execute(sql, params).fetchone()))


def _fetch_all(connection: sqlite3.Connection, sql: str, params: tuple[Any, ...] = ()) -> list[dict[str, Any]]:
    return [
        _raw_dict(row_to_dict(row))  # type: ignore[arg-type]
        for row in connection.execute(sql, params).fetchall()
    ]


class ProjectRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def create_project(self, name: str, description: str = "") -> str:
        now = utc_now_iso()
        project_id = new_id("project")
        raw = {
            "project_id": project_id,
            "name": name,
            "description": description,
            "created_at": now,
            "updated_at": now,
        }
        self.connection.execute(
            """
            INSERT INTO projects(project_id, name, description, created_at, updated_at, raw_json)
            VALUES(?, ?, ?, ?, ?, ?)
            """,
            (project_id, name, description, now, now, dataclass_to_json(raw)),
        )
        self.connection.commit()
        return project_id

    def get_project(self, project_id: str) -> dict[str, Any] | None:
        return _fetch_one(self.connection, "SELECT * FROM projects WHERE project_id = ?", (project_id,))

    def list_projects(self) -> list[dict[str, Any]]:
        return _fetch_all(self.connection, "SELECT * FROM projects ORDER BY created_at, name")


class AssetGraphRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_asset_graph(self, project_id: str, graph: AssetGraph) -> None:
        now = utc_now_iso()
        parent_by_target = {
            edge.target_id: edge.source_id
            for edge in graph.edges
            if edge.relationship == "contains"
        }
        with transaction(self.connection):
            for node in graph.nodes.values():
                self.connection.execute(
                    """
                    INSERT OR REPLACE INTO assets(
                        asset_id, project_id, node_type, label, parent_asset_id,
                        created_at, updated_at, raw_json
                    )
                    VALUES(?, ?, ?, ?, ?, COALESCE((SELECT created_at FROM assets WHERE asset_id = ?), ?), ?, ?)
                    """,
                    (
                        node.id,
                        project_id,
                        node.node_type,
                        node.label,
                        parent_by_target.get(node.id),
                        node.id,
                        now,
                        now,
                        dataclass_to_json(node),
                    ),
                )
            for edge in graph.edges:
                self.connection.execute(
                    """
                    DELETE FROM asset_edges
                    WHERE project_id = ?
                      AND source_asset_id = ?
                      AND target_asset_id = ?
                      AND edge_type = ?
                      AND edge_id != ?
                    """,
                    (project_id, edge.source_id, edge.target_id, edge.relationship, edge.id),
                )
                self.connection.execute(
                    """
                    INSERT OR REPLACE INTO asset_edges(
                        edge_id, project_id, source_asset_id, target_asset_id,
                        edge_type, created_at, raw_json
                    )
                    VALUES(?, ?, ?, ?, ?, COALESCE((SELECT created_at FROM asset_edges WHERE edge_id = ?), ?), ?)
                    """,
                    (
                        edge.id,
                        project_id,
                        edge.source_id,
                        edge.target_id,
                        edge.relationship,
                        edge.id,
                        now,
                        dataclass_to_json(edge),
                    ),
                )

    def get_asset_graph(self, project_id: str) -> dict[str, Any]:
        return {
            "project_id": project_id,
            "assets": self.list_assets(project_id),
            "edges": self.list_edges(project_id),
        }

    def list_assets(self, project_id: str) -> list[dict[str, Any]]:
        return _fetch_all(self.connection, "SELECT * FROM assets WHERE project_id = ? ORDER BY asset_id", (project_id,))

    def list_edges(self, project_id: str) -> list[dict[str, Any]]:
        return _fetch_all(self.connection, "SELECT * FROM asset_edges WHERE project_id = ? ORDER BY edge_id", (project_id,))

    def find_assets_by_metadata(self, project_id: str, key: str, value: object) -> list[dict[str, Any]]:
        matches: list[dict[str, Any]] = []
        for asset in self.list_assets(project_id):
            raw = asset.get("raw", {})
            metadata = raw.get("metadata", {}) if isinstance(raw, dict) else {}
            if isinstance(metadata, dict) and metadata.get(key) == value:
                matches.append(asset)
        return matches

    def graph_exists_for_preset(self, project_id: str, preset_id: str) -> bool:
        return any(
            asset.get("node_type") == "battery_pack"
            for asset in self.find_assets_by_metadata(project_id, "preset_id", preset_id)
        )

    def delete_edges_for_graph_scope(self, project_id: str, preset_id: str) -> None:
        scoped_assets = self.find_assets_by_metadata(project_id, "preset_id", preset_id)
        asset_ids = {str(asset["asset_id"]) for asset in scoped_assets}
        if not asset_ids:
            return
        with transaction(self.connection):
            for asset_id in asset_ids:
                self.connection.execute(
                    """
                    DELETE FROM asset_edges
                    WHERE project_id = ?
                      AND (source_asset_id = ? OR target_asset_id = ?)
                    """,
                    (project_id, asset_id, asset_id),
                )


class ComponentRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_component(self, component: ComponentTwin) -> None:
        self._save_component_row(component)
        self.connection.commit()

    def _save_component_row(self, component: ComponentTwin) -> None:
        now = utc_now_iso()
        self.connection.execute(
            """
            INSERT OR REPLACE INTO components(
                component_id, asset_id, instance_id, revision_id, component_class,
                component_subclass, name, created_at, updated_at, raw_json
            )
            VALUES(?, ?, ?, ?, ?, ?, ?, COALESCE((SELECT created_at FROM components WHERE component_id = ?), ?), ?, ?)
            """,
            (
                component.id,
                component.asset_id,
                component.instance_id,
                component.revision_id,
                component.component_class,
                component.component_subclass,
                component.name,
                component.id,
                now,
                now,
                dataclass_to_json(component),
            ),
        )

    def save_components(self, components: tuple[ComponentTwin, ...] | list[ComponentTwin]) -> None:
        with transaction(self.connection):
            for component in components:
                self._save_component_row(component)

    def get_component(self, component_id: str) -> dict[str, Any] | None:
        return _fetch_one(self.connection, "SELECT * FROM components WHERE component_id = ?", (component_id,))

    def list_components(self, project_id: str | None = None) -> list[dict[str, Any]]:
        if project_id is None:
            return _fetch_all(self.connection, "SELECT * FROM components ORDER BY component_id")
        return _fetch_all(
            self.connection,
            """
            SELECT c.* FROM components c
            JOIN assets a ON a.asset_id = c.asset_id
            WHERE a.project_id = ?
            ORDER BY c.component_id
            """,
            (project_id,),
        )


class GeometryRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_geometry_ref(self, geometry: GeometryRef, asset_id: str = "") -> None:
        self._save_geometry_ref_row(geometry, asset_id)
        self.connection.commit()

    def _save_geometry_ref_row(self, geometry: GeometryRef, asset_id: str = "") -> None:
        now = utc_now_iso()
        self.connection.execute(
            """
            INSERT OR REPLACE INTO geometry_refs(
                geometry_id, asset_id, geometry_type, uri, created_at, updated_at, raw_json
            )
            VALUES(?, ?, ?, ?, COALESCE((SELECT created_at FROM geometry_refs WHERE geometry_id = ?), ?), ?, ?)
            """,
            (geometry.id, asset_id, geometry.geometry_type, geometry.uri, geometry.id, now, now, dataclass_to_json(geometry)),
        )

    def save_geometry_refs(self, geometry_refs: tuple[GeometryRef, ...] | list[GeometryRef]) -> None:
        with transaction(self.connection):
            for geometry in geometry_refs:
                self._save_geometry_ref_row(geometry, str(geometry.metadata.get("asset_id", "")))

    def list_geometry_refs(self, asset_id: str | None = None) -> list[dict[str, Any]]:
        if asset_id is None:
            return _fetch_all(self.connection, "SELECT * FROM geometry_refs ORDER BY geometry_id")
        return _fetch_all(self.connection, "SELECT * FROM geometry_refs WHERE asset_id = ? ORDER BY geometry_id", (asset_id,))


class ScenarioRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_scenario(self, project_id: str, scenario: Any) -> str:
        now = utc_now_iso()
        scenario_id = getattr(scenario, "id", getattr(getattr(scenario, "identity", None), "id", new_id("scenario")))
        name = getattr(scenario, "name", "Scenario")
        metadata = getattr(scenario, "metadata", {})
        base_asset_graph_id = str(metadata.get("base_asset_graph_id", "")) if isinstance(metadata, dict) else ""
        self.connection.execute(
            """
            INSERT OR REPLACE INTO scenarios(
                scenario_id, project_id, name, scenario_type, base_asset_graph_id,
                created_at, updated_at, raw_json
            )
            VALUES(?, ?, ?, ?, ?, COALESCE((SELECT created_at FROM scenarios WHERE scenario_id = ?), ?), ?, ?)
            """,
            (
                scenario_id,
                project_id,
                name,
                type(scenario).__name__,
                base_asset_graph_id,
                scenario_id,
                now,
                now,
                dataclass_to_json(scenario),
            ),
        )
        self.connection.commit()
        return scenario_id

    def get_scenario(self, scenario_id: str) -> dict[str, Any] | None:
        return _fetch_one(self.connection, "SELECT * FROM scenarios WHERE scenario_id = ?", (scenario_id,))


class SimulationRunRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def create_run(self, project_id: str, scenario_id: str, manifest: RunManifest) -> str:
        now = utc_now_iso()
        manifest_hash = stable_hash(manifest)
        raw = dataclass_to_dict(manifest)
        raw["manifest_hash"] = manifest_hash
        self.connection.execute(
            """
            INSERT OR REPLACE INTO simulation_runs(
                run_id, project_id, scenario_id, solver_id, solver_version, fidelity,
                status, started_at, completed_at, manifest_hash, result_ref, raw_json
            )
            VALUES(?, ?, ?, ?, ?, ?, ?, ?, NULL, ?, '', ?)
            """,
            (
                manifest.run_id,
                project_id,
                scenario_id,
                manifest.solver_id,
                str(manifest.metadata.get("solver_version", "")),
                str(manifest.metadata.get("fidelity", "")),
                "running",
                now,
                manifest_hash,
                dataclass_to_json(raw),
            ),
        )
        self.connection.commit()
        return manifest.run_id

    def complete_run(self, run_id: str, result_package: ResultPackage) -> None:
        now = utc_now_iso()
        result_ref = result_package.artifacts[0].id if result_package.artifacts else ""
        with transaction(self.connection):
            self.connection.execute(
                """
                UPDATE simulation_runs
                SET solver_id = ?, solver_version = ?, fidelity = ?, status = ?,
                    completed_at = ?, result_ref = ?, raw_json = ?
                WHERE run_id = ?
                """,
                (
                    result_package.solver_id,
                    result_package.solver_version,
                    str(result_package.metadata.get("fidelity", result_package.credibility_card.credibility_level)),
                    "completed",
                    now,
                    result_ref,
                    dataclass_to_json(result_package),
                    run_id,
                ),
            )
            for artifact in result_package.artifacts:
                self.connection.execute(
                    """
                    INSERT OR REPLACE INTO simulation_artifacts(
                        artifact_id, run_id, artifact_type, uri, created_at, raw_json
                    )
                    VALUES(?, ?, ?, ?, COALESCE((SELECT created_at FROM simulation_artifacts WHERE artifact_id = ?), ?), ?)
                    """,
                    (
                        artifact.id,
                        run_id,
                        artifact.artifact_type,
                        artifact.uri,
                        artifact.id,
                        now,
                        dataclass_to_json(artifact),
                    ),
                )

    def fail_run(self, run_id: str, error_message: str) -> None:
        now = utc_now_iso()
        self.connection.execute(
            """
            UPDATE simulation_runs
            SET status = ?, completed_at = ?, raw_json = ?
            WHERE run_id = ?
            """,
            ("failed", now, dataclass_to_json({"error": error_message, "completed_at": now}), run_id),
        )
        self.connection.commit()

    def get_run(self, run_id: str) -> dict[str, Any] | None:
        row = _fetch_one(self.connection, "SELECT * FROM simulation_runs WHERE run_id = ?", (run_id,))
        if row is None:
            return None
        row["artifacts"] = _fetch_all(
            self.connection,
            "SELECT * FROM simulation_artifacts WHERE run_id = ? ORDER BY created_at",
            (run_id,),
        )
        return row

    def list_runs(self, project_id: str | None = None) -> list[dict[str, Any]]:
        if project_id is None:
            return _fetch_all(self.connection, "SELECT * FROM simulation_runs ORDER BY started_at DESC")
        return _fetch_all(
            self.connection,
            "SELECT * FROM simulation_runs WHERE project_id = ? ORDER BY started_at DESC",
            (project_id,),
        )


class ValidationRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_validation_record(self, record: ValidationRecord) -> str:
        now = utc_now_iso()
        self.connection.execute(
            """
            INSERT OR REPLACE INTO validation_records(
                validation_id, run_id, target_id, validation_tier,
                uncertainty_class, created_at, raw_json
            )
            VALUES(?, ?, ?, ?, ?, COALESCE((SELECT created_at FROM validation_records WHERE validation_id = ?), ?), ?)
            """,
            (
                record.id,
                record.run_id,
                record.target_id or record.run_id,
                record.validation_tier,
                record.uncertainty_class,
                record.id,
                now,
                dataclass_to_json(record),
            ),
        )
        self.connection.commit()
        return record.id

    def list_for_run(self, run_id: str) -> list[dict[str, Any]]:
        return _fetch_all(self.connection, "SELECT * FROM validation_records WHERE run_id = ? ORDER BY created_at", (run_id,))


class ProvenanceRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_provenance_record(self, record: ProvenanceRecord) -> str:
        created_at = record.created_at_iso or utc_now_iso()
        self.connection.execute(
            """
            INSERT OR REPLACE INTO provenance_records(
                provenance_id, run_id, activity_type, created_at, raw_json
            )
            VALUES(?, ?, ?, ?, ?)
            """,
            (
                record.id,
                record.run_id,
                record.activity_type or record.source or "activity",
                created_at,
                dataclass_to_json(record),
            ),
        )
        self.connection.commit()
        return record.id

    def list_for_run(self, run_id: str) -> list[dict[str, Any]]:
        return _fetch_all(self.connection, "SELECT * FROM provenance_records WHERE run_id = ? ORDER BY created_at", (run_id,))


class ReportRepository:
    def __init__(self, connection: sqlite3.Connection) -> None:
        self.connection = connection

    def save_report(self, record: ReportRecord) -> str:
        now = utc_now_iso()
        self.connection.execute(
            """
            INSERT OR REPLACE INTO reports(
                report_id, project_id, run_id, report_type, title, created_at, raw_json
            )
            VALUES(?, ?, ?, ?, ?, COALESCE((SELECT created_at FROM reports WHERE report_id = ?), ?), ?)
            """,
            (
                record.id,
                record.project_id,
                record.run_id,
                record.report_type,
                record.title,
                record.id,
                now,
                dataclass_to_json(record),
            ),
        )
        self.connection.commit()
        return record.id

    def list_reports(self, project_id: str | None = None) -> list[dict[str, Any]]:
        if project_id is None:
            return _fetch_all(self.connection, "SELECT * FROM reports ORDER BY created_at DESC")
        return _fetch_all(self.connection, "SELECT * FROM reports WHERE project_id = ? ORDER BY created_at DESC", (project_id,))

    def get_report(self, report_id: str) -> dict[str, Any] | None:
        return _fetch_one(self.connection, "SELECT * FROM reports WHERE report_id = ?", (report_id,))

    def list_for_run(self, run_id: str) -> list[dict[str, Any]]:
        return _fetch_all(self.connection, "SELECT * FROM reports WHERE run_id = ? ORDER BY created_at DESC", (run_id,))
