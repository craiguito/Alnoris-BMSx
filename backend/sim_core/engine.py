from __future__ import annotations

from statistics import mean

from .physics.degradation import effective_capacity_scale, effective_resistance_scale, step_degradation
from .physics.electrical import (
    compute_heat_w,
    compute_next_soc,
    compute_open_circuit_voltage,
    compute_power_w,
    compute_terminal_voltage,
    effective_r0_ohm,
    step_electrical_state,
    validate_electrical_model,
)
from .physics.pack import build_group_states, derive_pack_properties
from .physics.thermal import compute_next_temperature_c
from .profiles import current_for_time
from .types import (
    CellGroupState,
    GroupStepResult,
    SimulationConfig,
    SimulationPoint,
    SimulationResult,
    SimulationSummary,
    SimulationWarning,
)


def _group_cutoff_voltage_v(config: SimulationConfig, series_factor: float) -> float:
    return config.cell_cutoff_voltage * series_factor


def _step_group(
    group: CellGroupState,
    config: SimulationConfig,
    group_voltage_scale: float,
    group_base_resistance_ohm: float,
    group_capacity_as: float,
    thermal_mass_j_per_k: float,
    current_a: float,
) -> GroupStepResult:
    r0_ohm = effective_r0_ohm(config, group_base_resistance_ohm) * effective_resistance_scale(group)
    open_circuit_voltage_v = compute_open_circuit_voltage(
        soc=group.soc,
        config=config,
        group_voltage_scale=group_voltage_scale,
    )
    next_electrical_state = step_electrical_state(
        current_a=current_a,
        dt_s=config.time_step_s,
        model=config.electrical_model,
        state=group.electrical_state,
        series_factor=group_voltage_scale,
        parallel_count=config.cells_in_parallel,
    )
    terminal_voltage_v = compute_terminal_voltage(
        open_circuit_voltage_v=open_circuit_voltage_v,
        current_a=current_a,
        r0_ohm=r0_ohm,
        state=next_electrical_state,
    )
    heat_w = compute_heat_w(
        current_a=current_a,
        r0_ohm=r0_ohm,
        model=config.electrical_model,
        series_factor=group_voltage_scale,
        parallel_count=config.cells_in_parallel,
    )
    next_temp_c = compute_next_temperature_c(
        temp_c=group.temp_c,
        heat_w=heat_w,
        ambient_temp_c=config.ambient_temp_c,
        cooling_coeff_w_per_k=config.cooling_coeff_w_per_k,
        thermal_mass_j_per_k=thermal_mass_j_per_k,
        dt_s=config.time_step_s,
    )
    next_degradation = step_degradation(
        state=group.degradation,
        config=config.degradation,
        current_a=current_a,
        dt_s=config.time_step_s,
        temp_c=next_temp_c,
        soc=group.soc,
    )
    next_soc = compute_next_soc(
        current_soc=group.soc,
        current_a=current_a,
        dt_s=config.time_step_s,
        capacity_as=group_capacity_as * effective_capacity_scale(group),
    )

    return GroupStepResult(
        terminal_voltage_v=terminal_voltage_v,
        open_circuit_voltage_v=open_circuit_voltage_v,
        heat_w=heat_w,
        next_state=CellGroupState(
            index=group.index,
            soc=next_soc,
            temp_c=next_temp_c,
            resistance_scale=group.resistance_scale,
            capacity_scale=group.capacity_scale,
            electrical_state=next_electrical_state,
            degradation=next_degradation,
        ),
    )


def _build_time_point(
    time_s: int,
    current_a: float,
    group_step_results: list[GroupStepResult],
) -> SimulationPoint:
    group_voltages_v = [result.terminal_voltage_v for result in group_step_results]
    group_heats_w = [result.heat_w for result in group_step_results]
    group_temps_c = [result.next_state.temp_c for result in group_step_results]
    group_socs = [result.next_state.soc for result in group_step_results]
    weakest_group_index = min(
        range(len(group_voltages_v)),
        key=lambda index: group_voltages_v[index],
    )

    pack_voltage_v = sum(group_voltages_v)
    pack_heat_w = sum(group_heats_w)
    pack_power_w = compute_power_w(pack_voltage_v, current_a)
    pack_temp_max_c = max(group_temps_c)
    pack_temp_avg_c = mean(group_temps_c)
    soc_min = min(group_socs)
    soc_max = max(group_socs)
    soc_avg = mean(group_socs)

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
        group_voltage_min_v=min(group_voltages_v),
        group_voltage_max_v=max(group_voltages_v),
        weakest_group_index=weakest_group_index,
    )


def run_simulation(config: SimulationConfig) -> SimulationResult:
    validated_config = SimulationConfig(
        **{
            **config.__dict__,
            "electrical_model": validate_electrical_model(config.electrical_model),
        }
    )
    pack = derive_pack_properties(validated_config)
    groups = build_group_states(validated_config, pack)
    total_steps = validated_config.duration_s // validated_config.time_step_s
    group_cutoff_voltage_v = _group_cutoff_voltage_v(validated_config, pack.series_factor)
    thermal_mass_j_per_k = (
        validated_config.pack_mass_kg * validated_config.pack_heat_capacity_j_per_kgk / pack.group_count
    )

    time_series: list[SimulationPoint] = []
    termination_reason = "duration_elapsed"

    for step in range(total_steps + 1):
        time_s = step * validated_config.time_step_s
        current_a = current_for_time(validated_config.current_profile, validated_config.discharge_current_a, time_s)

        step_results = [
            _step_group(
                group=group,
                config=validated_config,
                group_voltage_scale=pack.series_factor,
                group_base_resistance_ohm=pack.group_base_resistance_ohm,
                group_capacity_as=pack.group_capacity_as,
                thermal_mass_j_per_k=thermal_mass_j_per_k,
                current_a=current_a,
            )
            for group in groups
        ]
        groups = [result.next_state for result in step_results]
        point = _build_time_point(time_s=time_s, current_a=current_a, group_step_results=step_results)
        time_series.append(point)

        if point.group_voltage_min_v <= group_cutoff_voltage_v:
            termination_reason = "group_cutoff_voltage_reached"
            break
        if point.soc_min <= 0.0 and current_a > 0.0:
            termination_reason = "soc_depleted"
            break
        if point.soc_max >= 1.0 and current_a < 0.0:
            termination_reason = "soc_ceiling_reached"
            break

    summary = build_summary(
        config=validated_config,
        time_series=time_series,
        groups=groups,
        theoretical_energy_wh=pack.theoretical_energy_wh,
        group_cutoff_voltage_v=group_cutoff_voltage_v,
        termination_reason=termination_reason,
    )

    return SimulationResult(
        pack_nominal_voltage_v=pack.nominal_voltage_v,
        pack_capacity_ah=pack.capacity_ah,
        theoretical_energy_wh=pack.theoretical_energy_wh,
        summary=summary,
        time_series=time_series,
    )


def build_summary(
    config: SimulationConfig,
    time_series: list[SimulationPoint],
    groups: list[CellGroupState],
    theoretical_energy_wh: float,
    group_cutoff_voltage_v: float,
    termination_reason: str,
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
        )

    runtime_s = time_series[-1].time_s
    delivered_capacity_ah = sum(point.current_a * config.time_step_s / 3600.0 for point in time_series[:-1])
    delivered_energy_wh = sum(point.pack_power_w * config.time_step_s / 3600.0 for point in time_series[:-1])
    min_terminal_voltage_v = min(point.pack_voltage_v for point in time_series)
    min_group_voltage_v = min(point.group_voltage_min_v for point in time_series)
    max_group_temp_c = max(point.pack_temp_max_c for point in time_series)
    final_soc_avg = time_series[-1].soc_avg
    soc_spread = time_series[-1].soc_max - time_series[-1].soc_min
    estimated_capacity_retention = mean(
        max(1.0 - group.degradation.capacity_loss_fraction, 0.0) for group in groups
    )
    estimated_resistance_growth = mean(group.degradation.resistance_growth_fraction for group in groups)

    warnings: list[SimulationWarning] = []
    if max_group_temp_c >= 60.0:
        warnings.append(
            SimulationWarning(
                code="thermal_limit_exceeded",
                message="Peak group temperature exceeded 60 C.",
                severity="high",
            )
        )
    elif max_group_temp_c >= 45.0:
        warnings.append(
            SimulationWarning(
                code="thermal_margin_low",
                message="Peak group temperature exceeded 45 C.",
                severity="medium",
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

    usable_energy_ratio = delivered_energy_wh / theoretical_energy_wh if theoretical_energy_wh else 0.0
    if delivered_energy_wh > 0.0 and usable_energy_ratio < 0.8:
        warnings.append(
            SimulationWarning(
                code="low_usable_energy",
                message="Delivered energy was significantly below theoretical energy.",
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
    )
