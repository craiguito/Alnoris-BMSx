from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from backend.twincore.ids import new_id, require_valid_id

from .states import BatteryBMSState, BatteryDegradationState, BatteryElectricalState, BatteryThermalState


@dataclass(frozen=True)
class BatterySimulationResult:
    run_id: str = ""
    scenario_id: str = ""
    pack_nominal_voltage_v: float = 0.0
    pack_capacity_ah: float = 0.0
    theoretical_energy_wh: float = 0.0
    summary: dict[str, Any] = field(default_factory=dict)
    time_series: tuple[dict[str, Any], ...] = ()
    final_electrical_state: BatteryElectricalState = field(default_factory=BatteryElectricalState)
    final_thermal_state: BatteryThermalState = field(default_factory=BatteryThermalState)
    final_degradation_state: BatteryDegradationState = field(default_factory=BatteryDegradationState)
    final_bms_state: BatteryBMSState = field(default_factory=BatteryBMSState)
    legacy_result: Any | None = None
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.simulation_result.v1", init=False)

    def __post_init__(self) -> None:
        run_id = self.run_id or new_id("battery_simulation_run")
        require_valid_id(run_id, "run_id")
        object.__setattr__(self, "run_id", run_id)
