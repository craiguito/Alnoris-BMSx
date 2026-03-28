from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .engine import run_simulation
from .physics.electrical import validate_electrical_model
from .test_catalog import build_test_catalog
from .test_runner import run_virtual_test, virtual_test_result_to_dict
from .test_vetting import vet_virtual_test
from .types import (
    BalancingConfig,
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    FaultConfig,
    FaultSpec,
    GroupVariationConfig,
    PhysicsConfig,
    RcBranchParams,
    SocLookupPoint,
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


def _parse_physics(payload: dict[str, Any]) -> PhysicsConfig:
    physics_payload = payload.get("physics", {})
    defaults = PhysicsConfig()
    curve_payload = physics_payload.get("resistance_soc_curve", defaults.resistance_soc_curve)
    curve_items: list[SocLookupPoint] = []
    for item in curve_payload or ():
        if isinstance(item, SocLookupPoint):
            curve_items.append(item)
        else:
            curve_items.append(
                SocLookupPoint(soc=float(item["soc"]), multiplier=float(item["multiplier"]))
            )
    curve = tuple(curve_items)
    return PhysicsConfig(
        discharge_efficiency=float(physics_payload.get("discharge_efficiency", defaults.discharge_efficiency)),
        charge_efficiency=float(physics_payload.get("charge_efficiency", defaults.charge_efficiency)),
        resistance_temperature_alpha_per_c=float(
            physics_payload.get("resistance_temperature_alpha_per_c", defaults.resistance_temperature_alpha_per_c)
        ),
        resistance_reference_temp_c=float(
            physics_payload.get("resistance_reference_temp_c", defaults.resistance_reference_temp_c)
        ),
        capacity_temperature_reference_c=float(
            physics_payload.get("capacity_temperature_reference_temp_c", physics_payload.get("capacity_temperature_reference_c", defaults.capacity_temperature_reference_c))
        ),
        capacity_cold_derate_per_c=float(
            physics_payload.get("capacity_cold_derate_per_c", defaults.capacity_cold_derate_per_c)
        ),
        min_capacity_scale=float(
            physics_payload.get("min_capacity_scale", defaults.min_capacity_scale)
        ),
        self_discharge_per_day=float(
            physics_payload.get("self_discharge_per_day", defaults.self_discharge_per_day)
        ),
        interconnect_resistance_ohm_per_group=float(
            physics_payload.get("interconnect_resistance_ohm_per_group", defaults.interconnect_resistance_ohm_per_group)
        ),
        pack_interconnect_resistance_ohm=float(
            physics_payload.get("pack_interconnect_resistance_ohm", defaults.pack_interconnect_resistance_ohm)
        ),
        neighbor_thermal_coupling_w_per_k=float(
            physics_payload.get("neighbor_thermal_coupling_w_per_k", defaults.neighbor_thermal_coupling_w_per_k)
        ),
        resistance_vs_soc_enabled=bool(
            physics_payload.get("resistance_vs_soc_enabled", defaults.resistance_vs_soc_enabled)
        ),
        resistance_soc_curve=curve if curve else defaults.resistance_soc_curve,
        hysteresis_enabled=bool(
            physics_payload.get("hysteresis_enabled", defaults.hysteresis_enabled)
        ),
        hysteresis_max_voltage_v=float(
            physics_payload.get("hysteresis_max_voltage_v", defaults.hysteresis_max_voltage_v)
        ),
        hysteresis_response_rate_per_s=float(
            physics_payload.get("hysteresis_response_rate_per_s", defaults.hysteresis_response_rate_per_s)
        ),
        hysteresis_relaxation_tau_s=float(
            physics_payload.get("hysteresis_relaxation_tau_s", defaults.hysteresis_relaxation_tau_s)
        ),
        hysteresis_current_scale_a=float(
            physics_payload.get("hysteresis_current_scale_a", defaults.hysteresis_current_scale_a)
        ),
        rc_state_dependence_enabled=bool(
            physics_payload.get("rc_state_dependence_enabled", defaults.rc_state_dependence_enabled)
        ),
        rc_low_soc_multiplier=float(
            physics_payload.get("rc_low_soc_multiplier", defaults.rc_low_soc_multiplier)
        ),
        rc_high_temp_multiplier_per_c=float(
            physics_payload.get("rc_high_temp_multiplier_per_c", defaults.rc_high_temp_multiplier_per_c)
        ),
        diffusion_stress_enabled=bool(
            physics_payload.get("diffusion_stress_enabled", defaults.diffusion_stress_enabled)
        ),
        diffusion_stress_max_v=float(
            physics_payload.get("diffusion_stress_max_v", defaults.diffusion_stress_max_v)
        ),
        diffusion_stress_build_rate_per_s=float(
            physics_payload.get("diffusion_stress_build_rate_per_s", defaults.diffusion_stress_build_rate_per_s)
        ),
        diffusion_stress_decay_tau_s=float(
            physics_payload.get("diffusion_stress_decay_tau_s", defaults.diffusion_stress_decay_tau_s)
        ),
        diffusion_stress_current_scale_a=float(
            physics_payload.get("diffusion_stress_current_scale_a", defaults.diffusion_stress_current_scale_a)
        ),
        two_node_thermal_enabled=bool(
            physics_payload.get("two_node_thermal_enabled", defaults.two_node_thermal_enabled)
        ),
        core_surface_thermal_coupling_w_per_k=float(
            physics_payload.get("core_surface_thermal_coupling_w_per_k", defaults.core_surface_thermal_coupling_w_per_k)
        ),
        surface_thermal_mass_fraction=float(
            physics_payload.get("surface_thermal_mass_fraction", defaults.surface_thermal_mass_fraction)
        ),
        nonlinear_cooling_enabled=bool(
            physics_payload.get("nonlinear_cooling_enabled", defaults.nonlinear_cooling_enabled)
        ),
        nonlinear_cooling_delta_threshold_c=float(
            physics_payload.get("nonlinear_cooling_delta_threshold_c", defaults.nonlinear_cooling_delta_threshold_c)
        ),
        nonlinear_cooling_gain_per_c=float(
            physics_payload.get("nonlinear_cooling_gain_per_c", defaults.nonlinear_cooling_gain_per_c)
        ),
        reversible_heat_enabled=bool(
            physics_payload.get("reversible_heat_enabled", defaults.reversible_heat_enabled)
        ),
        reversible_heat_coeff_v_per_k=float(
            physics_payload.get("reversible_heat_coeff_v_per_k", defaults.reversible_heat_coeff_v_per_k)
        ),
        charge_resistance_multiplier=float(
            physics_payload.get("charge_resistance_multiplier", defaults.charge_resistance_multiplier)
        ),
        discharge_resistance_multiplier=float(
            physics_payload.get("discharge_resistance_multiplier", defaults.discharge_resistance_multiplier)
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
            balancing=_parse_balancing(payload),
            faults=_parse_faults(payload),
            thermal_zones=_parse_thermal_zones(payload),
            group_zone_assignments=tuple(int(zone_id) for zone_id in payload.get("group_zone_assignments", [])),
            group_labels=tuple(str(label) for label in payload.get("group_labels", [])),
            group_entity_ids=tuple(str(entity_id) for entity_id in payload.get("group_entity_ids", [])),
            physics=_parse_physics(payload),
        )
    )


def run_simulation_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    config = simulation_config_from_dict(payload)
    result = run_simulation(config)
    return simulation_result_to_dict(result)


def simulation_result_to_dict(result: SimulationResult) -> dict[str, Any]:
    return asdict(result)


def virtual_test_catalog_to_dict() -> dict[str, Any]:
    return {
        "tests": [
            asdict(definition)
            for definition in build_test_catalog()
        ]
    }


def vet_virtual_test_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    base_config = simulation_config_from_dict(payload["base_config"])
    vetting = vet_virtual_test(
        test_id=str(payload["test_id"]),
        parameters=dict(payload.get("parameters", {})),
        base_config=base_config,
    )
    return {
        "test_id": str(payload["test_id"]),
        "vetting_result": asdict(vetting),
    }


def run_virtual_test_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    base_config = simulation_config_from_dict(payload["base_config"])
    result = run_virtual_test(
        test_id=str(payload["test_id"]),
        parameters=dict(payload.get("parameters", {})),
        base_config=base_config,
    )
    return virtual_test_result_to_dict(result)
