from __future__ import annotations

import argparse
import json
import sys
from typing import Any

from backend.batterytwin.solvers.pack_ecm import BatteryPackECMSolverPlugin
from backend.batterytwin.templates import (
    preset_to_asset_graph,
    preset_to_battery_scenario,
    preset_to_component_twins,
    preset_to_geometry_refs,
)
from backend.sim_core.system_presets import battery_system_preset_catalog_to_dict, get_system_preset
from backend.twincore.schemas.report import ReportRecord
from backend.twincore.schemas.simulation import RunManifest
from backend.twincore.serialization import dataclass_to_dict, stable_hash
from backend.twincore.storage import (
    AssetGraphRepository,
    ComponentRepository,
    GeometryRepository,
    ProjectRepository,
    ProvenanceRepository,
    ReportRepository,
    ScenarioRepository,
    SimulationRunRepository,
    ValidationRepository,
    initialize_database,
    transaction,
)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="python -m backend.batterytwin.cli")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list-presets")

    init_db = subparsers.add_parser("init-db")
    init_db.add_argument("--db", required=False)

    create_project = subparsers.add_parser("create-project")
    create_project.add_argument("--db", required=False)
    create_project.add_argument("--name", required=True)
    create_project.add_argument("--description", default="")

    create_graph = subparsers.add_parser("create-preset-graph")
    create_graph.add_argument("--db", required=False)
    create_graph.add_argument("--project-id", required=True)
    create_graph.add_argument("--preset-id", required=True)

    run_preset = subparsers.add_parser("run-preset")
    run_preset.add_argument("--db", required=False)
    run_preset.add_argument("--project-id", required=True)
    run_preset.add_argument("--preset-id", required=True)
    run_preset.add_argument("--refresh-graph", action="store_true")

    get_run = subparsers.add_parser("get-run")
    get_run.add_argument("--db", required=False)
    get_run.add_argument("--run-id", required=True)

    list_runs = subparsers.add_parser("list-runs")
    list_runs.add_argument("--db", required=False)
    list_runs.add_argument("--project-id", required=False)
    return parser


def _preset_graph_counts(connection: Any, project_id: str, preset_id: str) -> dict[str, Any]:
    asset_repository = AssetGraphRepository(connection)
    assets = asset_repository.find_assets_by_metadata(project_id, "preset_id", preset_id)
    asset_ids = {str(asset["asset_id"]) for asset in assets}
    edges = [
        edge
        for edge in asset_repository.list_edges(project_id)
        if edge.get("source_asset_id") in asset_ids and edge.get("target_asset_id") in asset_ids
    ]
    components = [
        component
        for component in ComponentRepository(connection).list_components(project_id)
        if component.get("asset_id") in asset_ids
    ]
    geometry_refs = [
        geometry
        for geometry in GeometryRepository(connection).list_geometry_refs()
        if geometry.get("asset_id") in asset_ids
    ]
    return {
        "project_id": project_id,
        "preset_id": preset_id,
        "asset_graph_id": f"batterytwin:{project_id}:{preset_id}:asset_graph",
        "asset_count": len(assets),
        "edge_count": len(edges),
        "component_count": len(components),
        "geometry_count": len(geometry_refs),
    }


def _save_preset_graph(connection: Any, project_id: str, preset_id: str) -> dict[str, Any]:
    preset = get_system_preset(preset_id)
    graph = preset_to_asset_graph(preset, project_id)
    components = preset_to_component_twins(preset, graph)
    geometry_refs = preset_to_geometry_refs(preset, graph)

    with transaction(connection):
        AssetGraphRepository(connection).save_asset_graph(project_id, graph)
        ComponentRepository(connection).save_components(components)
        GeometryRepository(connection).save_geometry_refs(geometry_refs)

    counts = _preset_graph_counts(connection, project_id, preset.preset_id)
    counts["asset_graph_id"] = graph.id
    return counts


def _report_for_run(
    *,
    project_id: str,
    preset_id: str,
    preset_display_name: str,
    scenario_id: str,
    run_id: str,
    result_summary: dict[str, Any],
    credibility_card: Any,
) -> ReportRecord:
    warnings = result_summary.get("warnings", [])
    return ReportRecord(
        title=f"BatteryTwin Screening Report - {preset_display_name}",
        report_type="battery_screening_summary",
        project_id=project_id,
        run_id=run_id,
        sections=(
            {
                "summary_metrics": result_summary,
                "credibility_card": dataclass_to_dict(credibility_card),
                "warnings": warnings,
                "assumptions": list(getattr(credibility_card, "assumptions", ())),
                "run_id": run_id,
                "scenario_id": scenario_id,
                "preset_id": preset_id,
            },
        ),
        metadata={
            "preset_id": preset_id,
            "scenario_id": scenario_id,
        },
    )


def _run_preset(connection: Any, project_id: str, preset_id: str, *, refresh_graph: bool = False) -> dict[str, Any]:
    preset = get_system_preset(preset_id)
    graph_reused = AssetGraphRepository(connection).graph_exists_for_preset(project_id, preset.preset_id) and not refresh_graph
    graph_result = (
        _preset_graph_counts(connection, project_id, preset.preset_id)
        if graph_reused
        else _save_preset_graph(connection, project_id, preset_id)
    )
    graph = preset_to_asset_graph(preset, project_id)
    scenario = preset_to_battery_scenario(preset, project_id=project_id)
    scenario_id = ScenarioRepository(connection).save_scenario(project_id, scenario)
    solver = BatteryPackECMSolverPlugin()
    manifest = RunManifest(
        scenario=scenario,
        solver_id=solver.solver_id,
        asset_graph=graph,
        metadata={
            "project_id": project_id,
            "preset_id": preset.preset_id,
            "solver_version": solver.solver_version,
            "fidelity": solver.fidelity,
        },
    )
    manifest_hash = stable_hash(manifest)
    run_repository = SimulationRunRepository(connection)
    run_repository.create_run(project_id, scenario_id, manifest)
    try:
        result_package = solver.run(manifest)
        run_repository.complete_run(manifest.run_id, result_package)
    except Exception as exc:
        run_repository.fail_run(manifest.run_id, str(exc))
        raise

    provenance_id = ""
    if result_package.provenance is not None:
        provenance_id = ProvenanceRepository(connection).save_provenance_record(result_package.provenance)

    validation_id = ""
    validation_repository = ValidationRepository(connection)
    for record in result_package.credibility_card.validation_records:
        validation_id = validation_repository.save_validation_record(record)

    report = _report_for_run(
        project_id=project_id,
        preset_id=preset.preset_id,
        preset_display_name=preset.display_name,
        scenario_id=scenario_id,
        run_id=manifest.run_id,
        result_summary=dict(result_package.summary),
        credibility_card=result_package.credibility_card,
    )
    report_id = ReportRepository(connection).save_report(report)

    return {
        "project_id": project_id,
        "preset_id": preset.preset_id,
        "asset_graph_id": graph_result["asset_graph_id"],
        "graph_reused": graph_reused,
        "scenario_id": scenario_id,
        "run_id": manifest.run_id,
        "solver_id": result_package.solver_id,
        "solver_version": result_package.solver_version,
        "manifest_hash": manifest_hash,
        "summary": result_package.summary,
        "credibility_card": dataclass_to_dict(result_package.credibility_card),
        "provenance_id": provenance_id,
        "validation_id": validation_id,
        "report_id": report_id,
    }


def run_command(argv: list[str] | None = None) -> dict[str, Any]:
    args = _parser().parse_args(argv)

    if args.command == "list-presets":
        return battery_system_preset_catalog_to_dict()

    connection = initialize_database(getattr(args, "db", None))
    try:
        if args.command == "init-db":
            return {"db": args.db or "data/twincore/twincore.sqlite", "initialized": True}
        if args.command == "create-project":
            project_id = ProjectRepository(connection).create_project(args.name, args.description)
            return {"project_id": project_id, "name": args.name, "description": args.description}
        if args.command == "create-preset-graph":
            return _save_preset_graph(connection, args.project_id, args.preset_id)
        if args.command == "run-preset":
            return _run_preset(connection, args.project_id, args.preset_id, refresh_graph=args.refresh_graph)
        if args.command == "get-run":
            run = SimulationRunRepository(connection).get_run(args.run_id)
            if run is None:
                raise ValueError(f"Unknown run_id: {args.run_id}")
            run["provenance_records"] = ProvenanceRepository(connection).list_for_run(args.run_id)
            run["validation_records"] = ValidationRepository(connection).list_for_run(args.run_id)
            run["reports"] = ReportRepository(connection).list_for_run(args.run_id)
            run["scenario"] = ScenarioRepository(connection).get_scenario(str(run["scenario_id"]))
            raw = run.get("raw", {})
            if isinstance(raw, dict):
                if "credibility_card" in raw:
                    run["credibility_card"] = raw["credibility_card"]
                if "summary" in raw:
                    run["result_summary"] = raw["summary"]
            return run
        if args.command == "list-runs":
            return {"runs": SimulationRunRepository(connection).list_runs(args.project_id)}
    finally:
        connection.close()

    raise ValueError(f"Unsupported command: {args.command}")


def main(argv: list[str] | None = None) -> int:
    try:
        result = run_command(argv)
    except Exception as exc:
        json.dump({"ok": False, "error": str(exc)}, sys.stdout)
        sys.stdout.write("\n")
        return 1
    json.dump({"ok": True, "result": result}, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
