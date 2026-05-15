from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from backend.twincore.schemas.identity import IdentityRef

from .components import BatteryPackTwin


@dataclass(frozen=True)
class BatteryScenario:
    name: str = "Battery Scenario"
    pack: BatteryPackTwin = field(default_factory=BatteryPackTwin)
    ambient_temp_c: float = 25.0
    discharge_current_a: float = 1.0
    duration_s: int = 60
    time_step_s: int = 1
    initial_soc: float = 1.0
    current_profile: tuple[tuple[int, float], ...] = ()
    legacy_config_overrides: dict[str, Any] = field(default_factory=dict)
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="battery_scenario"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.scenario.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
