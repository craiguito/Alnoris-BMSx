from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class MeshRef:
    uri: str
    mesh_format: str = ""
    coordinate_system: str = "right_handed_z_up"
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="mesh"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.mesh_ref.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class GeometryRef:
    geometry_type: str
    uri: str = ""
    mesh: MeshRef | None = None
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="geometry"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.geometry_ref.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
