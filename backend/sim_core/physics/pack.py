from __future__ import annotations

from ..types import PackProperties, SimulationConfig


def derive_pack_properties(config: SimulationConfig) -> PackProperties:
    nominal_voltage_v = config.cell_nominal_voltage * config.cells_in_series
    capacity_ah = config.cell_capacity_ah * config.cells_in_parallel
    theoretical_energy_wh = nominal_voltage_v * capacity_ah
    resistance_ohm = (
        config.internal_resistance_ohm_per_cell
        * config.cells_in_series
        / config.cells_in_parallel
    )
    capacity_as = capacity_ah * 3600.0

    return PackProperties(
        nominal_voltage_v=nominal_voltage_v,
        capacity_ah=capacity_ah,
        theoretical_energy_wh=theoretical_energy_wh,
        resistance_ohm=resistance_ohm,
        capacity_as=capacity_as,
    )
