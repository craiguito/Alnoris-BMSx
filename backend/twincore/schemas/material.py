from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class MaterialRecord:
    name: str
    material_class: str = ""
    density_kg_per_m3: float | None = None
    thermal_conductivity_w_per_mk: float | None = None
    electrical_resistivity_ohm_m: float | None = None
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="material"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.material_record.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
