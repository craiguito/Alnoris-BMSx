from __future__ import annotations

import json
from collections import Counter, deque
from typing import Any

from backend.twincore.storage import AssetGraphRepository, ComponentRepository, GeometryRepository


def _row_to_dict(row: Any) -> dict[str, Any] | None:
    if row is None:
        return None
    payload = dict(row)
    raw_json = payload.get("raw_json")
    if isinstance(raw_json, str) and "raw" not in payload:
        try:
            payload["raw"] = json.loads(raw_json)
        except json.JSONDecodeError:
            payload["raw"] = {}
    return payload


def _metadata(row: dict[str, Any] | None) -> dict[str, Any]:
    if not row:
        return {}
    raw = row.get("raw", {})
    metadata = raw.get("metadata", {}) if isinstance(raw, dict) else {}
    return dict(metadata) if isinstance(metadata, dict) else {}


def _asset_card(row: dict[str, Any] | None) -> dict[str, Any] | None:
    if row is None:
        return None
    return {
        "asset_id": row.get("asset_id"),
        "project_id": row.get("project_id"),
        "node_type": row.get("node_type"),
        "label": row.get("label"),
        "parent_asset_id": row.get("parent_asset_id"),
        "created_at": row.get("created_at"),
        "updated_at": row.get("updated_at"),
        "metadata": _metadata(row),
    }


def _edge_card(row: dict[str, Any]) -> dict[str, Any]:
    return {
        "edge_id": row.get("edge_id"),
        "project_id": row.get("project_id"),
        "source_asset_id": row.get("source_asset_id"),
        "target_asset_id": row.get("target_asset_id"),
        "edge_type": row.get("edge_type"),
        "created_at": row.get("created_at"),
        "metadata": _metadata(row),
    }


class AssetGraphService:
    def __init__(self, connection: Any) -> None:
        self.connection = connection

    def _get_asset_row(self, asset_id: str) -> dict[str, Any] | None:
        row = self.connection.execute("SELECT * FROM assets WHERE asset_id = ?", (asset_id,)).fetchone()
        return _row_to_dict(row)

    def get_project_graph(self, project_id: str) -> dict[str, Any]:
        repository = AssetGraphRepository(self.connection)
        assets = repository.list_assets(project_id)
        edges = repository.list_edges(project_id)
        asset_ids = {str(asset["asset_id"]) for asset in assets}
        geometry_refs = [
            geometry
            for geometry in GeometryRepository(self.connection).list_geometry_refs()
            if geometry.get("asset_id") in asset_ids
        ]
        node_type_counts = Counter(str(asset.get("node_type", "")) for asset in assets)
        return {
            "project_id": project_id,
            "nodes": [_asset_card(asset) for asset in assets],
            "edges": [_edge_card(edge) for edge in edges],
            "counts": {
                "assets": len(assets),
                "edges": len(edges),
                "components": len(ComponentRepository(self.connection).list_components(project_id)),
                "geometry_refs": len(geometry_refs),
            },
            "node_type_counts": dict(sorted(node_type_counts.items())),
        }

    def get_asset_tree(self, project_id: str) -> dict[str, Any]:
        repository = AssetGraphRepository(self.connection)
        assets = repository.list_assets(project_id)
        edges = [edge for edge in repository.list_edges(project_id) if edge.get("edge_type") == "contains"]
        by_id = {str(asset["asset_id"]): asset for asset in assets}
        child_ids = {str(edge["target_asset_id"]) for edge in edges}
        children_by_parent: dict[str, list[str]] = {}
        for edge in edges:
            children_by_parent.setdefault(str(edge["source_asset_id"]), []).append(str(edge["target_asset_id"]))

        roots = [asset for asset in assets if str(asset["asset_id"]) not in child_ids]
        roots.sort(key=lambda row: (0 if row.get("node_type") == "battery_pack" else 1, str(row.get("asset_id"))))

        def build(asset_id: str) -> dict[str, Any]:
            asset = by_id[asset_id]
            return {
                "asset_id": asset.get("asset_id"),
                "label": asset.get("label"),
                "node_type": asset.get("node_type"),
                "metadata": _metadata(asset),
                "children": [build(child_id) for child_id in sorted(children_by_parent.get(asset_id, [])) if child_id in by_id],
            }

        if not roots:
            return {"project_id": project_id, "asset_id": "", "label": "", "node_type": "asset_tree", "children": []}
        if len(roots) == 1:
            root = build(str(roots[0]["asset_id"]))
            root["project_id"] = project_id
            return root
        return {
            "project_id": project_id,
            "asset_id": f"{project_id}:asset_tree",
            "label": "Asset Tree",
            "node_type": "asset_tree",
            "children": [build(str(root["asset_id"])) for root in roots],
        }

    def get_asset_neighborhood(self, asset_id: str, depth: int = 1) -> dict[str, Any]:
        center = self._get_asset_row(asset_id)
        if center is None:
            raise ValueError(f"Unknown asset_id: {asset_id}")
        project_id = str(center["project_id"])
        repository = AssetGraphRepository(self.connection)
        all_edges = repository.list_edges(project_id)
        by_id = {str(asset["asset_id"]): asset for asset in repository.list_assets(project_id)}
        adjacent: dict[str, set[str]] = {}
        for edge in all_edges:
            source = str(edge["source_asset_id"])
            target = str(edge["target_asset_id"])
            adjacent.setdefault(source, set()).add(target)
            adjacent.setdefault(target, set()).add(source)

        selected = {asset_id}
        queue: deque[tuple[str, int]] = deque([(asset_id, 0)])
        while queue:
            current, current_depth = queue.popleft()
            if current_depth >= max(0, depth):
                continue
            for neighbor in adjacent.get(current, set()):
                if neighbor not in selected:
                    selected.add(neighbor)
                    queue.append((neighbor, current_depth + 1))

        selected_edges = [
            edge
            for edge in all_edges
            if str(edge["source_asset_id"]) in selected and str(edge["target_asset_id"]) in selected
        ]
        return {
            "asset_id": asset_id,
            "project_id": project_id,
            "depth": depth,
            "center": _asset_card(center),
            "nodes": [_asset_card(by_id[node_id]) for node_id in sorted(selected) if node_id in by_id],
            "edges": [_edge_card(edge) for edge in selected_edges],
        }

    def list_assets_by_type(self, project_id: str, node_type: str) -> list[dict[str, Any]]:
        assets = AssetGraphRepository(self.connection).list_assets(project_id)
        return [
            card
            for card in (_asset_card(asset) for asset in assets if asset.get("node_type") == node_type)
            if card is not None
        ]

    def get_children(self, asset_id: str) -> list[dict[str, Any]]:
        asset = self._get_asset_row(asset_id)
        if asset is None:
            return []
        project_id = str(asset["project_id"])
        rows = self.connection.execute(
            """
            SELECT a.* FROM assets a
            JOIN asset_edges e ON e.target_asset_id = a.asset_id
            WHERE e.project_id = ? AND e.source_asset_id = ? AND e.edge_type = 'contains'
            ORDER BY a.node_type, a.label, a.asset_id
            """,
            (project_id, asset_id),
        ).fetchall()
        return [
            card
            for card in (_asset_card(_row_to_dict(row)) for row in rows)
            if card is not None
        ]

    def get_parent(self, asset_id: str) -> dict[str, Any] | None:
        asset = self._get_asset_row(asset_id)
        if asset is None:
            return None
        parent_id = asset.get("parent_asset_id")
        if not parent_id:
            edge = self.connection.execute(
                """
                SELECT source_asset_id FROM asset_edges
                WHERE target_asset_id = ? AND edge_type = 'contains'
                ORDER BY created_at
                LIMIT 1
                """,
                (asset_id,),
            ).fetchone()
            parent_id = edge["source_asset_id"] if edge is not None else ""
        if not parent_id:
            return None
        return _asset_card(self._get_asset_row(str(parent_id)))
