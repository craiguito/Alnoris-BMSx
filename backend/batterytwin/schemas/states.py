from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class BatteryElectricalState:
    pack_voltage_v: float = 0.0
    pack_current_a: float = 0.0
    pack_power_w: float = 0.0
    soc_avg: float = 0.0
    soc_min: float = 0.0
    soc_max: float = 0.0
    schema_version: str = field(default="batterytwin.electrical_state.v1", init=False)


@dataclass(frozen=True)
class BatteryThermalState:
    pack_temp_avg_c: float = 25.0
    pack_temp_max_c: float = 25.0
    max_core_temp_c: float = 25.0
    max_surface_temp_c: float = 25.0
    schema_version: str = field(default="batterytwin.thermal_state.v1", init=False)


@dataclass(frozen=True)
class BatteryDegradationState:
    capacity_retention: float = 1.0
    resistance_growth: float = 0.0
    cumulative_charge_throughput_ah: float = 0.0
    cumulative_discharge_throughput_ah: float = 0.0
    schema_version: str = field(default="batterytwin.degradation_state.v1", init=False)


@dataclass(frozen=True)
class BatteryBMSState:
    balancing_active: bool = False
    fault_count: int = 0
    warning_codes: tuple[str, ...] = ()
    termination_reason: str = ""
    schema_version: str = field(default="batterytwin.bms_state.v1", init=False)
