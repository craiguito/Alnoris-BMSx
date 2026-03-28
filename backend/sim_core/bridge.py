from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .engine import run_simulation
from .physics.electrical import validate_electrical_model
from .types import (
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    GroupVariationConfig,
    RcBranchParams,
    SimulationConfig,
    SimulationResult,
)
from .validation import validate_current_profile, validate_simulation_config


def _parse_rc_branches(payload: Any) -> tuple[RcBranchParams, ...]:
    if payload is None:
        return ()

    branches: list[RcBranchParams] = []
    for branch_payload in payload:
        branches.append(
            RcBranchParams(
                resistance_ohm=float(branch_payload["resistance_ohm"]),
                capacitance_f=float(branch_payload["capacitance_f"]),
            )
        )
    return tuple(branches)


def _parse_electrical_model(payload: dict[str, Any]) -> ElectricalModelConfig:
    model_payload = payload.get("electrical_model")
    if model_payload is None:
        return validate_electrical_model(
            ElectricalModelConfig(
                model_type="rint",
                r0_ohm_per_cell=float(payload["internal_resistance_ohm_per_cell"]),
            )
        )

    if isinstance(model_payload, str):
        return validate_electrical_model(
            ElectricalModelConfig(
                model_type=model_payload,
                r0_ohm_per_cell=float(payload["internal_resistance_ohm_per_cell"]),
            )
        )

    return validate_electrical_model(
        ElectricalModelConfig(
            model_type=str(model_payload.get("type", "rint")),
            r0_ohm_per_cell=float(
                model_payload.get("r0_ohm_per_cell", payload["internal_resistance_ohm_per_cell"])
            ),
            rc_branches=_parse_rc_branches(model_payload.get("rc_branches")),
        )
    )


def _parse_current_profile(payload: dict[str, Any]) -> CurrentProfile | None:
    profile_payload = payload.get("current_profile")
    if profile_payload is None:
        return None

    if isinstance(profile_payload, str):
        return validate_current_profile(CurrentProfile.from_csv(profile_payload))

    if isinstance(profile_payload, list):
        points_payload = profile_payload
    else:
        points_payload = profile_payload.get("points", [])

    points = tuple(
        CurrentProfilePoint(
            time_s=int(point["time_s"]),
            current_a=float(point["current_a"]),
        )
        for point in points_payload
    )
    return validate_current_profile(CurrentProfile(points=points))


def _parse_group_variation(payload: dict[str, Any]) -> GroupVariationConfig:
    variation_payload = payload.get("group_variation", {})
    return GroupVariationConfig(
        capacity_variation_fraction=float(variation_payload.get("capacity_variation_fraction", 0.0)),
        resistance_variation_fraction=float(variation_payload.get("resistance_variation_fraction", 0.0)),
        initial_soc_variation_abs=float(variation_payload.get("initial_soc_variation_abs", 0.0)),
    )


def _parse_degradation(payload: dict[str, Any]) -> DegradationConfig:
    degradation_payload = payload.get("degradation", {})
    return DegradationConfig(
        capacity_fade_per_throughput_ah=float(
            degradation_payload.get("capacity_fade_per_throughput_ah", DegradationConfig().capacity_fade_per_throughput_ah)
        ),
        resistance_growth_per_throughput_ah=float(
            degradation_payload.get("resistance_growth_per_throughput_ah", DegradationConfig().resistance_growth_per_throughput_ah)
        ),
        temperature_reference_c=float(
            degradation_payload.get("temperature_reference_c", DegradationConfig().temperature_reference_c)
        ),
        temperature_acceleration_per_c=float(
            degradation_payload.get("temperature_acceleration_per_c", DegradationConfig().temperature_acceleration_per_c)
        ),
        depth_of_discharge_weight=float(
            degradation_payload.get("depth_of_discharge_weight", DegradationConfig().depth_of_discharge_weight)
        ),
    )


def simulation_config_from_dict(payload: dict[str, Any]) -> SimulationConfig:
    return validate_simulation_config(
        SimulationConfig(
        cell_nominal_voltage=float(payload["cell_nominal_voltage"]),
        cell_full_voltage=float(payload["cell_full_voltage"]),
        cell_empty_voltage=float(payload["cell_empty_voltage"]),
        cell_cutoff_voltage=float(payload["cell_cutoff_voltage"]),
        cell_capacity_ah=float(payload["cell_capacity_ah"]),
        cells_in_series=int(payload["cells_in_series"]),
        cells_in_parallel=int(payload["cells_in_parallel"]),
        internal_resistance_ohm_per_cell=float(payload["internal_resistance_ohm_per_cell"]),
        ambient_temp_c=float(payload["ambient_temp_c"]),
        discharge_current_a=float(payload["discharge_current_a"]),
        duration_s=int(payload["duration_s"]),
        time_step_s=int(payload["time_step_s"]),
        initial_soc=float(payload["initial_soc"]),
        pack_mass_kg=float(payload["pack_mass_kg"]),
        pack_heat_capacity_j_per_kgk=float(payload["pack_heat_capacity_j_per_kgk"]),
        cooling_coeff_w_per_k=float(payload["cooling_coeff_w_per_k"]),
        electrical_model=_parse_electrical_model(payload),
        current_profile=_parse_current_profile(payload),
        group_count=int(payload["group_count"]) if payload.get("group_count") is not None else None,
        group_variation=_parse_group_variation(payload),
        degradation=_parse_degradation(payload),
        )
    )


def run_simulation_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    config = simulation_config_from_dict(payload)
    result = run_simulation(config)
    return simulation_result_to_dict(result)


def simulation_result_to_dict(result: SimulationResult) -> dict[str, Any]:
    return asdict(result)
