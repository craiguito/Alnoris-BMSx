from __future__ import annotations

import math

from ..chemistry import get_chemistry_preset
from ..types import (
    ElectricalModelConfig,
    GroupElectricalState,
    OcvLookupPoint,
    PackProperties,
    RcBranchParams,
    SocLookupPoint,
    SimulationConfig,
)


def interpolate_ocv_curve(points: tuple[OcvLookupPoint, ...], soc: float) -> float:
    clamped_soc = min(max(soc, 0.0), 1.0)
    if not points:
        raise ValueError("OCV interpolation requires at least one point.")
    if clamped_soc <= points[0].soc:
        return points[0].voltage_v
    if clamped_soc >= points[-1].soc:
        return points[-1].voltage_v
    for low, high in zip(points[:-1], points[1:]):
        if low.soc <= clamped_soc <= high.soc:
            span = max(high.soc - low.soc, 1e-9)
            fraction = (clamped_soc - low.soc) / span
            return low.voltage_v + fraction * (high.voltage_v - low.voltage_v)
    return points[-1].voltage_v


def compute_open_circuit_voltage(
    soc: float,
    config: SimulationConfig,
    group_voltage_scale: float = 1.0,
) -> float:
    if config.physics.ocv_curve:
        return interpolate_ocv_curve(config.physics.ocv_curve, soc) * group_voltage_scale

    chemistry = get_chemistry_preset(config.chemistry_name)
    if chemistry.ocv_curve:
        return interpolate_ocv_curve(chemistry.ocv_curve, soc) * group_voltage_scale

    empty_voltage_v = config.cell_empty_voltage * group_voltage_scale
    full_voltage_v = config.cell_full_voltage * group_voltage_scale
    nominal_voltage_v = config.cell_nominal_voltage * group_voltage_scale
    usable_span_v = max(full_voltage_v - empty_voltage_v, 1e-9)

    reference_fraction = (nominal_voltage_v - empty_voltage_v) / usable_span_v
    reference_fraction = min(max(reference_fraction, 1e-6), 1.0 - 1e-6)
    nominal_soc_anchor = 0.5
    ocv_exponent = math.log(reference_fraction) / math.log(nominal_soc_anchor)

    clamped_soc = min(max(soc, 0.0), 1.0)
    return empty_voltage_v + usable_span_v * (clamped_soc**ocv_exponent)


def init_rc_state(model: ElectricalModelConfig) -> GroupElectricalState:
    return GroupElectricalState(rc_branch_voltages_v=tuple(0.0 for _ in model.rc_branches))


def effective_r0_ohm(config: SimulationConfig, base_resistance_ohm: float) -> float:
    if config.electrical_model.r0_ohm_per_cell is None:
        return base_resistance_ohm

    group_count = max(1, config.group_count or config.cells_in_series)
    series_factor = config.cells_in_series // group_count
    return config.electrical_model.r0_ohm_per_cell * series_factor / max(config.cells_in_parallel, 1)


def interpolate_soc_curve(points: tuple[SocLookupPoint, ...], soc: float) -> float:
    clamped_soc = min(max(soc, 0.0), 1.0)
    if not points:
        return 1.0
    if clamped_soc <= points[0].soc:
        return points[0].multiplier
    if clamped_soc >= points[-1].soc:
        return points[-1].multiplier
    for low, high in zip(points[:-1], points[1:]):
        if low.soc <= clamped_soc <= high.soc:
            span = max(high.soc - low.soc, 1e-9)
            fraction = (clamped_soc - low.soc) / span
            return low.multiplier + fraction * (high.multiplier - low.multiplier)
    return points[-1].multiplier


def soc_resistance_multiplier(config: SimulationConfig, soc: float) -> float:
    if not config.physics.resistance_vs_soc_enabled:
        return 1.0
    return max(interpolate_soc_curve(config.physics.resistance_soc_curve, soc), 0.05)


def current_direction_resistance_multiplier(current_a: float, config: SimulationConfig) -> float:
    if current_a < 0.0:
        return config.physics.charge_resistance_multiplier
    if current_a > 0.0:
        return config.physics.discharge_resistance_multiplier
    return 1.0


def temperature_adjusted_resistance_ohm(
    base_resistance_ohm: float,
    temp_c: float,
    config: SimulationConfig,
) -> float:
    adjusted = base_resistance_ohm * (
        1.0 + config.physics.resistance_temperature_alpha_per_c * (temp_c - config.physics.resistance_reference_temp_c)
    )
    return max(adjusted, 1e-9)


def interconnect_resistance_ohm(config: SimulationConfig) -> float:
    group_count = max(1, config.group_count or config.cells_in_series)
    return (
        config.physics.interconnect_resistance_ohm_per_group
        + (config.physics.pack_interconnect_resistance_ohm / group_count)
    )


def capacity_temperature_scale(temp_c: float, config: SimulationConfig) -> float:
    if temp_c >= config.physics.capacity_temperature_reference_c:
        return 1.0
    cold_delta_c = config.physics.capacity_temperature_reference_c - temp_c
    scale = 1.0 - config.physics.capacity_cold_derate_per_c * cold_delta_c
    return max(config.physics.min_capacity_scale, min(scale, 1.0))


def effective_rc_branch(branch: RcBranchParams, series_factor: float, parallel_count: int) -> RcBranchParams:
    parallel_count = max(parallel_count, 1)
    return RcBranchParams(
        resistance_ohm=branch.resistance_ohm * series_factor / parallel_count,
        capacitance_f=branch.capacitance_f * parallel_count / max(series_factor, 1e-9),
    )


def state_adjusted_rc_branch(
    branch: RcBranchParams,
    series_factor: float,
    parallel_count: int,
    soc: float,
    temp_c: float,
    config: SimulationConfig,
) -> RcBranchParams:
    effective_branch = effective_rc_branch(branch, series_factor, parallel_count)
    if not config.physics.rc_state_dependence_enabled:
        return effective_branch

    low_soc_intensity = max(0.0, 0.5 - soc) / 0.5
    resistance_multiplier = 1.0 + low_soc_intensity * (config.physics.rc_low_soc_multiplier - 1.0)
    if temp_c > config.physics.resistance_reference_temp_c:
        resistance_multiplier *= (
            1.0 + (temp_c - config.physics.resistance_reference_temp_c) * config.physics.rc_high_temp_multiplier_per_c
        )
    return RcBranchParams(
        resistance_ohm=max(effective_branch.resistance_ohm * resistance_multiplier, 1e-9),
        capacitance_f=effective_branch.capacitance_f,
    )


def step_rc_branch_voltage(voltage_v: float, current_a: float, branch: RcBranchParams, dt_s: int) -> float:
    tau_s = max(branch.resistance_ohm * branch.capacitance_f, 1e-9)
    alpha = math.exp(-dt_s / tau_s)
    return alpha * voltage_v + branch.resistance_ohm * (1.0 - alpha) * current_a


def step_electrical_state(
    current_a: float,
    dt_s: int,
    model: ElectricalModelConfig,
    state: GroupElectricalState,
    series_factor: float,
    parallel_count: int,
    soc: float,
    temp_c: float,
    config: SimulationConfig,
) -> GroupElectricalState:
    branch_voltages: list[float] = []
    if model.model_type != "rint" and model.rc_branches:
        for branch_voltage_v, branch in zip(state.rc_branch_voltages_v, model.rc_branches):
            adjusted_branch = state_adjusted_rc_branch(branch, series_factor, parallel_count, soc, temp_c, config)
            branch_voltages.append(
                step_rc_branch_voltage(
                    voltage_v=branch_voltage_v,
                    current_a=current_a,
                    branch=adjusted_branch,
                    dt_s=dt_s,
                )
            )

    hysteresis_voltage_v = state.hysteresis_voltage_v
    if config.physics.hysteresis_enabled and config.physics.hysteresis_max_voltage_v > 0.0:
        drive = min(abs(current_a) / max(config.physics.hysteresis_current_scale_a, 1e-9), 1.0)
        target = 0.0 if abs(current_a) < 1e-9 else math.copysign(config.physics.hysteresis_max_voltage_v, current_a)
        drive_alpha = 1.0 - math.exp(-dt_s * config.physics.hysteresis_response_rate_per_s * drive)
        relax_alpha = 1.0 - math.exp(-dt_s / max(config.physics.hysteresis_relaxation_tau_s, 1e-9))
        if abs(current_a) < 1e-9:
            hysteresis_voltage_v *= (1.0 - relax_alpha)
        else:
            hysteresis_voltage_v = hysteresis_voltage_v + drive_alpha * (target - hysteresis_voltage_v)

    diffusion_stress_v = state.diffusion_stress_v
    if config.physics.diffusion_stress_enabled and config.physics.diffusion_stress_max_v > 0.0:
        target = min(
            config.physics.diffusion_stress_max_v,
            config.physics.diffusion_stress_max_v * abs(current_a) / max(config.physics.diffusion_stress_current_scale_a, 1e-9),
        )
        if abs(current_a) < 1e-9:
            decay_alpha = 1.0 - math.exp(-dt_s / max(config.physics.diffusion_stress_decay_tau_s, 1e-9))
            diffusion_stress_v *= (1.0 - decay_alpha)
        else:
            build_alpha = 1.0 - math.exp(-dt_s * config.physics.diffusion_stress_build_rate_per_s)
            diffusion_stress_v = diffusion_stress_v + build_alpha * (target - diffusion_stress_v)

    return GroupElectricalState(
        rc_branch_voltages_v=tuple(branch_voltages),
        hysteresis_voltage_v=hysteresis_voltage_v,
        diffusion_stress_v=diffusion_stress_v,
    )


def compute_terminal_voltage(
    open_circuit_voltage_v: float,
    current_a: float,
    r0_ohm: float,
    state: GroupElectricalState,
) -> float:
    return max(
        open_circuit_voltage_v
        - current_a * r0_ohm
        - sum(state.rc_branch_voltages_v)
        - state.diffusion_stress_v
        - state.hysteresis_voltage_v,
        0.0,
    )


def compute_power_w(terminal_voltage_v: float, current_a: float) -> float:
    return terminal_voltage_v * current_a


def compute_heat_w(
    current_a: float,
    r0_ohm: float,
) -> float:
    return (current_a**2) * r0_ohm


def diffusion_stress_resistance_multiplier(state: GroupElectricalState, config: SimulationConfig) -> float:
    if not config.physics.diffusion_stress_enabled or config.physics.diffusion_stress_max_v <= 0.0:
        return 1.0
    normalized_stress = min(
        max(state.diffusion_stress_v / max(config.physics.diffusion_stress_max_v, 1e-9), 0.0),
        1.0,
    )
    return 1.0 + normalized_stress * config.physics.diffusion_stress_resistance_coeff


def compute_reversible_heat_w(current_a: float, temp_c: float, soc: float, config: SimulationConfig) -> float:
    if not config.physics.reversible_heat_enabled or config.physics.reversible_heat_coeff_v_per_k == 0.0:
        return 0.0
    entropy_coeff_v_per_k = config.physics.reversible_heat_coeff_v_per_k * (2.0 * soc - 1.0)
    return -current_a * (temp_c + 273.15) * entropy_coeff_v_per_k


def compute_next_soc(current_soc: float, current_a: float, dt_s: int, capacity_as: float, config: SimulationConfig) -> float:
    if current_a >= 0.0:
        coulomb_term = current_a * dt_s / max(capacity_as * max(config.physics.discharge_efficiency, 1e-9), 1e-9)
    else:
        coulomb_term = current_a * dt_s * config.physics.charge_efficiency / max(capacity_as, 1e-9)
    self_discharge_term = config.physics.self_discharge_per_day * dt_s / 86400.0
    next_soc = current_soc - coulomb_term - self_discharge_term
    return min(max(next_soc, 0.0), 1.0)


def is_supported_model(model_type: str) -> bool:
    return model_type in {"rint", "1rc", "2rc"}


def validate_electrical_model(model: ElectricalModelConfig) -> ElectricalModelConfig:
    model_type = model.model_type.lower()
    if not is_supported_model(model_type):
        raise ValueError(f"Unsupported electrical model type: {model.model_type}")

    required_branches = {"rint": 0, "1rc": 1, "2rc": 2}[model_type]
    branches = model.rc_branches[:required_branches]
    if len(branches) != required_branches:
        raise ValueError(
            f"Electrical model '{model_type}' requires {required_branches} RC branch(es); "
            f"received {len(model.rc_branches)}."
        )
    if model.r0_ohm_per_cell is not None and model.r0_ohm_per_cell <= 0.0:
        raise ValueError("Electrical model r0_ohm_per_cell must be > 0.")
    for branch in branches:
        if branch.resistance_ohm <= 0.0:
            raise ValueError("RC branch resistance_ohm must be > 0.")
        if branch.capacitance_f <= 0.0:
            raise ValueError("RC branch capacitance_f must be > 0.")
    return ElectricalModelConfig(
        model_type=model_type,
        r0_ohm_per_cell=model.r0_ohm_per_cell,
        rc_branches=branches,
    )


def initial_pack_voltage(config: SimulationConfig, pack: PackProperties) -> float:
    return compute_open_circuit_voltage(config.initial_soc, config, group_voltage_scale=pack.group_count * pack.series_factor)
