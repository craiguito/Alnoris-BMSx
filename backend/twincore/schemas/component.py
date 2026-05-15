from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class ComponentTwin:
    component_type: str = "component"
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="component"))
    parent_id: str | None = None
    material_ids: tuple[str, ...] = ()
    geometry_id: str | None = None
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.component_twin.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
