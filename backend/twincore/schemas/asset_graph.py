from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from backend.twincore.ids import require_valid_id

from .identity import IdentityRef


@dataclass(frozen=True)
class AssetNode:
    node_type: str
    label: str = ""
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="asset_node"))
    component_id: str | None = None
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.asset_node.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class AssetEdge:
    source_id: str
    target_id: str
    relationship: str = "contains"
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="asset_edge"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.asset_edge.v1", init=False)

    def __post_init__(self) -> None:
        require_valid_id(self.source_id, "source_id")
        require_valid_id(self.target_id, "target_id")

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass
class AssetGraph:
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="asset_graph", name="Asset Graph"))
    nodes: dict[str, AssetNode] = field(default_factory=dict)
    edges: list[AssetEdge] = field(default_factory=list)
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.asset_graph.v1", init=False)

    def add_node(self, node: AssetNode) -> AssetNode:
        require_valid_id(node.id)
        if node.id in self.nodes:
            raise ValueError(f"AssetGraph already contains node id {node.id}.")
        self.nodes[node.id] = node
        return node

    def add_edge(self, source_id: str, target_id: str, relationship: str = "contains", **metadata: Any) -> AssetEdge:
        if source_id not in self.nodes:
            raise ValueError(f"AssetGraph does not contain source node id {source_id}.")
        if target_id not in self.nodes:
            raise ValueError(f"AssetGraph does not contain target node id {target_id}.")
        edge = AssetEdge(source_id=source_id, target_id=target_id, relationship=relationship, metadata=dict(metadata))
        self.edges.append(edge)
        return edge

    def children_of(self, source_id: str, relationship: str | None = None) -> tuple[AssetNode, ...]:
        child_ids = [
            edge.target_id
            for edge in self.edges
            if edge.source_id == source_id and (relationship is None or edge.relationship == relationship)
        ]
        return tuple(self.nodes[node_id] for node_id in child_ids)

    def node_ids_by_type(self, node_type: str) -> tuple[str, ...]:
        return tuple(node.id for node in self.nodes.values() if node.node_type == node_type)
