from __future__ import annotations

"""Group-level fault injection helpers.

These faults are intended for exploratory engineering analysis, not certification-grade
failure prediction.
"""

from dataclasses import dataclass

from ..types import FaultConfig, FaultSpec


@dataclass(frozen=True)
class FaultEffects:
    resistance_multiplier: float = 1.0
    capacity_multiplier: float = 1.0
    heat_multiplier: float = 1.0
    cooling_multiplier: float = 1.0
    reported_soc_offset: float = 0.0
    flags: tuple[str, ...] = ()


def is_fault_active(fault: FaultSpec, time_s: int) -> bool:
    if time_s < fault.start_time_s:
        return False
    if fault.end_time_s is not None and time_s > fault.end_time_s:
        return False
    return True


def effects_for_group(faults: FaultConfig, group_index: int, time_s: int) -> FaultEffects:
    resistance_multiplier = 1.0
    capacity_multiplier = 1.0
    heat_multiplier = 1.0
    cooling_multiplier = 1.0
    reported_soc_offset = 0.0
    flags: list[str] = []

    for fault in faults.faults:
        if fault.group_index != group_index or not is_fault_active(fault, time_s):
            continue
        flags.append(fault.fault_type)
        if fault.fault_type == "high_resistance_group":
            resistance_multiplier *= fault.factor
        elif fault.fault_type == "low_capacity_group":
            capacity_multiplier *= fault.factor
        elif fault.fault_type == "elevated_self_heating_group":
            heat_multiplier *= fault.factor
        elif fault.fault_type == "cooling_loss_group":
            cooling_multiplier *= fault.factor
        elif fault.fault_type == "stuck_high_soc_group":
            reported_soc_offset += fault.factor

    return FaultEffects(
        resistance_multiplier=resistance_multiplier,
        capacity_multiplier=capacity_multiplier,
        heat_multiplier=heat_multiplier,
        cooling_multiplier=cooling_multiplier,
        reported_soc_offset=reported_soc_offset,
        flags=tuple(flags),
    )


def reported_soc_for_group(faults: FaultConfig, group_index: int, true_soc: float, time_s: int) -> float:
    """Return the BMS-reported SOC for a group without changing physical SOC.

    Reporting faults such as ``stuck_high_soc_group`` bias the surfaced SOC
    signal, but the simulator's electrochemical state remains anchored to the
    true SOC value.
    """

    effects = effects_for_group(faults, group_index, time_s)
    return min(max(true_soc + effects.reported_soc_offset, 0.0), 1.0)
