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
    """Engineering degradation approximation using throughput, calendar, SOC, and DoD stress."""

    throughput_capacity_fade_per_ah: float | None = None
    throughput_resistance_growth_per_ah: float | None = None
    calendar_capacity_fade_per_hour: float = 2.0e-7
    calendar_resistance_growth_per_hour: float = 8.0e-8
    high_soc_capacity_accel: float = 0.45
    high_soc_resistance_accel: float = 0.25
    dod_stress_factor: float = 0.20
    charge_stress_factor: float = 0.18
    reference_temp_c: float | None = None
    temperature_accel_per_c: float | None = None
    high_soc_threshold: float = 0.8
    charge_current_stress_threshold_a: float | None = None
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
class ThermalZoneConfig:
    zone_id: int
    name: str
    ambient_temp_c: float | None = None
    cooling_coeff_w_per_k: float | None = None
    cooling_coeff_multiplier: float | None = None
    note: str = ""


@dataclass(frozen=True)
class SocLookupPoint:
    soc: float
    multiplier: float


@dataclass(frozen=True)
class OcvLookupPoint:
    soc: float
    voltage_v: float


@dataclass(frozen=True)
class PhysicsConfig:
    """Lightweight engineering physics extensions for ECM-based pack studies."""

    discharge_efficiency: float = 1.0
    charge_efficiency: float = 0.995
    resistance_temperature_alpha_per_c: float = 0.0
    resistance_reference_temp_c: float = 25.0
    capacity_temperature_reference_c: float = 25.0
    capacity_cold_derate_per_c: float = 0.0
    min_capacity_scale: float = 0.65
    self_discharge_per_day: float = 0.0
    interconnect_resistance_ohm_per_group: float = 0.0
    pack_interconnect_resistance_ohm: float = 0.0
    neighbor_thermal_coupling_w_per_k: float = 0.0
    resistance_vs_soc_enabled: bool = False
    resistance_soc_curve: tuple[SocLookupPoint, ...] = (
        SocLookupPoint(0.0, 1.18),
        SocLookupPoint(0.2, 1.08),
        SocLookupPoint(0.5, 1.0),
        SocLookupPoint(0.8, 1.03),
        SocLookupPoint(1.0, 1.08),
    )
    hysteresis_enabled: bool = False
    hysteresis_max_voltage_v: float = 0.0
    hysteresis_response_rate_per_s: float = 0.15
    hysteresis_relaxation_tau_s: float = 180.0
    hysteresis_current_scale_a: float = 5.0
    rc_state_dependence_enabled: bool = False
    rc_low_soc_multiplier: float = 1.2
    rc_high_temp_multiplier_per_c: float = 0.0
    diffusion_stress_enabled: bool = False
    diffusion_stress_max_v: float = 0.0
    diffusion_stress_build_rate_per_s: float = 0.10
    diffusion_stress_decay_tau_s: float = 90.0
    diffusion_stress_current_scale_a: float = 8.0
    diffusion_stress_resistance_coeff: float = 0.0
    two_node_thermal_enabled: bool = False
    core_surface_thermal_coupling_w_per_k: float = 1.8
    surface_thermal_mass_fraction: float = 0.35
    core_thermal_mass_j_per_k: float | None = None
    surface_thermal_mass_j_per_k: float | None = None
    nonlinear_cooling_enabled: bool = False
    nonlinear_cooling_delta_threshold_c: float = 12.0
    nonlinear_cooling_gain_per_c: float = 0.02
    reversible_heat_enabled: bool = False
    reversible_heat_coeff_v_per_k: float = 0.0
    charge_resistance_multiplier: float = 1.0
    discharge_resistance_multiplier: float = 1.0


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
    thermal_zones: tuple[ThermalZoneConfig, ...] = ()
    group_zone_assignments: tuple[int, ...] = ()
    group_labels: tuple[str, ...] = ()
    group_entity_ids: tuple[str, ...] = ()
    chemistry_name: str = "generic_liion"
    physics: PhysicsConfig = field(default_factory=PhysicsConfig)


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
    cumulative_time_s: float = 0.0
    cumulative_charge_throughput_ah: float = 0.0
    cumulative_discharge_throughput_ah: float = 0.0
    cumulative_high_soc_time_s: float = 0.0
    cumulative_cycle_stress: float = 0.0
    capacity_loss_fraction: float = 0.0
    resistance_growth_fraction: float = 0.0
    soc_window_min: float = 1.0
    soc_window_max: float = 0.0


@dataclass(frozen=True)
class GroupElectricalState:
    rc_branch_voltages_v: tuple[float, ...] = ()
    hysteresis_voltage_v: float = 0.0
    diffusion_stress_v: float = 0.0


@dataclass(frozen=True)
class CellGroupState:
    index: int
    soc: float
    temp_c: float
    core_temp_c: float = 25.0
    surface_temp_c: float = 25.0
    resistance_scale: float = 1.0
    capacity_scale: float = 1.0
    electrical_state: GroupElectricalState = field(default_factory=GroupElectricalState)
    degradation: DegradationState = field(default_factory=DegradationState)


@dataclass(frozen=True)
class GroupStepResult:
    terminal_voltage_v: float
    open_circuit_voltage_v: float
    heat_w: float
    reversible_heat_w: float
    effective_resistance_ohm: float
    balance_current_a: float
    fault_flags: list[str]
    next_state: CellGroupState
    hysteresis_voltage_v: float = 0.0
    diffusion_stress_v: float = 0.0
    degradation_rate_indicator: float = 0.0


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
    group_core_temp: list[float]
    group_surface_temp: list[float]
    group_core_temp_c: list[float]
    group_surface_temp_c: list[float]
    group_hysteresis_v: list[float]
    group_diffusion_stress: list[float]
    group_effective_resistance_ohm: list[float]
    group_heat_w: list[float]
    balancing_active_groups: list[int]
    fault_active_groups: list[int]
    group_balance_current_a: list[float]
    group_fault_flags: list[list[str]]
    group_zone_ids: list[int]
    group_labels: list[str]
    group_entity_ids: list[str]
    zone_temp_max_c: dict[int, float]
    estimated_capacity_retention: float
    estimated_resistance_multiplier: float
    degradation_rate_indicator: float


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
    hottest_zone_id: int
    max_zone_temp_c: float
    cumulative_charge_throughput_ah: float
    cumulative_discharge_throughput_ah: float
    cumulative_high_soc_time_h: float
    estimated_cycle_stress: float
    degradation_model_version: str
    group_capacity_retention: list[float]
    group_resistance_growth: list[float]
    max_core_temp_c: float
    max_surface_temp_c: float
    temp_gradient_max_c: float
    max_diffusion_stress: float
    nonlinear_features_enabled: list[str]
    chemistry_name: str


@dataclass(frozen=True)
class SimulationResult:
    pack_nominal_voltage_v: float
    pack_capacity_ah: float
    theoretical_energy_wh: float
    summary: SimulationSummary
    time_series: list[SimulationPoint]
