from __future__ import annotations

from ..physics.electrical import init_rc_state, validate_electrical_model
from ..physics.faults import effects_for_group
from ..types import CellGroupState, PackProperties, SimulationConfig


def effective_group_count(config: SimulationConfig) -> int:
    return max(1, config.group_count or config.cells_in_series)


def derive_pack_properties(config: SimulationConfig) -> PackProperties:
    """Groups are equal-sized series segments, not fractional virtual slices."""
    group_count = effective_group_count(config)
    if config.cells_in_series % group_count != 0:
        raise ValueError(
            "group_count must evenly divide cells_in_series so each group maps to a real series segment."
        )
    series_factor = config.cells_in_series // group_count
    group_nominal_voltage_v = config.cell_nominal_voltage * series_factor
    group_capacity_ah = config.cell_capacity_ah * config.cells_in_parallel
    group_base_resistance_ohm = (
        config.internal_resistance_ohm_per_cell * series_factor / max(config.cells_in_parallel, 1)
    )

    nominal_voltage_v = group_nominal_voltage_v * group_count
    capacity_ah = group_capacity_ah
    theoretical_energy_wh = nominal_voltage_v * capacity_ah
    resistance_ohm = group_base_resistance_ohm * group_count
    capacity_as = capacity_ah * 3600.0

    return PackProperties(
        nominal_voltage_v=nominal_voltage_v,
        capacity_ah=capacity_ah,
        theoretical_energy_wh=theoretical_energy_wh,
        resistance_ohm=resistance_ohm,
        capacity_as=capacity_as,
        group_count=group_count,
        series_factor=series_factor,
        group_nominal_voltage_v=group_nominal_voltage_v,
        group_capacity_ah=group_capacity_ah,
        group_capacity_as=group_capacity_ah * 3600.0,
        group_base_resistance_ohm=group_base_resistance_ohm,
    )


def build_group_states(config: SimulationConfig, pack: PackProperties) -> list[CellGroupState]:
    validate_electrical_model(config.electrical_model)

    if pack.group_count <= 1:
        offsets = [0.0]
    else:
        offsets = [
            (index - (pack.group_count - 1) / 2.0) / ((pack.group_count - 1) / 2.0)
            for index in range(pack.group_count)
        ]

    states: list[CellGroupState] = []
    for index, offset in enumerate(offsets):
        capacity_scale = max(0.7, 1.0 + config.group_variation.capacity_variation_fraction * offset)
        resistance_scale = max(0.5, 1.0 + config.group_variation.resistance_variation_fraction * offset)
        initial_soc = min(
            max(config.initial_soc + config.group_variation.initial_soc_variation_abs * offset, 0.0),
            1.0,
        )
        initial_faults = effects_for_group(config.faults, index, 0)
        states.append(
            CellGroupState(
                index=index,
                soc=min(max(initial_soc + initial_faults.soc_offset, 0.0), 1.0),
                temp_c=config.ambient_temp_c,
                core_temp_c=config.ambient_temp_c,
                surface_temp_c=config.ambient_temp_c,
                resistance_scale=resistance_scale,
                capacity_scale=capacity_scale,
                electrical_state=init_rc_state(config.electrical_model),
            )
        )
    return states
