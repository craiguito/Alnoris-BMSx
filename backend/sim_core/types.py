from __future__ import annotations

import csv
from dataclasses import dataclass, field


@dataclass(frozen=True)
class RcBranchParams:
    """One RC branch in a Thevenin-style equivalent circuit model."""

    resistance_ohm: float
    capacitance_f: float


@dataclass(frozen=True)
class ElectricalModelConfig:
    model_type: str = "rint"
    r0_ohm_per_cell: float | None = None
    rc_branches: tuple[RcBranchParams, ...] = ()


@dataclass(frozen=True)
class CurrentProfilePoint:
    time_s: int
    current_a: float


@dataclass(frozen=True)
class CurrentProfile:
    points: tuple[CurrentProfilePoint, ...] = ()

    @classmethod
    def from_csv(cls, filepath: str) -> "CurrentProfile":
        """Load a simple time_s,current_a drive cycle CSV."""
        points: list[CurrentProfilePoint] = []
        with open(filepath, "r", encoding="utf-8", newline="") as handle:
            reader = csv.reader(handle)
            for row in reader:
                if not row:
                    continue
                first = row[0].strip().lower()
                if first == "time_s":
                    continue
                if len(row) < 2:
                    raise ValueError("Current profile CSV rows must contain time_s,current_a.")
                points.append(CurrentProfilePoint(time_s=int(float(row[0])), current_a=float(row[1])))
        return cls(points=tuple(points))


@dataclass(frozen=True)
class GroupVariationConfig:
    capacity_variation_fraction: float = 0.0
    resistance_variation_fraction: float = 0.0
    initial_soc_variation_abs: float = 0.0


@dataclass(frozen=True)
class DegradationConfig:
    capacity_fade_per_throughput_ah: float = 2.5e-5
    resistance_growth_per_throughput_ah: float = 1.2e-5
    temperature_reference_c: float = 25.0
    temperature_acceleration_per_c: float = 0.02
    depth_of_discharge_weight: float = 0.15


@dataclass(frozen=True)
class BalancingConfig:
    """Passive balancing bleeds current from the highest groups to reduce spread."""

    enabled: bool = False
    mode: str = "passive"
    voltage_threshold_v: float | None = None
    soc_threshold: float | None = None
    bleed_current_a: float = 0.0
    max_active_groups: int | None = None


@dataclass(frozen=True)
class FaultSpec:
    fault_type: str
    group_index: int
    factor: float
    start_time_s: float = 0.0
    end_time_s: float | None = None


@dataclass(frozen=True)
class FaultConfig:
    """Faults are engineering stress injections for analysis, not certification models."""

    faults: tuple[FaultSpec, ...] = ()


@dataclass(frozen=True)
class SimulationConfig:
    cell_nominal_voltage: float
    cell_full_voltage: float
    cell_empty_voltage: float
    cell_cutoff_voltage: float
    cell_capacity_ah: float
    cells_in_series: int
    cells_in_parallel: int
    internal_resistance_ohm_per_cell: float
    ambient_temp_c: float
    discharge_current_a: float
    duration_s: int
    time_step_s: int
    initial_soc: float
    pack_mass_kg: float
    pack_heat_capacity_j_per_kgk: float
    cooling_coeff_w_per_k: float
    electrical_model: ElectricalModelConfig = field(default_factory=ElectricalModelConfig)
    current_profile: CurrentProfile | None = None
    group_count: int | None = None
    group_variation: GroupVariationConfig = field(default_factory=GroupVariationConfig)
    degradation: DegradationConfig = field(default_factory=DegradationConfig)
    balancing: BalancingConfig = field(default_factory=BalancingConfig)
    faults: FaultConfig = field(default_factory=FaultConfig)


@dataclass(frozen=True)
class SimulationWarning:
    code: str
    message: str
    severity: str


@dataclass(frozen=True)
class PackProperties:
    nominal_voltage_v: float
    capacity_ah: float
    theoretical_energy_wh: float
    resistance_ohm: float
    capacity_as: float
    group_count: int
    series_factor: float
    group_nominal_voltage_v: float
    group_capacity_ah: float
    group_capacity_as: float
    group_base_resistance_ohm: float


@dataclass(frozen=True)
class DegradationState:
    cumulative_throughput_ah: float = 0.0
    capacity_loss_fraction: float = 0.0
    resistance_growth_fraction: float = 0.0


@dataclass(frozen=True)
class GroupElectricalState:
    rc_branch_voltages_v: tuple[float, ...] = ()


@dataclass(frozen=True)
class CellGroupState:
    index: int
    soc: float
    temp_c: float
    resistance_scale: float = 1.0
    capacity_scale: float = 1.0
    electrical_state: GroupElectricalState = field(default_factory=GroupElectricalState)
    degradation: DegradationState = field(default_factory=DegradationState)


@dataclass(frozen=True)
class GroupStepResult:
    terminal_voltage_v: float
    open_circuit_voltage_v: float
    heat_w: float
    balance_current_a: float
    fault_flags: list[str]
    next_state: CellGroupState


@dataclass(frozen=True)
class SimulationPoint:
    time_s: int
    soc: float
    terminal_voltage_v: float
    power_w: float
    heat_w: float
    temp_c: float
    current_a: float
    pack_voltage_v: float
    pack_power_w: float
    pack_heat_w: float
    pack_temp_max_c: float
    pack_temp_avg_c: float
    soc_min: float
    soc_max: float
    soc_avg: float
    temp_avg: float
    temp_max: float
    group_voltage_min_v: float
    group_voltage_max_v: float
    weakest_group_index: int
    hottest_group_index: int
    group_soc: list[float]
    group_voltage: list[float]
    group_temp: list[float]
    balancing_active_groups: list[int]
    fault_active_groups: list[int]
    group_balance_current_a: list[float]
    group_fault_flags: list[list[str]]


@dataclass(frozen=True)
class SimulationSummary:
    runtime_s: int
    delivered_energy_wh: float
    delivered_capacity_ah: float
    min_terminal_voltage_v: float
    peak_temp_c: float
    final_soc: float
    termination_reason: str
    warnings: list[SimulationWarning]
    final_soc_avg: float
    soc_spread: float
    max_group_temp_c: float
    min_group_voltage_v: float
    estimated_capacity_retention: float
    estimated_resistance_growth: float
    electrical_model_type: str
    profile_used: bool
    total_energy_wh: float
    weakest_group_index: int
    hottest_group_index: int
    max_group_temp: float
    min_group_voltage: float
    capacity_retention: float
    resistance_growth: float
    balancing_used: bool
    total_balance_ah: float
    fault_count: int
    first_faulted_group_index: int | None


@dataclass(frozen=True)
class SimulationResult:
    pack_nominal_voltage_v: float
    pack_capacity_ah: float
    theoretical_energy_wh: float
    summary: SimulationSummary
    time_series: list[SimulationPoint]
