from __future__ import annotations

from dataclasses import asdict
from typing import Any

from .chemistry import get_chemistry_preset
from .engine import run_simulation
from .physics.electrical import validate_electrical_model
from .system_presets import battery_system_preset_catalog_to_dict
from .test_catalog import build_experimental_test_catalog, build_test_catalog
from .test_runner import run_virtual_test, virtual_test_result_to_dict
from .test_vetting import vet_virtual_test
from .truth_data_manager import TruthDatasetRegistry
from .types import (
    BalancingConfig,
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    FaultConfig,
    FaultSpec,
    GroupVariationConfig,
    OcvLookupPoint,
    PhysicsConfig,
    RcBranchParams,
    SocLookupPoint,
    SimulationConfig,
    SimulationResult,
    ThermalZoneConfig,
)
from .validation import validate_current_profile, validate_simulation_config


def _warning_to_dict(warning: Any) -> dict[str, Any]:
    return {
        "code": warning.code,
        "message": warning.message,
        "severity": warning.severity,
    }


def _simulation_point_to_dict(point: Any) -> dict[str, Any]:
    """Serialize a point using one canonical desktop-facing schema.

    Canonical names carry unit suffixes. The embedded ``legacy_aliases`` block
    and flattened legacy keys are temporary compatibility shims while the
    desktop migrates off older field names.
    """

    canonical = {
        "time_s": point.time_s,
        "current_a": point.current_a,
        "pack_voltage_v": point.pack_voltage_v,
        "pack_power_w": point.pack_power_w,
        "pack_heat_w": point.pack_heat_w,
        "pack_temp_avg_c": point.pack_temp_avg_c,
        "pack_temp_max_c": point.pack_temp_max_c,
        "soc_avg": point.soc_avg,
        "soc_min": point.soc_min,
        "soc_max": point.soc_max,
        "group_voltage_v": point.group_voltage,
        "group_soc": point.group_soc,
        "group_true_soc": point.group_true_soc,
        "group_core_temp_c": point.group_core_temp_c,
        "group_surface_temp_c": point.group_surface_temp_c,
        "group_heat_w": point.group_heat_w,
        "group_hysteresis_v": point.group_hysteresis_v,
        "group_diffusion_stress": point.group_diffusion_stress,
        "group_effective_resistance_ohm": point.group_effective_resistance_ohm,
        "group_zone_ids": point.group_zone_ids,
        "group_labels": point.group_labels,
        "group_entity_ids": point.group_entity_ids,
        "group_voltage_min_v": point.group_voltage_min_v,
        "group_voltage_max_v": point.group_voltage_max_v,
        "weakest_group_index": point.weakest_group_index,
        "hottest_group_index": point.hottest_group_index,
        "balancing_active_groups": point.balancing_active_groups,
        "fault_active_groups": point.fault_active_groups,
        "group_balance_current_a": point.group_balance_current_a,
        "group_fault_flags": point.group_fault_flags,
        "zone_temp_max_c": {str(zone_id): temp_c for zone_id, temp_c in point.zone_temp_max_c.items()},
        "estimated_capacity_retention": point.estimated_capacity_retention,
        "estimated_resistance_multiplier": point.estimated_resistance_multiplier,
        "degradation_rate_indicator": point.degradation_rate_indicator,
        "legacy_aliases": {
            "soc": point.soc,
            "terminal_voltage_v": point.terminal_voltage_v,
            "power_w": point.power_w,
            "heat_w": point.heat_w,
            "temp_c": point.temp_c,
            "temp_avg": point.temp_avg,
            "temp_max": point.temp_max,
            "group_voltage": point.group_voltage,
            "group_temp": point.group_temp,
            "group_core_temp": point.group_core_temp,
            "group_surface_temp": point.group_surface_temp,
        },
    }
    canonical.update(canonical["legacy_aliases"])
    return canonical


def _parse_include_time_series(value: Any) -> bool:
    if value is None:
        return True
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"true", "1", "yes"}:
            return True
        if normalized in {"false", "0", "no"}:
            return False
    raise ValueError("include_time_series must be a boolean.")


def _parse_max_time_series_points(value: Any) -> int | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("max_time_series_points must be an integer.")
    numeric_value = float(value)
    if not numeric_value.is_integer():
        raise ValueError("max_time_series_points must be an integer.")
    count = int(numeric_value)
    if count < 2:
        raise ValueError("max_time_series_points must be >= 2 when provided.")
    return count


def _downsample_time_series(points: list[Any], max_points: int | None) -> list[Any]:
    if max_points is None or len(points) <= max_points:
        return list(points)

    last_index = len(points) - 1
    selected_indices: list[int] = []
    previous_index = -1
    for slot in range(max_points):
        target = round(slot * last_index / (max_points - 1))
        minimum_index = previous_index + 1
        maximum_index = last_index - (max_points - slot - 1)
        index = min(max(target, minimum_index), maximum_index)
        selected_indices.append(index)
        previous_index = index
    return [points[index] for index in selected_indices]


def _simulation_summary_to_dict(summary: Any) -> dict[str, Any]:
    """Serialize the canonical summary contract with explicit legacy shims."""

    canonical = {
        "runtime_s": summary.runtime_s,
        "delivered_energy_wh": summary.delivered_energy_wh,
        "delivered_capacity_ah": summary.delivered_capacity_ah,
        "final_soc_avg": summary.final_soc_avg,
        "soc_spread": summary.soc_spread,
        "min_group_voltage_v": summary.min_group_voltage_v,
        "max_group_temp_c": summary.max_group_temp_c,
        "max_core_temp_c": summary.max_core_temp_c,
        "max_surface_temp_c": summary.max_surface_temp_c,
        "temp_gradient_max_c": summary.temp_gradient_max_c,
        "max_diffusion_stress": summary.max_diffusion_stress,
        "weakest_group_index": summary.weakest_group_index,
        "hottest_group_index": summary.hottest_group_index,
        "hottest_zone_id": summary.hottest_zone_id,
        "max_zone_temp_c": summary.max_zone_temp_c,
        "capacity_retention": summary.capacity_retention,
        "resistance_growth": summary.resistance_growth,
        "termination_reason": summary.termination_reason,
        "warnings": [_warning_to_dict(item) for item in summary.warnings],
        "electrical_model_type": summary.electrical_model_type,
        "profile_used": summary.profile_used,
        "total_energy_wh": summary.total_energy_wh,
        "balancing_used": summary.balancing_used,
        "total_balance_ah": summary.total_balance_ah,
        "fault_count": summary.fault_count,
        "first_faulted_group_index": summary.first_faulted_group_index,
        "cumulative_charge_throughput_ah": summary.cumulative_charge_throughput_ah,
        "cumulative_discharge_throughput_ah": summary.cumulative_discharge_throughput_ah,
        "cumulative_high_soc_time_h": summary.cumulative_high_soc_time_h,
        "estimated_cycle_stress": summary.estimated_cycle_stress,
        "degradation_model_version": summary.degradation_model_version,
        "group_capacity_retention": summary.group_capacity_retention,
        "group_resistance_growth": summary.group_resistance_growth,
        "nonlinear_features_enabled": summary.nonlinear_features_enabled,
        "chemistry_name": summary.chemistry_name,
        "legacy_aliases": {
            "min_terminal_voltage_v": summary.min_terminal_voltage_v,
            "peak_temp_c": summary.peak_temp_c,
            "final_soc": summary.final_soc,
            "estimated_capacity_retention": summary.estimated_capacity_retention,
            "estimated_resistance_growth": summary.estimated_resistance_growth,
            "max_group_temp": summary.max_group_temp,
            "min_group_voltage": summary.min_group_voltage,
        },
    }
    canonical.update(canonical["legacy_aliases"])
    return canonical


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
    ocv_curve_payload = physics_payload.get("ocv_curve", defaults.ocv_curve)
    ocv_curve_items: list[OcvLookupPoint] = []
    for item in ocv_curve_payload or ():
        if isinstance(item, OcvLookupPoint):
            ocv_curve_items.append(item)
        else:
            ocv_curve_items.append(
                OcvLookupPoint(soc=float(item["soc"]), voltage_v=float(item["voltage_v"]))
            )
    ocv_curve = tuple(ocv_curve_items)
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
        ocv_curve=ocv_curve,
        resistance_vs_soc_enabled=bool(
            physics_payload.get("resistance_vs_soc_enabled", defaults.resistance_vs_soc_enabled)
        ),
        resistance_soc_curve=curve if curve else defaults.resistance_soc_curve,
        hysteresis_enabled=bool(
            physics_payload.get("hysteresis_enabled", defaults.hysteresis_enabled)
        ),
        hysteresis_max_voltage_v=float(
            physics_payload.get(
                "hysteresis_max_voltage_v",
                physics_payload.get("hysteresis_max_v", defaults.hysteresis_max_voltage_v),
            )
        ),
        hysteresis_response_rate_per_s=float(
            physics_payload.get(
                "hysteresis_response_rate_per_s",
                physics_payload.get("hysteresis_gain", defaults.hysteresis_response_rate_per_s),
            )
        ),
        hysteresis_relaxation_tau_s=float(
            physics_payload.get(
                "hysteresis_relaxation_tau_s",
                physics_payload.get("hysteresis_decay_s", defaults.hysteresis_relaxation_tau_s),
            )
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
            physics_payload.get(
                "diffusion_stress_max_v",
                physics_payload.get("stress_voltage_coeff", defaults.diffusion_stress_max_v),
            )
        ),
        diffusion_stress_build_rate_per_s=float(
            physics_payload.get(
                "diffusion_stress_build_rate_per_s",
                physics_payload.get("stress_gain", defaults.diffusion_stress_build_rate_per_s),
            )
        ),
        diffusion_stress_decay_tau_s=float(
            physics_payload.get(
                "diffusion_stress_decay_tau_s",
                physics_payload.get("stress_decay_s", defaults.diffusion_stress_decay_tau_s),
            )
        ),
        diffusion_stress_current_scale_a=float(
            physics_payload.get("diffusion_stress_current_scale_a", defaults.diffusion_stress_current_scale_a)
        ),
        diffusion_stress_resistance_coeff=float(
            physics_payload.get(
                "diffusion_stress_resistance_coeff",
                physics_payload.get("stress_resistance_coeff", defaults.diffusion_stress_resistance_coeff),
            )
        ),
        two_node_thermal_enabled=bool(
            physics_payload.get("two_node_thermal_enabled", defaults.two_node_thermal_enabled)
        ),
        core_surface_thermal_coupling_w_per_k=float(
            physics_payload.get(
                "core_surface_thermal_coupling_w_per_k",
                physics_payload.get("core_surface_coupling_w_per_k", defaults.core_surface_thermal_coupling_w_per_k),
            )
        ),
        surface_thermal_mass_fraction=float(
            physics_payload.get("surface_thermal_mass_fraction", defaults.surface_thermal_mass_fraction)
        ),
        core_thermal_mass_j_per_k=(
            float(physics_payload["core_thermal_mass_j_per_k"])
            if physics_payload.get("core_thermal_mass_j_per_k") is not None
            else None
        ),
        surface_thermal_mass_j_per_k=(
            float(physics_payload["surface_thermal_mass_j_per_k"])
            if physics_payload.get("surface_thermal_mass_j_per_k") is not None
            else None
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
    chemistry_payload = payload.get("chemistry_name", payload.get("chemistry"))
    chemistry_name = (
        str(chemistry_payload.get("name", "generic_liion"))
        if isinstance(chemistry_payload, dict)
        else str(chemistry_payload or "generic_liion")
    )
    chemistry = get_chemistry_preset(chemistry_name)
    return validate_simulation_config(
        SimulationConfig(
            cell_nominal_voltage=float(payload.get("cell_nominal_voltage", chemistry.cell_nominal_voltage)),
            cell_full_voltage=float(payload.get("cell_full_voltage", chemistry.cell_full_voltage)),
            cell_empty_voltage=float(payload.get("cell_empty_voltage", chemistry.cell_empty_voltage)),
            cell_cutoff_voltage=float(payload.get("cell_cutoff_voltage", chemistry.cell_cutoff_voltage)),
            cell_capacity_ah=float(payload["cell_capacity_ah"]),
            cells_in_series=int(payload["cells_in_series"]),
            cells_in_parallel=int(payload["cells_in_parallel"]),
            internal_resistance_ohm_per_cell=float(payload["internal_resistance_ohm_per_cell"]),
            ambient_temp_c=float(payload["ambient_temp_c"]),
            discharge_current_a=float(payload["discharge_current_a"]),
            duration_s=int(payload["duration_s"]),
            time_step_s=float(payload["time_step_s"]),
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
            chemistry_name=chemistry.name,
            physics=_parse_physics(payload),
        )
    )


def run_simulation_from_dict(payload: dict[str, Any]) -> dict[str, Any]:
    config = simulation_config_from_dict(payload)
    result = run_simulation(config)
    return simulation_result_to_dict(
        result,
        include_time_series=_parse_include_time_series(payload.get("include_time_series", True)),
        max_time_series_points=_parse_max_time_series_points(payload.get("max_time_series_points")),
    )


def simulation_result_to_dict(
    result: SimulationResult,
    *,
    include_time_series: bool = True,
    max_time_series_points: int | None = None,
) -> dict[str, Any]:
    top_level_group_labels: list[str] = []
    top_level_group_entity_ids: list[str] = []
    top_level_group_zone_ids: list[int] = []
    if result.time_series:
        top_level_group_labels = list(result.time_series[-1].group_labels)
        top_level_group_entity_ids = list(result.time_series[-1].group_entity_ids)
        top_level_group_zone_ids = list(result.time_series[-1].group_zone_ids)

    returned_time_series = _downsample_time_series(result.time_series, max_time_series_points) if include_time_series else []
    payload = {
        "pack_nominal_voltage_v": result.pack_nominal_voltage_v,
        "pack_capacity_ah": result.pack_capacity_ah,
        "theoretical_energy_wh": result.theoretical_energy_wh,
        "group_labels": top_level_group_labels,
        "group_entity_ids": top_level_group_entity_ids,
        "group_zone_ids": top_level_group_zone_ids,
        "summary": _simulation_summary_to_dict(result.summary),
        "time_series_metadata": {
            "original_point_count": len(result.time_series),
            "returned_point_count": len(returned_time_series),
            "downsampled": include_time_series and len(returned_time_series) != len(result.time_series),
        },
    }
    if include_time_series:
        payload["time_series"] = [_simulation_point_to_dict(point) for point in returned_time_series]
    return payload


def virtual_test_catalog_to_dict() -> dict[str, Any]:
    return {
        "tests": [
            asdict(definition)
            for definition in build_test_catalog()
        ],
        "experimental_tests": [
            asdict(definition)
            for definition in build_experimental_test_catalog()
        ],
    }


def battery_system_preset_catalog_to_bridge_dict() -> dict[str, Any]:
    return battery_system_preset_catalog_to_dict()


def truth_dataset_catalog_to_dict() -> dict[str, Any]:
    registry = TruthDatasetRegistry()
    return {
        "truth_data_directory": str(registry.truth_data_dir),
        "datasets": [asdict(item) for item in registry.list_truth_datasets()],
        "scan_errors": list(registry.scan_errors),
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
