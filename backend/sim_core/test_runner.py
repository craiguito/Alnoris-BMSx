from __future__ import annotations

from dataclasses import asdict, replace
from statistics import mean
from typing import Any

from .engine import run_simulation
from .test_catalog import get_test_definition
from .test_vetting import vet_virtual_test
from .tests_framework import TestVettingResult, VirtualTestResult, VirtualTestScenarioResult
from .types import (
    BalancingConfig,
    CurrentProfile,
    CurrentProfilePoint,
    FaultConfig,
    FaultSpec,
    GroupVariationConfig,
    SimulationConfig,
    SimulationResult,
    ThermalZoneConfig,
)
from .validation import validate_simulation_config


def _result_metric(result: SimulationResult, key: str) -> float:
    if key == "average_voltage":
        return mean(point.pack_voltage_v for point in result.time_series)
    if key == "max_temp":
        return result.summary.max_group_temp_c
    if key == "runtime":
        return result.summary.runtime_s
    raise KeyError(key)


def _run(config: SimulationConfig) -> SimulationResult:
    return run_simulation(validate_simulation_config(config))


def _with_base(config: SimulationConfig, **updates: Any) -> SimulationConfig:
    return replace(config, **updates)


def _profile(*pairs: tuple[int, float]) -> CurrentProfile:
    return CurrentProfile(points=tuple(CurrentProfilePoint(time_s=int(t), current_a=float(i)) for t, i in pairs))


def run_virtual_test(test_id: str, parameters: dict[str, Any], base_config: SimulationConfig) -> VirtualTestResult:
    definition = get_test_definition(test_id)
    vetting = vet_virtual_test(test_id, parameters, base_config)
    if not vetting.is_valid:
        return VirtualTestResult(
            test_id=test_id,
            test_name=definition.display_name,
            parameters_used=vetting.normalized_parameters,
            vetting_result=vetting,
            summary_metrics={},
            warnings=vetting.warnings,
        )

    p = vetting.normalized_parameters
    sub_results: list[VirtualTestScenarioResult] = []
    comparison_series: list[dict[str, Any]] = []
    warnings = list(vetting.warnings)
    pass_fail: dict[str, bool] = {}
    primary_result: SimulationResult | None = None
    summary_metrics: dict[str, Any] = {}

    if test_id == "constant_current_discharge":
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=float(p["current_a"]),
            duration_s=int(p["max_duration_s"]),
            time_step_s=max(1, int(p["time_step_s"])),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            current_profile=None,
        ))
        summary_metrics = {
            "runtime_s": primary_result.summary.runtime_s,
            "delivered_energy_wh": primary_result.summary.delivered_energy_wh,
            "average_voltage_v": _result_metric(primary_result, "average_voltage"),
            "cutoff_reason": primary_result.summary.termination_reason,
        }
        pass_fail["reached_cutoff"] = primary_result.summary.termination_reason == "group_cutoff_voltage_reached"
    elif test_id == "constant_current_charge":
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=-abs(float(p["current_a"])),
            duration_s=int(p["max_duration_s"]),
            time_step_s=max(1, int(p["time_step_s"])),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            current_profile=_profile((0, -abs(float(p["current_a"])))),
        ))
        summary_metrics = {
            "charge_time_s": primary_result.summary.runtime_s,
            "energy_in_wh": -primary_result.summary.delivered_energy_wh,
            "final_soc": primary_result.summary.final_soc_avg,
        }
        pass_fail["reached_soc_ceiling"] = primary_result.summary.termination_reason == "soc_ceiling_reached"
    elif test_id == "pulse_power":
        pulse_count = int(p["pulse_count"])
        pulse_duration_s = float(p["pulse_duration_s"])
        rest_duration_s = float(p["rest_duration_s"])
        profile_points: list[tuple[int, float]] = []
        time_s = 0
        for _ in range(pulse_count):
            profile_points.append((time_s, float(p["pulse_current_a"])))
            time_s += int(pulse_duration_s)
            profile_points.append((time_s, 0.0))
            time_s += int(rest_duration_s)
        primary_result = _run(_with_base(
            base_config,
            duration_s=max(time_s, 1),
            time_step_s=max(1, int(p["time_step_s"])),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            current_profile=_profile(*profile_points),
        ))
        voltage_sag = primary_result.time_series[0].pack_voltage_v - min(point.pack_voltage_v for point in primary_result.time_series)
        summary_metrics = {
            "voltage_sag_v": voltage_sag,
            "recovered_voltage_v": primary_result.time_series[-1].pack_voltage_v,
            "max_temp_c": primary_result.summary.max_group_temp_c,
        }
    elif test_id == "ocv_relaxation":
        preload_duration_s = float(p["preload_duration_s"])
        profile_points = [
            (0, float(p["preload_current_a"])),
            (int(preload_duration_s), 0.0),
        ]
        primary_result = _run(_with_base(
            base_config,
            duration_s=int(preload_duration_s + float(p["rest_duration_s"])),
            time_step_s=max(1, int(p["time_step_s"])),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            current_profile=_profile(*profile_points),
        ))
        rest_start = next((point for point in primary_result.time_series if point.time_s >= int(preload_duration_s)), primary_result.time_series[-1])
        summary_metrics = {
            "recovered_voltage_v": primary_result.time_series[-1].pack_voltage_v,
            "relaxation_delta_v": primary_result.time_series[-1].pack_voltage_v - rest_start.pack_voltage_v,
            "rest_duration_s": float(p["rest_duration_s"]),
        }
    elif test_id == "rate_capability":
        currents = [abs(float(value)) for value in p["current_list_a"]]
        for current_a in currents:
            scenario_result = _run(_with_base(
                base_config,
                discharge_current_a=current_a,
                duration_s=int(p["max_duration_s"]),
                time_step_s=max(1, int(p["time_step_s"])),
                initial_soc=float(p["initial_soc"]),
                ambient_temp_c=float(p["ambient_temp_c"]),
                current_profile=None,
            ))
            sub_results.append(VirtualTestScenarioResult(
                name=f"{current_a:.2f} A",
                summary_metrics={
                    "current_a": current_a,
                    "capacity_ah": scenario_result.summary.delivered_capacity_ah,
                    "energy_wh": scenario_result.summary.delivered_energy_wh,
                },
                simulation_result=scenario_result,
            ))
            comparison_series.append({
                "name": "Delivered Capacity",
                "x": current_a,
                "y": scenario_result.summary.delivered_capacity_ah,
            })
        primary_result = sub_results[0].simulation_result if sub_results else None
        summary_metrics = {
            "scenario_count": len(sub_results),
            "best_capacity_ah": max((scenario.summary_metrics["capacity_ah"] for scenario in sub_results), default=0.0),
        }
    elif test_id == "thermal_stress":
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=float(p["current_a"]),
            duration_s=int(p["duration_s"]),
            time_step_s=max(1, int(p["time_step_s"])),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
        ))
        summary_metrics = {
            "max_temp_c": primary_result.summary.max_group_temp_c,
            "hottest_group_index": primary_result.summary.hottest_group_index,
            "temp_rise_c": primary_result.summary.max_group_temp_c - float(p["ambient_temp_c"]),
        }
    elif test_id == "storage_self_discharge":
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=0.0,
            duration_s=int(p["storage_duration_s"]),
            time_step_s=max(1, int(p["time_step_s"])),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            current_profile=None,
            physics=replace(base_config.physics, self_discharge_per_day=float(p["self_discharge_per_day"])),
        ))
        summary_metrics = {
            "soc_loss": float(p["initial_soc"]) - primary_result.summary.final_soc_avg,
            "capacity_retention": primary_result.summary.capacity_retention,
            "final_temp_c": primary_result.time_series[-1].pack_temp_avg_c,
        }
    elif test_id == "fault_response":
        faults = tuple(
            FaultSpec(
                fault_type=str(item["fault_type"]),
                group_index=int(item["group_index"]),
                factor=float(item["factor"]),
                start_time_s=float(item.get("start_time_s", 0.0)),
                end_time_s=float(item["end_time_s"]) if item.get("end_time_s") is not None else None,
            )
            for item in p["fault_specs"]
        )
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=float(p["current_a"]),
            duration_s=int(p["duration_s"]),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            faults=FaultConfig(faults=faults),
        ))
        summary_metrics = {
            "weakest_group_index": primary_result.summary.weakest_group_index,
            "max_temp_c": primary_result.summary.max_group_temp_c,
            "min_group_voltage_v": primary_result.summary.min_group_voltage_v,
        }
    elif test_id == "balancing_effectiveness":
        spread = max(float(p["initial_soc_spread"]), 0.0)
        variation = GroupVariationConfig(initial_soc_variation_abs=spread * 0.5)
        no_balance = _run(_with_base(
            base_config,
            discharge_current_a=0.0,
            duration_s=int(p["run_duration_s"]),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            group_variation=variation,
        ))
        with_balance = _run(_with_base(
            base_config,
            discharge_current_a=0.0,
            duration_s=int(p["run_duration_s"]),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            group_variation=variation,
            balancing=BalancingConfig(
                enabled=True,
                mode="passive",
                soc_threshold=float(p["soc_threshold"]),
                bleed_current_a=float(p["bleed_current_a"]),
                max_active_groups=None,
            ),
        ))
        primary_result = with_balance
        sub_results = [
            VirtualTestScenarioResult(
                name="Without Balancing",
                summary_metrics={"ending_soc_spread": no_balance.summary.soc_spread},
                simulation_result=no_balance,
            ),
            VirtualTestScenarioResult(
                name="With Balancing",
                summary_metrics={"ending_soc_spread": with_balance.summary.soc_spread},
                simulation_result=with_balance,
            ),
        ]
        summary_metrics = {
            "starting_soc_spread": no_balance.time_series[0].soc_max - no_balance.time_series[0].soc_min,
            "ending_soc_spread": with_balance.summary.soc_spread,
            "balancing_ah": with_balance.summary.total_balance_ah,
        }
        pass_fail["spread_reduced"] = with_balance.summary.soc_spread < no_balance.summary.soc_spread
    elif test_id == "thermal_zone_comparison":
        thermal_zones = base_config.thermal_zones
        assignments = base_config.group_zone_assignments
        if not thermal_zones:
            multipliers = [float(value) for value in p["zone_cooling_multipliers"]]
            split = max(1, (base_config.group_count or base_config.cells_in_series) // max(len(multipliers), 1))
            thermal_zones = tuple(
                ThermalZoneConfig(zone_id=index + 1, name=f"Zone {index + 1}", cooling_coeff_multiplier=value)
                for index, value in enumerate(multipliers)
            )
            assignments = tuple(min(index // split, len(multipliers) - 1) + 1 for index in range(base_config.group_count or base_config.cells_in_series))
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=float(p["current_a"]),
            duration_s=int(p["duration_s"]),
            initial_soc=float(p["initial_soc"]),
            ambient_temp_c=float(p["ambient_temp_c"]),
            thermal_zones=thermal_zones,
            group_zone_assignments=assignments,
        ))
        summary_metrics = {
            "hottest_zone_id": primary_result.summary.hottest_zone_id,
            "max_zone_temp_c": primary_result.summary.max_zone_temp_c,
            "zone_count": len(thermal_zones),
        }
    else:
        raise ValueError(f"Unsupported virtual test id: {test_id}")

    return VirtualTestResult(
        test_id=test_id,
        test_name=definition.display_name,
        parameters_used=p,
        vetting_result=vetting,
        summary_metrics=summary_metrics,
        warnings=warnings,
        pass_fail_indicators=pass_fail,
        primary_result=primary_result,
        sub_results=sub_results,
        comparison_series=comparison_series,
    )


def virtual_test_result_to_dict(result: VirtualTestResult) -> dict[str, Any]:
    payload = {
        "test_id": result.test_id,
        "test_name": result.test_name,
        "parameters_used": result.parameters_used,
        "vetting_result": {
            "is_valid": result.vetting_result.is_valid,
            "errors": result.vetting_result.errors,
            "warnings": result.vetting_result.warnings,
            "normalized_parameters": result.vetting_result.normalized_parameters,
        },
        "summary_metrics": result.summary_metrics,
        "warnings": result.warnings,
        "pass_fail_indicators": result.pass_fail_indicators,
        "comparison_series": result.comparison_series,
        "sub_results": [
            {
                "name": item.name,
                "summary_metrics": item.summary_metrics,
                "simulation_result": asdict(item.simulation_result) if item.simulation_result is not None else None,
            }
            for item in result.sub_results
        ],
    }
    if result.primary_result is not None:
        payload["primary_result"] = asdict(result.primary_result)
    return payload
