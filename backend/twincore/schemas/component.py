from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class ComponentTwin:
    component_type: str = "component"
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="component"))
    asset_id: str = ""
    instance_id: str = ""
    revision_id: str = "rev0"
    component_class: str = "component"
    component_subclass: str = ""
    name: str = ""
    parent_id: str | None = None
    material_ids: tuple[str, ...] = ()
    geometry_id: str | None = None
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.component_twin.v1", init=False)

    def __post_init__(self) -> None:
        if not self.instance_id:
            object.__setattr__(self, "instance_id", self.id)
        if not self.component_class or self.component_class == "component":
            object.__setattr__(self, "component_class", self.component_type)
        if not self.name:
            object.__setattr__(self, "name", self.component_type.replace("_", " ").title())

    @property
    def id(self) -> str:
        return self.identity.id
