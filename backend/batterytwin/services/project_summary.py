from __future__ import annotations

from typing import Any

from backend.twincore.services import AssetGraphService, ReportSummaryService, RunHistoryService


BATTERY_NODE_MAP = {
    "battery_pack": "packs",
    "battery_module": "modules",
    "cell_group": "cell_groups",
    "cooling_channel": "cooling_channels",
    "bms": "bms",
    "enclosure": "enclosures",
}


class BatteryTwinProjectSummaryService:
    def __init__(self, connection: Any) -> None:
        self.connection = connection

    def get_project_summary(self, project_id: str) -> dict[str, Any]:
        graph = AssetGraphService(self.connection).get_project_graph(project_id)
        node_counts = graph["node_type_counts"]
        run_service = RunHistoryService(self.connection)
        report_service = ReportSummaryService(self.connection)
        latest_run = run_service.get_latest_run_for_project(project_id)
        reports = report_service.list_project_reports(project_id)
        battery_counts = {
            output_key: int(node_counts.get(node_type, 0))
            for node_type, output_key in BATTERY_NODE_MAP.items()
        }
        return {
            "project_id": project_id,
            "asset_counts": graph["counts"],
            "battery_counts": battery_counts,
            "latest_run": latest_run,
            "latest_report": reports[0] if reports else None,
            "available_actions": [
                "inspect_components",
                "run_preset",
                "view_reports",
            ],
            "health": {
                "has_asset_graph": graph["counts"]["assets"] > 0,
                "has_runs": latest_run is not None,
                "has_reports": bool(reports),
            },
        }
