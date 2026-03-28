from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .engine import run_simulation
from .physics.electrical import validate_electrical_model
from .types import (
    BalancingConfig,
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    FaultConfig,
    FaultSpec,
    GroupVariationConfig,
    RcBranchParams,
    SimulationConfig,
    SimulationResult,
    ThermalZoneConfig,
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
    defaults = DegradationConfig()
    return DegradationConfig(
        throughput_capacity_fade_per_ah=(
            float(degradation_payload["throughput_capacity_fade_per_ah"])
            if degradation_payload.get("throughput_capacity_fade_per_ah") is not None
            else (
                float(degradation_payload["capacity_fade_per_throughput_ah"])
                if degradation_payload.get("capacity_fade_per_throughput_ah") is not None
                else defaults.throughput_capacity_fade_per_ah
            )
        ),
        throughput_resistance_growth_per_ah=(
            float(degradation_payload["throughput_resistance_growth_per_ah"])
            if degradation_payload.get("throughput_resistance_growth_per_ah") is not None
            else (
                float(degradation_payload["resistance_growth_per_throughput_ah"])
                if degradation_payload.get("resistance_growth_per_throughput_ah") is not None
                else defaults.throughput_resistance_growth_per_ah
            )
        ),
        calendar_capacity_fade_per_hour=float(
            degradation_payload.get("calendar_capacity_fade_per_hour", defaults.calendar_capacity_fade_per_hour)
        ),
        calendar_resistance_growth_per_hour=float(
            degradation_payload.get("calendar_resistance_growth_per_hour", defaults.calendar_resistance_growth_per_hour)
        ),
        high_soc_capacity_accel=float(
            degradation_payload.get("high_soc_capacity_accel", defaults.high_soc_capacity_accel)
        ),
        high_soc_resistance_accel=float(
            degradation_payload.get("high_soc_resistance_accel", defaults.high_soc_resistance_accel)
        ),
        dod_stress_factor=float(
            degradation_payload.get("dod_stress_factor", defaults.dod_stress_factor)
        ),
        charge_stress_factor=float(
            degradation_payload.get("charge_stress_factor", defaults.charge_stress_factor)
        ),
        reference_temp_c=(
            float(degradation_payload["reference_temp_c"])
            if degradation_payload.get("reference_temp_c") is not None
            else (
                float(degradation_payload["temperature_reference_c"])
                if degradation_payload.get("temperature_reference_c") is not None
                else defaults.reference_temp_c
            )
        ),
        temperature_accel_per_c=(
            float(degradation_payload["temperature_accel_per_c"])
            if degradation_payload.get("temperature_accel_per_c") is not None
            else (
                float(degradation_payload["temperature_acceleration_per_c"])
                if degradation_payload.get("temperature_acceleration_per_c") is not None
                else defaults.temperature_accel_per_c
            )
        ),
        high_soc_threshold=float(
            degradation_payload.get("high_soc_threshold", defaults.high_soc_threshold)
        ),
        charge_current_stress_threshold_a=(
            float(degradation_payload["charge_current_stress_threshold_a"])
            if degradation_payload.get("charge_current_stress_threshold_a") is not None
            else defaults.charge_current_stress_threshold_a
        ),
        capacity_fade_per_throughput_ah=float(
            degradation_payload.get("capacity_fade_per_throughput_ah", defaults.capacity_fade_per_throughput_ah)
        ),
        resistance_growth_per_throughput_ah=float(
            degradation_payload.get("resistance_growth_per_throughput_ah", defaults.resistance_growth_per_throughput_ah)
        ),
        temperature_reference_c=float(
            degradation_payload.get("temperature_reference_c", defaults.temperature_reference_c)
        ),
        temperature_acceleration_per_c=float(
            degradation_payload.get("temperature_acceleration_per_c", defaults.temperature_acceleration_per_c)
        ),
        depth_of_discharge_weight=float(
            degradation_payload.get("depth_of_discharge_weight", defaults.depth_of_discharge_weight)
        ),
    )


def _parse_balancing(payload: dict[str, Any]) -> BalancingConfig:
    balancing_payload = payload.get("balancing", {})
    return BalancingConfig(
        enabled=bool(balancing_payload.get("enabled", False)),
        mode=str(balancing_payload.get("mode", "passive")),
        voltage_threshold_v=(
            float(balancing_payload["voltage_threshold_v"])
            if balancing_payload.get("voltage_threshold_v") is not None
            else None
        ),
        soc_threshold=(
            float(balancing_payload["soc_threshold"])
            if balancing_payload.get("soc_threshold") is not None
            else None
        ),
        bleed_current_a=float(balancing_payload.get("bleed_current_a", 0.0)),
        max_active_groups=(
            int(balancing_payload["max_active_groups"])
            if balancing_payload.get("max_active_groups") is not None
            else None
        ),
    )


def _parse_faults(payload: dict[str, Any]) -> FaultConfig:
    fault_payload = payload.get("faults", {})
    raw_faults = fault_payload if isinstance(fault_payload, list) else fault_payload.get("faults", [])
    faults: list[FaultSpec] = []
    for item in raw_faults:
        faults.append(
            FaultSpec(
                fault_type=str(item["fault_type"]),
                group_index=int(item["group_index"]),
                factor=float(item["factor"]),
                start_time_s=float(item.get("start_time_s", 0.0)),
                end_time_s=float(item["end_time_s"]) if item.get("end_time_s") is not None else None,
            )
        )
    return FaultConfig(faults=tuple(faults))


def _parse_thermal_zones(payload: dict[str, Any]) -> tuple[ThermalZoneConfig, ...]:
    zones_payload = payload.get("thermal_zones", [])
    zones: list[ThermalZoneConfig] = []
    for zone_payload in zones_payload:
        zones.append(
            ThermalZoneConfig(
                zone_id=int(zone_payload["zone_id"]),
                name=str(zone_payload.get("name", f"Zone {zone_payload['zone_id']}")),
                ambient_temp_c=float(zone_payload["ambient_temp_c"]) if zone_payload.get("ambient_temp_c") is not None else None,
                cooling_coeff_w_per_k=float(zone_payload["cooling_coeff_w_per_k"]) if zone_payload.get("cooling_coeff_w_per_k") is not None else None,
                cooling_coeff_multiplier=float(zone_payload["cooling_coeff_multiplier"]) if zone_payload.get("cooling_coeff_multiplier") is not None else None,
                note=str(zone_payload.get("note", "")),
            )
        )
    return tuple(zones)


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
            balancing=_parse_balancing(payload),
            faults=_parse_faults(payload),
            thermal_zones=_parse_thermal_zones(payload),
            group_zone_assignments=tuple(int(zone_id) for zone_id in payload.get("group_zone_assignments", [])),
            group_labels=tuple(str(label) for label in payload.get("group_labels", [])),
            group_entity_ids=tuple(str(entity_id) for entity_id in payload.get("group_entity_ids", [])),
        )
    )


def run_simulation_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    config = simulation_config_from_dict(payload)
    result = run_simulation(config)
    return simulation_result_to_dict(result)


def simulation_result_to_dict(result: SimulationResult) -> dict[str, Any]:
    return asdict(result)
