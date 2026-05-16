from __future__ import annotations

from typing import Any

from backend.twincore.storage import ScenarioRepository


def _raw_scenario(row: dict[str, Any] | None) -> dict[str, Any]:
    raw = row.get("raw", {}) if isinstance(row, dict) else {}
    return raw if isinstance(raw, dict) else {}


def _scenario_summary(row: dict[str, Any] | None) -> dict[str, Any] | None:
    if row is None:
        return None
    raw = _raw_scenario(row)
    metadata = raw.get("metadata", {}) if isinstance(raw.get("metadata", {}), dict) else {}
    pack = raw.get("pack", {}) if isinstance(raw.get("pack", {}), dict) else {}
    cell = pack.get("cell", {}) if isinstance(pack.get("cell", {}), dict) else {}
    return {
        "scenario_id": row.get("scenario_id"),
        "name": row.get("name"),
        "scenario_type": row.get("scenario_type"),
        "preset_id": metadata.get("preset_id", ""),
        "project_id": row.get("project_id") or metadata.get("project_id", ""),
        "duration_s": raw.get("duration_s"),
        "time_step_s": raw.get("time_step_s"),
        "initial_soc": raw.get("initial_soc"),
        "discharge_current_a": raw.get("discharge_current_a"),
        "pack": {
            "cells_in_series": pack.get("cells_in_series"),
            "cells_in_parallel": pack.get("cells_in_parallel"),
            "group_count": pack.get("group_count"),
            "chemistry_name": cell.get("chemistry_name") or metadata.get("chemistry_name", ""),
        },
    }


class ScenarioService:
    def __init__(self, connection: Any) -> None:
        self.connection = connection

    def get_scenario_summary(self, scenario_id: str) -> dict[str, Any]:
        row = ScenarioRepository(self.connection).get_scenario(scenario_id)
        summary = _scenario_summary(row)
        if summary is None:
            raise ValueError(f"Unknown scenario_id: {scenario_id}")
        return summary

    def list_project_scenarios(self, project_id: str) -> list[dict[str, Any]]:
        rows = self.connection.execute(
            "SELECT * FROM scenarios WHERE project_id = ? ORDER BY updated_at DESC, created_at DESC",
            (project_id,),
        ).fetchall()
        summaries = []
        for row in rows:
            scenario = ScenarioRepository(self.connection).get_scenario(str(row["scenario_id"]))
            summary = _scenario_summary(scenario)
            if summary is not None:
                summaries.append(summary)
        return summaries
