from __future__ import annotations

import math

from ..types import (
    ElectricalModelConfig,
    GroupElectricalState,
    PackProperties,
    RcBranchParams,
    SimulationConfig,
)


def compute_open_circuit_voltage(
    soc: float,
    config: SimulationConfig,
    group_voltage_scale: float = 1.0,
) -> float:
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
) -> GroupElectricalState:
    if model.model_type == "rint" or not model.rc_branches:
        return GroupElectricalState(rc_branch_voltages_v=())

    branch_voltages: list[float] = []
    for branch_voltage_v, branch in zip(state.rc_branch_voltages_v, model.rc_branches):
        effective_branch = effective_rc_branch(branch, series_factor, parallel_count)
        branch_voltages.append(
            step_rc_branch_voltage(
                voltage_v=branch_voltage_v,
                current_a=current_a,
                branch=effective_branch,
                dt_s=dt_s,
            )
        )
    return GroupElectricalState(rc_branch_voltages_v=tuple(branch_voltages))


def compute_terminal_voltage(
    open_circuit_voltage_v: float,
    current_a: float,
    r0_ohm: float,
    state: GroupElectricalState,
) -> float:
    return max(open_circuit_voltage_v - current_a * r0_ohm - sum(state.rc_branch_voltages_v), 0.0)


def compute_power_w(terminal_voltage_v: float, current_a: float) -> float:
    return terminal_voltage_v * current_a


def compute_heat_w(
    current_a: float,
    r0_ohm: float,
) -> float:
    return (current_a**2) * r0_ohm


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
