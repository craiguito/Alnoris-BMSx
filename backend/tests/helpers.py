from __future__ import annotations

from backend.sim_core.reference_cells import REFERENCE_CELLS
from backend.sim_core.types import (
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    GroupVariationConfig,
    RcBranchParams,
    SimulationConfig,
)


def make_config(cell_key: str = "panasonic_ncr18650b", **overrides: object) -> SimulationConfig:
    cell = REFERENCE_CELLS[cell_key]
    config = SimulationConfig(
        cell_nominal_voltage=cell.cell_nominal_voltage,
        cell_full_voltage=cell.cell_full_voltage,
        cell_empty_voltage=cell.cell_empty_voltage,
        cell_cutoff_voltage=cell.cell_cutoff_voltage,
        cell_capacity_ah=cell.cell_capacity_ah,
        cells_in_series=1,
        cells_in_parallel=1,
        internal_resistance_ohm_per_cell=cell.internal_resistance_ohm_per_cell,
        ambient_temp_c=25.0,
        discharge_current_a=cell.recommended_discharge_current_a,
        duration_s=7200,
        time_step_s=1,
        initial_soc=1.0,
        pack_mass_kg=cell.pack_mass_kg,
        pack_heat_capacity_j_per_kgk=cell.pack_heat_capacity_j_per_kgk,
        cooling_coeff_w_per_k=cell.cooling_coeff_w_per_k,
        electrical_model=ElectricalModelConfig(
            model_type="rint",
            r0_ohm_per_cell=cell.internal_resistance_ohm_per_cell,
        ),
        current_profile=None,
        group_count=None,
        group_variation=GroupVariationConfig(),
        degradation=DegradationConfig(),
    )
    return SimulationConfig(**{**config.__dict__, **overrides})


def make_1rc_model(r0_ohm_per_cell: float, branch_resistance_ohm: float, capacitance_f: float) -> ElectricalModelConfig:
    return ElectricalModelConfig(
        model_type="1rc",
        r0_ohm_per_cell=r0_ohm_per_cell,
        rc_branches=(RcBranchParams(branch_resistance_ohm, capacitance_f),),
    )


def make_2rc_model(
    r0_ohm_per_cell: float,
    branch1_resistance_ohm: float,
    branch1_capacitance_f: float,
    branch2_resistance_ohm: float,
    branch2_capacitance_f: float,
) -> ElectricalModelConfig:
    return ElectricalModelConfig(
        model_type="2rc",
        r0_ohm_per_cell=r0_ohm_per_cell,
        rc_branches=(
            RcBranchParams(branch1_resistance_ohm, branch1_capacitance_f),
            RcBranchParams(branch2_resistance_ohm, branch2_capacitance_f),
        ),
    )


def make_profile(*pairs: tuple[int, float]) -> CurrentProfile:
    return CurrentProfile(points=tuple(CurrentProfilePoint(time_s=time_s, current_a=current_a) for time_s, current_a in pairs))
