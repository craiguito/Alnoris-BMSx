from __future__ import annotations

"""Passive balancing helpers for early-stage pack analysis.

This is a deterministic bleed-current approximation, not an active balancing controller.
"""

from ..physics.electrical import compute_open_circuit_voltage
from ..physics.faults import effects_for_group
from ..types import BalancingConfig, CellGroupState, SimulationConfig


def balance_currents_for_groups(
    groups: list[CellGroupState],
    config: SimulationConfig,
    group_voltage_scale: float,
    time_s: int,
) -> list[float]:
    balancing = config.balancing
    if not balancing.enabled:
        return [0.0 for _ in groups]

    candidates: list[tuple[float, int]] = []
    for group in groups:
        fault_effects = effects_for_group(config.faults, group.index, time_s)
        reported_soc = min(max(group.soc + fault_effects.reported_soc_offset, 0.0), 1.0)
        meets_soc = balancing.soc_threshold is not None and reported_soc >= balancing.soc_threshold
        group_ocv_v = compute_open_circuit_voltage(group.soc, config, group_voltage_scale=group_voltage_scale)
        meets_voltage = balancing.voltage_threshold_v is not None and group_ocv_v >= balancing.voltage_threshold_v
        if not (meets_soc or meets_voltage):
            continue
        score = max(reported_soc, group_ocv_v)
        candidates.append((score, group.index))

    candidates.sort(reverse=True)
    if balancing.max_active_groups is not None:
        candidates = candidates[: balancing.max_active_groups]

    selected = {index for _, index in candidates}
    return [
        balancing.bleed_current_a if group.index in selected else 0.0
        for group in groups
    ]
