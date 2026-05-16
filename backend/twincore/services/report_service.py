from __future__ import annotations

from typing import Any

from backend.twincore.services.credibility import credibility_card_to_summary
from backend.twincore.storage import ReportRepository


def _first_section(report: dict[str, Any] | None) -> dict[str, Any]:
    raw = report.get("raw", {}) if isinstance(report, dict) else {}
    sections = raw.get("sections", []) if isinstance(raw, dict) else []
    if isinstance(sections, list) and sections and isinstance(sections[0], dict):
        return sections[0]
    if isinstance(sections, tuple) and sections and isinstance(sections[0], dict):
        return sections[0]
    return {}


def _report_card(report: dict[str, Any] | None) -> dict[str, Any] | None:
    if report is None:
        return None
    section = _first_section(report)
    credibility_card = section.get("credibility_card", {})
    return {
        "report_id": report.get("report_id"),
        "title": report.get("title"),
        "report_type": report.get("report_type"),
        "project_id": report.get("project_id"),
        "run_id": report.get("run_id"),
        "created_at": report.get("created_at"),
        "summary_metrics": section.get("summary_metrics", {}),
        "credibility_card": credibility_card_to_summary(credibility_card),
        "assumptions": list(section.get("assumptions", [])) if isinstance(section.get("assumptions", []), (list, tuple)) else [],
        "warnings": list(section.get("warnings", [])) if isinstance(section.get("warnings", []), (list, tuple)) else [],
    }


class ReportSummaryService:
    def __init__(self, connection: Any) -> None:
        self.connection = connection

    def list_project_reports(self, project_id: str) -> list[dict[str, Any]]:
        return [
            card
            for card in (_report_card(report) for report in ReportRepository(self.connection).list_reports(project_id))
            if card is not None
        ]

    def get_report_detail(self, report_id: str) -> dict[str, Any] | None:
        report = ReportRepository(self.connection).get_report(report_id)
        card = _report_card(report)
        if card is None:
            return None
        raw = report.get("raw", {}) if isinstance(report, dict) else {}
        card["sections"] = raw.get("sections", []) if isinstance(raw, dict) else []
        return card

    def get_report_for_run(self, run_id: str) -> dict[str, Any] | None:
        reports = ReportRepository(self.connection).list_for_run(run_id)
        return _report_card(reports[0]) if reports else None
