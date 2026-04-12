from __future__ import annotations

import json
import shutil
from contextlib import contextmanager
from pathlib import Path
from uuid import uuid4

from backend.sim_core.reference_cells import REFERENCE_CELLS
from backend.sim_core.types import (
    BalancingConfig,
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    FaultConfig,
    GroupVariationConfig,
    PhysicsConfig,
    RcBranchParams,
    SimulationConfig,
    SimulationResult,
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
        balancing=BalancingConfig(),
        faults=FaultConfig(),
        physics=PhysicsConfig(),
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


def build_truth_dataset_payload(
    config: SimulationConfig,
    result: SimulationResult,
    *,
    include_soc: bool = True,
    extra_metadata: dict[str, object] | None = None,
) -> dict[str, object]:
    metadata: dict[str, object] = {
        "chemistry_name": config.chemistry_name,
        "ambient_temp_c": config.ambient_temp_c,
        "initial_soc": config.initial_soc,
        "capacity_ah": config.cell_capacity_ah * config.cells_in_parallel,
        "cells_in_series": config.cells_in_series,
        "cells_in_parallel": config.cells_in_parallel,
        "group_count": config.group_count if config.group_count is not None else config.cells_in_series,
    }
    if extra_metadata:
        metadata.update(extra_metadata)

    data: list[dict[str, object]] = []
    for point in result.time_series:
        row: dict[str, object] = {
            "time_s": point.time_s,
            "current_a": point.current_a,
            "voltage_v": point.pack_voltage_v,
            "temp_c": point.pack_temp_avg_c,
        }
        if include_soc:
            row["soc"] = point.soc_avg
        data.append(row)

    return {
        "metadata": metadata,
        "data": data,
    }


def write_truth_dataset(
    path: str | Path,
    config: SimulationConfig,
    result: SimulationResult,
    *,
    include_soc: bool = True,
    extra_metadata: dict[str, object] | None = None,
) -> Path:
    target = Path(path)
    payload = build_truth_dataset_payload(
        config,
        result,
        include_soc=include_soc,
        extra_metadata=extra_metadata,
    )
    target.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return target


@contextmanager
def temporary_workspace_dir(prefix: str = "sim_truth_"):
    root = Path(__file__).resolve().parent / "_tmp"
    root.mkdir(parents=True, exist_ok=True)
    target = root / f"{prefix}{uuid4().hex}"
    target.mkdir(parents=True, exist_ok=True)
    try:
        yield str(target)
    finally:
        shutil.rmtree(target, ignore_errors=True)
