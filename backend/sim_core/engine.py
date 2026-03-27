from __future__ import annotations

from .physics.electrical import (
    compute_heat_w,
    compute_next_soc,
    compute_open_circuit_voltage,
    compute_power_w,
    compute_terminal_voltage,
)
from .physics.pack import derive_pack_properties
from .physics.thermal import compute_next_temperature_c
from .types import (
    SimulationConfig,
    SimulationPoint,
    SimulationResult,
    SimulationSummary,
    SimulationWarning,
)


def run_simulation(config: SimulationConfig) -> SimulationResult:
    pack = derive_pack_properties(config)
    total_steps = config.duration_s // config.time_step_s

    cutoff_voltage_v = config.cell_cutoff_voltage * config.cells_in_series
    soc = config.initial_soc
    temp_c = config.ambient_temp_c
    time_series: list[SimulationPoint] = []
    termination_reason = "duration_elapsed"

    for step in range(total_steps + 1):
        time_s = step * config.time_step_s

        open_circuit_voltage_v = compute_open_circuit_voltage(soc=soc, config=config, pack=pack)
        terminal_voltage_v = compute_terminal_voltage(
            open_circuit_voltage_v=open_circuit_voltage_v,
            current_a=config.discharge_current_a,
            pack_resistance_ohm=pack.resistance_ohm,
        )
        power_w = compute_power_w(
            terminal_voltage_v=terminal_voltage_v,
            current_a=config.discharge_current_a,
        )
        heat_w = compute_heat_w(
            current_a=config.discharge_current_a,
            pack_resistance_ohm=pack.resistance_ohm,
        )
        temp_c = compute_next_temperature_c(
            temp_c=temp_c,
            heat_w=heat_w,
            config=config,
        )

        time_series.append(
            SimulationPoint(
                time_s=time_s,
                soc=max(soc, 0.0),
                terminal_voltage_v=terminal_voltage_v,
                power_w=power_w,
                heat_w=heat_w,
                temp_c=temp_c,
            )
        )

        soc = compute_next_soc(
            current_soc=soc,
            config=config,
            capacity_as=pack.capacity_as,
        )
        if terminal_voltage_v <= cutoff_voltage_v:
            termination_reason = "cutoff_voltage_reached"
            break
        if soc <= 0:
            termination_reason = "soc_depleted"
            break

    summary = build_summary(
        config=config,
        time_series=time_series,
        theoretical_energy_wh=pack.theoretical_energy_wh,
        cutoff_voltage_v=cutoff_voltage_v,
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
    theoretical_energy_wh: float,
    cutoff_voltage_v: float,
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
        )

    runtime_s = time_series[-1].time_s
    delivered_capacity_ah = config.discharge_current_a * runtime_s / 3600.0
    delivered_energy_wh = sum(
        point.power_w * config.time_step_s / 3600.0 for point in time_series[:-1]
    )
    min_terminal_voltage_v = min(point.terminal_voltage_v for point in time_series)
    peak_temp_c = max(point.temp_c for point in time_series)
    final_soc = time_series[-1].soc

    warnings: list[SimulationWarning] = []
    if peak_temp_c >= 60.0:
        warnings.append(
            SimulationWarning(
                code="thermal_limit_exceeded",
                message="Peak pack temperature exceeded 60 C.",
                severity="high",
            )
        )
    elif peak_temp_c >= 45.0:
        warnings.append(
            SimulationWarning(
                code="thermal_margin_low",
                message="Peak pack temperature exceeded 45 C.",
                severity="medium",
            )
        )

    voltage_margin_v = max(min_terminal_voltage_v - cutoff_voltage_v, 0.0)
    if termination_reason == "cutoff_voltage_reached":
        warnings.append(
            SimulationWarning(
                code="cutoff_voltage_reached",
                message="Simulation stopped because pack voltage reached cutoff.",
                severity="medium",
            )
        )
    elif voltage_margin_v <= 0.1 * config.cells_in_series:
        warnings.append(
            SimulationWarning(
                code="low_voltage_margin",
                message="Minimum terminal voltage approached the cutoff threshold.",
                severity="low",
            )
        )

    usable_energy_ratio = delivered_energy_wh / theoretical_energy_wh if theoretical_energy_wh else 0.0
    if usable_energy_ratio < 0.8:
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
        peak_temp_c=peak_temp_c,
        final_soc=final_soc,
        termination_reason=termination_reason,
        warnings=warnings,
    )
