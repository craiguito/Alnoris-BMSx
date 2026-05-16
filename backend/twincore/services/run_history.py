from __future__ import annotations

from typing import Any

from backend.twincore.services.credibility import credibility_card_to_summary
from backend.twincore.storage import ProvenanceRepository, ReportRepository, ScenarioRepository, SimulationRunRepository, ValidationRepository


SUMMARY_KEYS = (
    "final_soc_avg",
    "delivered_energy_wh",
    "max_group_temp_c",
    "min_group_voltage_v",
    "termination_reason",
    "capacity_retention",
    "resistance_growth",
)


def _summary_subset(raw_summary: Any) -> dict[str, Any]:
    if not isinstance(raw_summary, dict):
        return {}
    return {key: raw_summary[key] for key in SUMMARY_KEYS if key in raw_summary}


def _raw_result(run: dict[str, Any] | None) -> dict[str, Any]:
    raw = run.get("raw", {}) if isinstance(run, dict) else {}
    return raw if isinstance(raw, dict) else {}


class RunHistoryService:
    def __init__(self, connection: Any) -> None:
        self.connection = connection

    def _run_card(self, run: dict[str, Any]) -> dict[str, Any]:
        raw = _raw_result(run)
        reports = ReportRepository(self.connection).list_for_run(str(run["run_id"]))
        return {
            "run_id": run.get("run_id"),
            "scenario_id": run.get("scenario_id"),
            "solver_id": run.get("solver_id"),
            "solver_version": run.get("solver_version"),
            "fidelity": run.get("fidelity"),
            "status": run.get("status"),
            "started_at": run.get("started_at"),
            "completed_at": run.get("completed_at"),
            "summary": _summary_subset(raw.get("summary", {})),
            "credibility": credibility_card_to_summary(raw.get("credibility_card", {})),
            "report_ids": [report["report_id"] for report in reports],
        }

    def list_project_runs(self, project_id: str, limit: int = 50) -> list[dict[str, Any]]:
        runs = SimulationRunRepository(self.connection).list_runs(project_id)
        return [self._run_card(run) for run in runs[: max(0, limit)]]

    def get_run_detail(self, run_id: str) -> dict[str, Any]:
        run = SimulationRunRepository(self.connection).get_run(run_id)
        if run is None:
            raise ValueError(f"Unknown run_id: {run_id}")
        raw = _raw_result(run)
        card = self._run_card(run)
        return {
            **card,
            "project_id": run.get("project_id"),
            "manifest_hash": run.get("manifest_hash"),
            "result_ref": run.get("result_ref"),
            "result_summary": raw.get("summary", {}),
            "credibility_card": credibility_card_to_summary(raw.get("credibility_card", {})),
            "artifacts": run.get("artifacts", []),
            "scenario": ScenarioRepository(self.connection).get_scenario(str(run.get("scenario_id", ""))),
            "provenance_records": ProvenanceRepository(self.connection).list_for_run(run_id),
            "validation_records": ValidationRepository(self.connection).list_for_run(run_id),
            "reports": ReportRepository(self.connection).list_for_run(run_id),
        }

    def get_latest_run_for_project(self, project_id: str) -> dict[str, Any] | None:
        runs = self.list_project_runs(project_id, limit=1)
        return runs[0] if runs else None

    def get_latest_run_for_asset(self, asset_id: str) -> dict[str, Any] | None:
        row = self.connection.execute("SELECT project_id FROM assets WHERE asset_id = ?", (asset_id,)).fetchone()
        if row is None:
            return None
        return self.get_latest_run_for_project(str(row["project_id"]))
