from __future__ import annotations

from typing import Any

from backend.twincore.services.asset_graph_service import AssetGraphService, _asset_card, _metadata, _row_to_dict
from backend.twincore.services.credibility import credibility_card_to_summary
from backend.twincore.services.run_history import RunHistoryService
from backend.twincore.storage import ComponentRepository, GeometryRepository, ProvenanceRepository, SimulationRunRepository, ValidationRepository


def _component_card(row: dict[str, Any] | None) -> dict[str, Any] | None:
    if row is None:
        return None
    raw = row.get("raw", {}) if isinstance(row.get("raw", {}), dict) else {}
    return {
        "component_id": row.get("component_id"),
        "asset_id": row.get("asset_id"),
        "instance_id": row.get("instance_id"),
        "revision_id": row.get("revision_id"),
        "component_class": row.get("component_class"),
        "component_subclass": row.get("component_subclass"),
        "name": row.get("name"),
        "created_at": row.get("created_at"),
        "updated_at": row.get("updated_at"),
        "metadata": raw.get("metadata", {}) if isinstance(raw, dict) else {},
    }


def _first_component_for_asset(connection: Any, asset_id: str) -> dict[str, Any] | None:
    row = connection.execute(
        "SELECT * FROM components WHERE asset_id = ? ORDER BY component_id LIMIT 1",
        (asset_id,),
    ).fetchone()
    return _row_to_dict(row)


def _latest_completed_run(connection: Any, project_id: str) -> dict[str, Any] | None:
    for run in SimulationRunRepository(connection).list_runs(project_id):
        if run.get("status") == "completed":
            return run
    return None


def _result_payload(raw_result: dict[str, Any]) -> dict[str, Any]:
    artifacts = raw_result.get("artifacts", [])
    if isinstance(artifacts, list):
        for artifact in artifacts:
            if isinstance(artifact, dict) and isinstance(artifact.get("payload"), dict):
                return artifact["payload"]
    return {}


def _indexed_value(point: dict[str, Any], keys: tuple[str, ...], index: int) -> Any:
    for key in keys:
        values = point.get(key)
        if isinstance(values, list) and 0 <= index < len(values):
            return values[index]
    return None


class ComponentInspectorService:
    def __init__(self, connection: Any) -> None:
        self.connection = connection
        self.graph_service = AssetGraphService(connection)

    def inspect_component(self, component_id: str) -> dict[str, Any]:
        component = ComponentRepository(self.connection).get_component(component_id)
        if component is None:
            return self._missing_payload("component", component_id)
        return self.inspect_asset(str(component["asset_id"]))

    def inspect_asset(self, asset_id: str) -> dict[str, Any]:
        asset = self.graph_service._get_asset_row(asset_id)
        if asset is None:
            return self._missing_payload("asset", asset_id)
        component = _first_component_for_asset(self.connection, asset_id)
        project_id = str(asset["project_id"])
        parent = self.graph_service.get_parent(asset_id)
        children = self.graph_service.get_children(asset_id)
        geometry_refs = GeometryRepository(self.connection).list_geometry_refs(asset_id)
        latest_runs = RunHistoryService(self.connection).list_project_runs(project_id, limit=5)
        latest_run = _latest_completed_run(self.connection, project_id)
        latest_state = self._latest_state_for_asset(asset, latest_run)
        validation_summary = self._validation_summary(latest_run)
        provenance_summary = self._provenance_summary(latest_run)
        badges = [str(asset.get("node_type", "asset"))]
        if latest_run is None:
            badges.append("no simulation run yet")
        elif latest_run.get("status"):
            badges.append(str(latest_run["status"]))
        if validation_summary.get("validation_tier"):
            badges.append(str(validation_summary["validation_tier"]))

        component_card = _component_card(component)
        return {
            "asset": _asset_card(asset),
            "component": component_card,
            "parent": parent,
            "children": children,
            "geometry_refs": geometry_refs,
            "latest_runs": latest_runs,
            "latest_state": latest_state,
            "validation_summary": validation_summary,
            "provenance_summary": provenance_summary,
            "display": {
                "title": str(asset.get("label") or (component_card or {}).get("name") or asset_id),
                "subtitle": str((component_card or {}).get("component_class") or asset.get("node_type") or ""),
                "badges": badges,
            },
        }

    def inspect_pack(self, project_id: str) -> dict[str, Any]:
        packs = self.graph_service.list_assets_by_type(project_id, "battery_pack")
        if not packs:
            return self._missing_payload("battery_pack", project_id)
        return self.inspect_asset(str(packs[0]["asset_id"]))

    def list_component_cards(self, project_id: str) -> list[dict[str, Any]]:
        assets = {str(asset["asset_id"]): asset for asset in AssetGraphService(self.connection).get_project_graph(project_id)["nodes"]}
        cards: list[dict[str, Any]] = []
        for component in ComponentRepository(self.connection).list_components(project_id):
            asset = assets.get(str(component["asset_id"]))
            card = _component_card(component)
            if card is not None:
                card["asset"] = asset
                card["display"] = {
                    "title": card.get("name") or (asset or {}).get("label") or card.get("component_id"),
                    "subtitle": card.get("component_class", ""),
                    "badges": [badge for badge in (card.get("component_class"), (asset or {}).get("node_type")) if badge],
                }
                cards.append(card)
        return cards

    def _latest_state_for_asset(self, asset: dict[str, Any], latest_run: dict[str, Any] | None) -> dict[str, Any]:
        if latest_run is None:
            return {}
        raw_result = latest_run.get("raw", {}) if isinstance(latest_run.get("raw", {}), dict) else {}
        summary = raw_result.get("summary", {}) if isinstance(raw_result.get("summary", {}), dict) else {}
        if asset.get("node_type") == "battery_pack":
            return {
                "run_id": latest_run.get("run_id"),
                "final_soc_avg": summary.get("final_soc_avg", summary.get("final_soc")),
                "delivered_energy_wh": summary.get("delivered_energy_wh"),
                "max_group_temp_c": summary.get("max_group_temp_c"),
                "min_group_voltage_v": summary.get("min_group_voltage_v"),
                "termination_reason": summary.get("termination_reason"),
            }
        if asset.get("node_type") == "cell_group":
            metadata = _metadata(asset)
            index = int(metadata.get("group_index", metadata.get("series_index", -1)))
            payload = _result_payload(raw_result)
            time_series = payload.get("time_series", []) if isinstance(payload, dict) else []
            final_point = time_series[-1] if isinstance(time_series, list) and time_series else {}
            if isinstance(final_point, dict) and index >= 0:
                return {
                    "run_id": latest_run.get("run_id"),
                    "group_index": index,
                    "soc": _indexed_value(final_point, ("group_soc", "group_true_soc"), index),
                    "voltage_v": _indexed_value(final_point, ("group_voltage",), index),
                    "temp_c": _indexed_value(final_point, ("group_temp", "group_surface_temp_c", "group_core_temp_c"), index),
                    "thermal_zone_id": metadata.get("thermal_zone_id"),
                }
        return {}

    def _validation_summary(self, latest_run: dict[str, Any] | None) -> dict[str, Any]:
        if latest_run is None:
            return {}
        records = ValidationRepository(self.connection).list_for_run(str(latest_run["run_id"]))
        raw_result = latest_run.get("raw", {}) if isinstance(latest_run.get("raw", {}), dict) else {}
        credibility = credibility_card_to_summary(raw_result.get("credibility_card", {}))
        return {
            "record_count": len(records),
            "validation_tier": credibility.get("validation_tier", ""),
            "uncertainty_class": credibility.get("uncertainty_class", ""),
            "credibility": credibility,
        }

    def _provenance_summary(self, latest_run: dict[str, Any] | None) -> dict[str, Any]:
        if latest_run is None:
            return {}
        records = ProvenanceRepository(self.connection).list_for_run(str(latest_run["run_id"]))
        return {
            "record_count": len(records),
            "activity_types": [record.get("activity_type", "") for record in records],
        }

    def _missing_payload(self, kind: str, identifier: str) -> dict[str, Any]:
        return {
            "asset": None,
            "component": None,
            "parent": None,
            "children": [],
            "geometry_refs": [],
            "latest_runs": [],
            "latest_state": {},
            "validation_summary": {},
            "provenance_summary": {},
            "display": {
                "title": identifier,
                "subtitle": kind,
                "badges": [f"missing {kind}"],
            },
        }
