from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class ScenarioOverride:
    target_path: str
    value: Any
    reason: str = ""
    schema_version: str = field(default="twincore.scenario_override.v1", init=False)


@dataclass(frozen=True)
class Scenario:
    name: str = ""
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="scenario"))
    description: str = ""
    overrides: tuple[ScenarioOverride, ...] = ()
    payload: Any | None = None
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.scenario.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
