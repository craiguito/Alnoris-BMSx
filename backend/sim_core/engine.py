from __future__ import annotations

"""Battery pack ECM simulator.

Groups represent equal-sized series segments of the pack, not arbitrary fractional slices.
This keeps the grouped model physically interpretable while remaining lightweight enough
for early-stage design work. The simulator uses ECM dynamics rather than full
electrochemical or distributed thermal physics.
"""

from dataclasses import dataclass
from statistics import mean

from .chemistry import apply_chemistry_defaults
from .physics.balancing import balance_currents_for_groups
from .physics.degradation import effective_capacity_scale, effective_resistance_scale, step_degradation
from .physics.electrical import (
    capacity_temperature_scale,
    compute_heat_w,
    compute_next_soc,
    compute_open_circuit_voltage,
    compute_power_w,
    compute_reversible_heat_w,
    compute_terminal_voltage,
    current_direction_resistance_multiplier,
    diffusion_stress_resistance_multiplier,
    effective_r0_ohm,
    interconnect_resistance_ohm,
    soc_resistance_multiplier,
    step_electrical_state,
    temperature_adjusted_resistance_ohm,
    validate_electrical_model,
)
from .physics.faults import effects_for_group, reported_soc_for_group
from .physics.pack import build_group_states, derive_pack_properties
from .physics.thermal import compute_next_group_temperatures, resolve_group_thermal_context
from .profiles import current_for_time
from .types import (
    CellGroupState,
    SimulationConfig,
    SimulationPoint,
    SimulationResult,
    SimulationSummary,
    SimulationWarning,
)
from .validation import validate_simulation_config


@dataclass(frozen=True)
class GroupObservation:
    state: CellGroupState
    terminal_voltage_v: float
    open_circuit_voltage_v: float
    heat_w: float
    reversible_heat_w: float
    effective_resistance_ohm: float
    balance_current_a: float
    reported_soc: float
    fault_flags: list[str]
    hysteresis_voltage_v: float
    diffusion_stress_v: float
    degradation_rate_indicator: float = 0.0


@dataclass(frozen=True)
class GroupAdvanceResult:
    next_state: CellGroupState
    degradation_rate_indicator: float
    thermal_substep_count: int


def _group_cutoff_voltage_v(config: SimulationConfig, series_factor: float) -> float:
    return config.cell_cutoff_voltage * series_factor


def _group_effective_resistance_ohm(
    *,
    group: CellGroupState,
    config: SimulationConfig,
    group_base_resistance_ohm: float,
    current_a: float,
    resistance_multiplier: float,
    electrical_state,
) -> float:
    r0_ohm = (
        effective_r0_ohm(config, group_base_resistance_ohm)
        * effective_resistance_scale(group)
        * resistance_multiplier
    )
    r0_ohm *= soc_resistance_multiplier(config, group.soc)
    r0_ohm *= current_direction_resistance_multiplier(current_a, config)
    r0_ohm = temperature_adjusted_resistance_ohm(r0_ohm, group.core_temp_c, config) + interconnect_resistance_ohm(config)
    r0_ohm *= diffusion_stress_resistance_multiplier(electrical_state, config)
    return r0_ohm


def _observe_group(
    *,
    group: CellGroupState,
    config: SimulationConfig,
    fault_time_s: int,
    group_voltage_scale: float,
    group_base_resistance_ohm: float,
    pack_current_a: float,
    balance_current_a: float,
    degradation_rate_indicator: float = 0.0,
) -> GroupObservation:
    fault_effects = effects_for_group(config.faults, group.index, fault_time_s)
    current_a = pack_current_a + balance_current_a
    r0_ohm = _group_effective_resistance_ohm(
        group=group,
        config=config,
        group_base_resistance_ohm=group_base_resistance_ohm,
        current_a=current_a,
        resistance_multiplier=fault_effects.resistance_multiplier,
        electrical_state=group.electrical_state,
    )
    open_circuit_voltage_v = compute_open_circuit_voltage(
        soc=group.soc,
        config=config,
        group_voltage_scale=group_voltage_scale,
    )
    terminal_voltage_v = compute_terminal_voltage(
        open_circuit_voltage_v=open_circuit_voltage_v,
        current_a=current_a,
        r0_ohm=r0_ohm,
        state=group.electrical_state,
    )
    reversible_heat_w = compute_reversible_heat_w(current_a, group.core_temp_c, group.soc, config)
    heat_w = (compute_heat_w(current_a, r0_ohm) + reversible_heat_w) * fault_effects.heat_multiplier

    return GroupObservation(
        state=group,
        terminal_voltage_v=terminal_voltage_v,
        open_circuit_voltage_v=open_circuit_voltage_v,
        heat_w=heat_w,
        reversible_heat_w=reversible_heat_w,
        effective_resistance_ohm=r0_ohm,
        balance_current_a=balance_current_a,
        reported_soc=reported_soc_for_group(config.faults, group.index, group.soc, fault_time_s),
        fault_flags=list(fault_effects.flags),
        hysteresis_voltage_v=group.electrical_state.hysteresis_voltage_v,
        diffusion_stress_v=group.electrical_state.diffusion_stress_v,
        degradation_rate_indicator=degradation_rate_indicator,
    )


def _observe_groups(
    *,
    groups: list[CellGroupState],
    config: SimulationConfig,
    fault_time_s: int,
    group_voltage_scale: float,
    group_base_resistance_ohm: float,
    pack_current_a: float,
    balance_currents_a: list[float],
    degradation_rate_indicators: list[float] | None = None,
) -> list[GroupObservation]:
    return [
        _observe_group(
            group=group,
            config=config,
            fault_time_s=fault_time_s,
            group_voltage_scale=group_voltage_scale,
            group_base_resistance_ohm=group_base_resistance_ohm,
            pack_current_a=pack_current_a,
            balance_current_a=balance_currents_a[group.index],
            degradation_rate_indicator=(
                degradation_rate_indicators[group.index]
                if degradation_rate_indicators is not None
                else 0.0
            ),
        )
        for group in groups
    ]


def _advance_group(
    *,
    group: CellGroupState,
    config: SimulationConfig,
    fault_time_s: int,
    dt_s: int,
    group_voltage_scale: float,
    group_base_resistance_ohm: float,
    group_capacity_as: float,
    thermal_mass_j_per_k: float,
    pack_current_a: float,
    balance_current_a: float,
    left_neighbor_temp_c: float | None,
    right_neighbor_temp_c: float | None,
) -> GroupAdvanceResult:
    fault_effects = effects_for_group(config.faults, group.index, fault_time_s)
    thermal_context = resolve_group_thermal_context(config, group.index)
    current_a = pack_current_a + balance_current_a
    next_electrical_state = step_electrical_state(
        current_a=current_a,
        dt_s=dt_s,
        model=config.electrical_model,
        state=group.electrical_state,
        series_factor=group_voltage_scale,
        parallel_count=config.cells_in_parallel,
        soc=group.soc,
        temp_c=group.core_temp_c,
        config=config,
    )
    interval_resistance_ohm = _group_effective_resistance_ohm(
        group=group,
        config=config,
        group_base_resistance_ohm=group_base_resistance_ohm,
        current_a=current_a,
        resistance_multiplier=fault_effects.resistance_multiplier,
        electrical_state=next_electrical_state,
    )
    reversible_heat_w = compute_reversible_heat_w(current_a, group.core_temp_c, group.soc, config)
    heat_w = (compute_heat_w(current_a, interval_resistance_ohm) + reversible_heat_w) * fault_effects.heat_multiplier
    thermal_step = compute_next_group_temperatures(
        core_temp_c=group.core_temp_c,
        surface_temp_c=group.surface_temp_c,
        heat_w=heat_w,
        ambient_temp_c=thermal_context.ambient_temp_c,
        cooling_coeff_w_per_k=thermal_context.cooling_coeff_w_per_k * fault_effects.cooling_multiplier,
        thermal_mass_j_per_k=thermal_mass_j_per_k,
        dt_s=dt_s,
        config=config,
        left_neighbor_surface_temp_c=left_neighbor_temp_c,
        right_neighbor_surface_temp_c=right_neighbor_temp_c,
    )
    degradation_result = step_degradation(
        state=group.degradation,
        config=config.degradation,
        current_a=current_a,
        dt_s=dt_s,
        temp_c=thermal_step.core_temp_c,
        soc=group.soc,
    )
    next_soc = compute_next_soc(
        current_soc=group.soc,
        current_a=current_a,
        dt_s=dt_s,
        capacity_as=(
            group_capacity_as
            * effective_capacity_scale(group)
            * capacity_temperature_scale(thermal_step.core_temp_c, config)
            * fault_effects.capacity_multiplier
        ),
        config=config,
    )

    return GroupAdvanceResult(
        next_state=CellGroupState(
            index=group.index,
            soc=next_soc,
            temp_c=thermal_step.representative_temp_c,
            core_temp_c=thermal_step.core_temp_c,
            surface_temp_c=thermal_step.surface_temp_c,
            resistance_scale=group.resistance_scale,
            capacity_scale=group.capacity_scale,
            electrical_state=next_electrical_state,
            degradation=degradation_result.next_state,
        ),
        degradation_rate_indicator=degradation_result.capacity_loss_increment / max(dt_s / 3600.0, 1e-9),
        thermal_substep_count=thermal_step.substep_count,
    )


def _build_time_point(
    *,
    config: SimulationConfig,
    time_s: int,
    current_a: float,
    group_observations: list[GroupObservation],
) -> SimulationPoint:
    group_voltages_v = [observation.terminal_voltage_v for observation in group_observations]
    group_heats_w = [observation.heat_w for observation in group_observations]
    group_temps_c = [observation.state.temp_c for observation in group_observations]
    group_core_temps_c = [observation.state.core_temp_c for observation in group_observations]
    group_surface_temps_c = [observation.state.surface_temp_c for observation in group_observations]
    group_true_socs = [observation.state.soc for observation in group_observations]
    group_reported_socs = [observation.reported_soc for observation in group_observations]
    group_balance_currents_a = [observation.balance_current_a for observation in group_observations]
    group_fault_flags = [observation.fault_flags for observation in group_observations]
    group_hysteresis_v = [observation.hysteresis_voltage_v for observation in group_observations]
    group_diffusion_stress = [observation.diffusion_stress_v for observation in group_observations]
    group_effective_resistance_ohm = [observation.effective_resistance_ohm for observation in group_observations]
    group_heat_w = [observation.heat_w for observation in group_observations]
    group_zone_ids = list(config.group_zone_assignments) if config.group_zone_assignments else [0 for _ in group_observations]
    group_labels = list(config.group_labels) if config.group_labels else [f"Group {index}" for index in range(len(group_observations))]
    group_entity_ids = list(config.group_entity_ids) if config.group_entity_ids else [f"group-{index}" for index in range(len(group_observations))]
    degradation_rate_indicator = mean([observation.degradation_rate_indicator for observation in group_observations])

    weakest_group_index = min(range(len(group_voltages_v)), key=lambda index: group_voltages_v[index])
    hottest_group_index = max(range(len(group_temps_c)), key=lambda index: group_temps_c[index])
    pack_voltage_v = sum(group_voltages_v)
    pack_heat_w = sum(group_heats_w)
    pack_power_w = compute_power_w(pack_voltage_v, current_a)
    pack_temp_max_c = max(group_temps_c)
    pack_temp_avg_c = mean(group_temps_c)
    soc_min = min(group_true_socs)
    soc_max = max(group_true_socs)
    soc_avg = mean(group_true_socs)
    zone_temp_max_c: dict[int, float] = {}
    for zone_id, temp_c in zip(group_zone_ids, group_temps_c):
        zone_temp_max_c[zone_id] = max(zone_temp_max_c.get(zone_id, temp_c), temp_c)

    return SimulationPoint(
        time_s=time_s,
        soc=soc_avg,
        terminal_voltage_v=pack_voltage_v,
        power_w=pack_power_w,
        heat_w=pack_heat_w,
        temp_c=pack_temp_max_c,
        current_a=current_a,
        pack_voltage_v=pack_voltage_v,
        pack_power_w=pack_power_w,
        pack_heat_w=pack_heat_w,
        pack_temp_max_c=pack_temp_max_c,
        pack_temp_avg_c=pack_temp_avg_c,
        soc_min=soc_min,
        soc_max=soc_max,
        soc_avg=soc_avg,
        temp_avg=pack_temp_avg_c,
        temp_max=pack_temp_max_c,
        group_voltage_min_v=min(group_voltages_v),
        group_voltage_max_v=max(group_voltages_v),
        weakest_group_index=weakest_group_index,
        hottest_group_index=hottest_group_index,
        group_soc=group_reported_socs,
        group_true_soc=group_true_socs,
        group_voltage=group_voltages_v,
        group_temp=group_temps_c,
        group_core_temp=group_core_temps_c,
        group_surface_temp=group_surface_temps_c,
        group_core_temp_c=group_core_temps_c,
        group_surface_temp_c=group_surface_temps_c,
        group_hysteresis_v=group_hysteresis_v,
        group_diffusion_stress=group_diffusion_stress,
        group_effective_resistance_ohm=group_effective_resistance_ohm,
        group_heat_w=group_heat_w,
        balancing_active_groups=[index for index, value in enumerate(group_balance_currents_a) if value > 0.0],
        fault_active_groups=[index for index, flags in enumerate(group_fault_flags) if flags],
        group_balance_current_a=group_balance_currents_a,
        group_fault_flags=group_fault_flags,
        group_zone_ids=group_zone_ids,
        group_labels=group_labels,
        group_entity_ids=group_entity_ids,
        zone_temp_max_c=zone_temp_max_c,
        estimated_capacity_retention=mean(
            max(1.0 - observation.state.degradation.capacity_loss_fraction, 0.0)
            for observation in group_observations
        ),
        estimated_resistance_multiplier=mean(
            1.0 + observation.state.degradation.resistance_growth_fraction
            for observation in group_observations
        ),
        degradation_rate_indicator=degradation_rate_indicator,
    )


def run_simulation(config: SimulationConfig) -> SimulationResult:
    chemistry_config = apply_chemistry_defaults(config)
    validated_config = validate_simulation_config(
        SimulationConfig(
            **{
                **chemistry_config.__dict__,
                "electrical_model": validate_electrical_model(chemistry_config.electrical_model),
            }
        )
    )
    pack = derive_pack_properties(validated_config)
    groups = build_group_states(validated_config, pack)
    group_cutoff_voltage_v = _group_cutoff_voltage_v(validated_config, int(pack.series_factor))
    thermal_mass_j_per_k = (
        validated_config.pack_mass_kg * validated_config.pack_heat_capacity_j_per_kgk / pack.group_count
    )

    current_time_s = 0
    current_a = current_for_time(validated_config.current_profile, validated_config.discharge_current_a, current_time_s)
    balance_currents_a = balance_currents_for_groups(
        groups=groups,
        config=validated_config,
        group_voltage_scale=pack.series_factor,
        time_s=current_time_s,
    )
    current_observations = _observe_groups(
        groups=groups,
        config=validated_config,
        fault_time_s=current_time_s,
        group_voltage_scale=pack.series_factor,
        group_base_resistance_ohm=pack.group_base_resistance_ohm,
        pack_current_a=current_a,
        balance_currents_a=balance_currents_a,
    )

    time_series: list[SimulationPoint] = [
        _build_time_point(
            config=validated_config,
            time_s=current_time_s,
            current_a=current_a,
            group_observations=current_observations,
        )
    ]
    metric_points: list[SimulationPoint] = [time_series[0]]
    termination_reason = "duration_elapsed"
    total_balance_ah = 0.0
    delivered_capacity_ah = 0.0
    delivered_energy_wh = 0.0
    cumulative_charge_throughput_ah = 0.0
    cumulative_discharge_throughput_ah = 0.0
    cumulative_high_soc_time_s = 0.0
    max_thermal_substeps = 1

    while current_time_s < validated_config.duration_s:
        interval_dt_s = min(validated_config.time_step_s, validated_config.duration_s - current_time_s)
        interval_start_point = time_series[-1]
        advance_results = [
            _advance_group(
                group=group,
                config=validated_config,
                fault_time_s=current_time_s,
                dt_s=interval_dt_s,
                group_voltage_scale=pack.series_factor,
                group_base_resistance_ohm=pack.group_base_resistance_ohm,
                group_capacity_as=pack.group_capacity_as,
                thermal_mass_j_per_k=thermal_mass_j_per_k,
                pack_current_a=current_a,
                balance_current_a=balance_currents_a[group.index],
                left_neighbor_temp_c=groups[group.index - 1].surface_temp_c if group.index > 0 else None,
                right_neighbor_temp_c=groups[group.index + 1].surface_temp_c if group.index + 1 < len(groups) else None,
            )
            for group in groups
        ]
        next_groups = [result.next_state for result in advance_results]
        degradation_rate_indicators = [result.degradation_rate_indicator for result in advance_results]
        max_thermal_substeps = max(
            max_thermal_substeps,
            max((result.thermal_substep_count for result in advance_results), default=1),
        )
        next_time_s = current_time_s + interval_dt_s

        interval_end_observations = _observe_groups(
            groups=next_groups,
            config=validated_config,
            fault_time_s=current_time_s,
            group_voltage_scale=pack.series_factor,
            group_base_resistance_ohm=pack.group_base_resistance_ohm,
            pack_current_a=current_a,
            balance_currents_a=balance_currents_a,
            degradation_rate_indicators=degradation_rate_indicators,
        )
        interval_end_point = _build_time_point(
            config=validated_config,
            time_s=next_time_s,
            current_a=current_a,
            group_observations=interval_end_observations,
        )
        metric_points.append(interval_end_point)

        delivered_capacity_ah += current_a * interval_dt_s / 3600.0
        delivered_energy_wh += (
            (interval_start_point.pack_power_w + interval_end_point.pack_power_w) * 0.5 * interval_dt_s / 3600.0
        )
        total_balance_ah += sum(balance_currents_a) * interval_dt_s / 3600.0
        cumulative_charge_throughput_ah += abs(min(current_a, 0.0)) * interval_dt_s / 3600.0
        cumulative_discharge_throughput_ah += max(current_a, 0.0) * interval_dt_s / 3600.0
        if any(
            before.soc >= validated_config.degradation.high_soc_threshold
            or after.soc >= validated_config.degradation.high_soc_threshold
            for before, after in zip(groups, next_groups)
        ):
            cumulative_high_soc_time_s += interval_dt_s

        groups = next_groups
        current_time_s = next_time_s
        current_a = current_for_time(validated_config.current_profile, validated_config.discharge_current_a, current_time_s)
        balance_currents_a = balance_currents_for_groups(
            groups=groups,
            config=validated_config,
            group_voltage_scale=pack.series_factor,
            time_s=current_time_s,
        )
        current_observations = _observe_groups(
            groups=groups,
            config=validated_config,
            fault_time_s=current_time_s,
            group_voltage_scale=pack.series_factor,
            group_base_resistance_ohm=pack.group_base_resistance_ohm,
            pack_current_a=current_a,
            balance_currents_a=balance_currents_a,
            degradation_rate_indicators=degradation_rate_indicators,
        )
        time_series.append(
            _build_time_point(
                config=validated_config,
                time_s=current_time_s,
                current_a=current_a,
                group_observations=current_observations,
            )
        )

        if interval_end_point.group_voltage_min_v <= group_cutoff_voltage_v:
            termination_reason = "group_cutoff_voltage_reached"
            break
        if interval_end_point.soc_min <= 0.0 and interval_start_point.current_a > 0.0:
            termination_reason = "soc_depleted"
            break
        if interval_end_point.soc_max >= 1.0 and interval_start_point.current_a < 0.0:
            termination_reason = "soc_ceiling_reached"
            break

    summary = build_summary(
        config=validated_config,
        time_series=time_series,
        metric_points=metric_points,
        groups=groups,
        theoretical_energy_wh=pack.theoretical_energy_wh,
        group_cutoff_voltage_v=group_cutoff_voltage_v,
        termination_reason=termination_reason,
        total_balance_ah=total_balance_ah,
        delivered_capacity_ah=delivered_capacity_ah,
        delivered_energy_wh=delivered_energy_wh,
        cumulative_charge_throughput_ah=cumulative_charge_throughput_ah,
        cumulative_discharge_throughput_ah=cumulative_discharge_throughput_ah,
        cumulative_high_soc_time_s=cumulative_high_soc_time_s,
        fault_count=len(validated_config.faults.faults),
        balancing_enabled=validated_config.balancing.enabled,
        max_thermal_substeps=max_thermal_substeps,
    )

    return SimulationResult(
        pack_nominal_voltage_v=pack.nominal_voltage_v,
        pack_capacity_ah=pack.capacity_ah,
        theoretical_energy_wh=pack.theoretical_energy_wh,
        summary=summary,
        time_series=time_series,
    )


def build_summary(
    *,
    config: SimulationConfig,
    time_series: list[SimulationPoint],
    metric_points: list[SimulationPoint],
    groups: list[CellGroupState],
    theoretical_energy_wh: float,
    group_cutoff_voltage_v: float,
    termination_reason: str,
    total_balance_ah: float,
    delivered_capacity_ah: float,
    delivered_energy_wh: float,
    cumulative_charge_throughput_ah: float,
    cumulative_discharge_throughput_ah: float,
    cumulative_high_soc_time_s: float,
    fault_count: int,
    balancing_enabled: bool,
    max_thermal_substeps: int,
) -> SimulationSummary:
    if not time_series:
        return SimulationSummary(
            runtime_s=0,
            delivered_energy_wh=0.0,
            delivered_capacity_ah=0.0,
            min_terminal_voltage_v=0.0,
            peak_temp_c=config.ambient_temp_c,
            final_soc=config.initial_soc,
            termination_reason="no_steps",
            warnings=[],
            final_soc_avg=config.initial_soc,
            soc_spread=0.0,
            max_group_temp_c=config.ambient_temp_c,
            min_group_voltage_v=0.0,
            estimated_capacity_retention=1.0,
            estimated_resistance_growth=0.0,
            electrical_model_type=config.electrical_model.model_type,
            profile_used=config.current_profile is not None and bool(config.current_profile.points),
            total_energy_wh=0.0,
            weakest_group_index=0,
            hottest_group_index=0,
            max_group_temp=config.ambient_temp_c,
            min_group_voltage=0.0,
            capacity_retention=1.0,
            resistance_growth=0.0,
            balancing_used=False,
            total_balance_ah=0.0,
            fault_count=fault_count,
            first_faulted_group_index=None,
            hottest_zone_id=0,
            max_zone_temp_c=config.ambient_temp_c,
            cumulative_charge_throughput_ah=0.0,
            cumulative_discharge_throughput_ah=0.0,
            cumulative_high_soc_time_h=0.0,
            estimated_cycle_stress=0.0,
            degradation_model_version="v2-pragmatic",
            group_capacity_retention=[],
            group_resistance_growth=[],
            max_core_temp_c=config.ambient_temp_c,
            max_surface_temp_c=config.ambient_temp_c,
            temp_gradient_max_c=0.0,
            max_diffusion_stress=0.0,
            nonlinear_features_enabled=[],
            chemistry_name=config.chemistry_name,
        )

    runtime_s = time_series[-1].time_s
    min_terminal_voltage_v = min(point.pack_voltage_v for point in metric_points)
    min_group_voltage_v = min(point.group_voltage_min_v for point in metric_points)
    max_group_temp_c = max(point.pack_temp_max_c for point in metric_points)
    max_core_temp_c = max((max(point.group_core_temp) for point in metric_points), default=config.ambient_temp_c)
    max_surface_temp_c = max((max(point.group_surface_temp) for point in metric_points), default=config.ambient_temp_c)
    temp_gradient_max_c = max(
        (max(abs(core - surface) for core, surface in zip(point.group_core_temp, point.group_surface_temp)) for point in metric_points),
        default=0.0,
    )
    max_diffusion_stress = max((max(point.group_diffusion_stress) for point in time_series), default=0.0)
    weakest_group_index = min(
        (
            (voltage, index)
            for point in metric_points
            for index, voltage in enumerate(point.group_voltage)
        ),
        key=lambda item: item[0],
    )[1]
    hottest_group_index = max(
        (
            (temp_c, index)
            for point in metric_points
            for index, temp_c in enumerate(point.group_temp)
        ),
        key=lambda item: item[0],
    )[1]
    final_soc_avg = mean(group.soc for group in groups)
    soc_spread = max((group.soc for group in groups), default=config.initial_soc) - min(
        (group.soc for group in groups),
        default=config.initial_soc,
    )
    estimated_capacity_retention = mean(
        max(1.0 - group.degradation.capacity_loss_fraction, 0.0) for group in groups
    )
    estimated_resistance_growth = mean(group.degradation.resistance_growth_fraction for group in groups)
    estimated_cycle_stress = mean(group.degradation.cumulative_cycle_stress for group in groups)
    group_capacity_retention = [max(1.0 - group.degradation.capacity_loss_fraction, 0.0) for group in groups]
    group_resistance_growth = [group.degradation.resistance_growth_fraction for group in groups]
    first_faulted_group_index = next(
        (point.fault_active_groups[0] for point in time_series if point.fault_active_groups),
        None,
    )
    hottest_zone_id = 0
    max_zone_temp_c = config.ambient_temp_c
    for point in metric_points:
        for zone_id, temp_c in point.zone_temp_max_c.items():
            if temp_c >= max_zone_temp_c:
                max_zone_temp_c = temp_c
                hottest_zone_id = zone_id

    warnings: list[SimulationWarning] = []
    peak_thermal_temp_c = max(max_core_temp_c, max_surface_temp_c)
    if peak_thermal_temp_c >= 60.0:
        warnings.append(
            SimulationWarning(
                code="thermal_limit_exceeded",
                message="Peak core or surface temperature exceeded 60 C.",
                severity="high",
            )
        )
    elif peak_thermal_temp_c >= 45.0:
        warnings.append(
            SimulationWarning(
                code="thermal_margin_low",
                message="Peak core or surface temperature exceeded 45 C.",
                severity="medium",
            )
        )

    if max_thermal_substeps >= 4:
        warnings.append(
            SimulationWarning(
                code="thermal_timestep_coarse",
                message="Thermal timestep is coarse relative to pack thermal dynamics; internal substepping was applied and fine detail may be under-resolved.",
                severity="medium" if max_thermal_substeps >= 16 else "low",
            )
        )

    if termination_reason == "group_cutoff_voltage_reached":
        warnings.append(
            SimulationWarning(
                code="cutoff_voltage_reached",
                message="Simulation stopped because the weakest group hit cutoff voltage.",
                severity="medium",
            )
        )
    elif min_group_voltage_v <= group_cutoff_voltage_v + 0.1:
        warnings.append(
            SimulationWarning(
                code="low_voltage_margin",
                message="Weakest group voltage approached the cutoff threshold.",
                severity="low",
            )
        )

    if soc_spread >= 0.05:
        warnings.append(
            SimulationWarning(
                code="soc_imbalance_detected",
                message="Group SOC spread exceeded 5%.",
                severity="low",
            )
        )

    initial_soc_avg = time_series[0].soc_avg
    usable_energy_reference_wh = theoretical_energy_wh * max(initial_soc_avg, 0.0)
    meaningful_discharge_attempt = (
        delivered_energy_wh > 0.0
        and initial_soc_avg >= 0.7
        and (
            termination_reason in {"group_cutoff_voltage_reached", "soc_depleted"}
            or final_soc_avg <= 0.1
            or time_series[-1].soc_min <= 0.05
        )
    )
    usable_energy_ratio = delivered_energy_wh / usable_energy_reference_wh if usable_energy_reference_wh else 0.0
    if meaningful_discharge_attempt and usable_energy_ratio < 0.8:
        warnings.append(
            SimulationWarning(
                code="low_usable_energy",
                message="Delivered energy was significantly below expected usable energy for the attempted discharge window.",
                severity="low",
            )
        )

    return SimulationSummary(
        runtime_s=runtime_s,
        delivered_energy_wh=delivered_energy_wh,
        delivered_capacity_ah=delivered_capacity_ah,
        min_terminal_voltage_v=min_terminal_voltage_v,
        peak_temp_c=max_group_temp_c,
        final_soc=final_soc_avg,
        termination_reason=termination_reason,
        warnings=warnings,
        final_soc_avg=final_soc_avg,
        soc_spread=soc_spread,
        max_group_temp_c=max_group_temp_c,
        min_group_voltage_v=min_group_voltage_v,
        estimated_capacity_retention=estimated_capacity_retention,
        estimated_resistance_growth=estimated_resistance_growth,
        electrical_model_type=config.electrical_model.model_type,
        profile_used=config.current_profile is not None and bool(config.current_profile.points),
        total_energy_wh=delivered_energy_wh,
        weakest_group_index=weakest_group_index,
        hottest_group_index=hottest_group_index,
        max_group_temp=max_group_temp_c,
        min_group_voltage=min_group_voltage_v,
        capacity_retention=estimated_capacity_retention,
        resistance_growth=estimated_resistance_growth,
        balancing_used=balancing_enabled and total_balance_ah > 0.0,
        total_balance_ah=total_balance_ah,
        fault_count=fault_count,
        first_faulted_group_index=first_faulted_group_index,
        hottest_zone_id=hottest_zone_id,
        max_zone_temp_c=max_zone_temp_c,
        cumulative_charge_throughput_ah=cumulative_charge_throughput_ah,
        cumulative_discharge_throughput_ah=cumulative_discharge_throughput_ah,
        cumulative_high_soc_time_h=cumulative_high_soc_time_s / 3600.0,
        estimated_cycle_stress=estimated_cycle_stress,
        degradation_model_version="v2-pragmatic",
        group_capacity_retention=group_capacity_retention,
        group_resistance_growth=group_resistance_growth,
        max_core_temp_c=max_core_temp_c,
        max_surface_temp_c=max_surface_temp_c,
        temp_gradient_max_c=temp_gradient_max_c,
        max_diffusion_stress=max_diffusion_stress,
        nonlinear_features_enabled=[
            name
            for enabled, name in (
                (config.physics.resistance_vs_soc_enabled, "soc_dependent_resistance"),
                (config.physics.hysteresis_enabled, "hysteresis"),
                (config.physics.rc_state_dependence_enabled, "state_dependent_rc"),
                (config.physics.diffusion_stress_enabled, "diffusion_stress"),
                (config.physics.two_node_thermal_enabled, "two_node_thermal"),
                (config.physics.nonlinear_cooling_enabled, "nonlinear_cooling"),
                (config.physics.reversible_heat_enabled, "reversible_heat"),
                (
                    config.physics.charge_resistance_multiplier != 1.0 or config.physics.discharge_resistance_multiplier != 1.0,
                    "current_direction_asymmetry",
                ),
            )
            if enabled
        ],
        chemistry_name=config.chemistry_name,
    )
