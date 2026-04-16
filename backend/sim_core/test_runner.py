from __future__ import annotations

from dataclasses import replace
from statistics import mean
from typing import Any

from .calibration import (
    build_validation_config,
    load_truth_dataset,
)
from .engine import run_simulation
from .test_catalog import get_test_definition
from .test_vetting import vet_virtual_test
from .tests_framework import TestVettingResult, VirtualTestResult, VirtualTestScenarioResult
from .validation_scorecard import (
    build_validation_scorecard,
    summarize_validation_scorecards,
)
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
from .validation import require_integral_seconds, validate_simulation_config


_MODEL_VALIDATION_SUMMARY_KEY_BY_METRIC = {
    "rmse_voltage": "rmse_voltage_v",
    "max_abs_voltage_error": "max_abs_voltage_error_v",
    "energy_error": "energy_error_fraction",
    "temp_rmse": "rmse_temp_c",
    "final_soc_error": "final_soc_error",
    "final_voltage_error": "final_voltage_error_v",
}
_MODEL_VALIDATION_PASS_FAIL_KEY_BY_METRIC = {
    "rmse_voltage": "voltage_rmse_within_limit",
    "max_abs_voltage_error": "max_abs_voltage_error_within_limit",
    "energy_error": "energy_error_within_limit",
    "temp_rmse": "temp_rmse_within_limit",
    "final_soc_error": "final_soc_error_within_limit",
    "final_voltage_error": "final_voltage_error_within_limit",
}


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


def _validation_thresholds(parameters: dict[str, Any]) -> dict[str, float]:
    thresholds: dict[str, float] = {}
    for key in (
        "max_voltage_rmse_v",
        "max_abs_voltage_error_v",
        "max_energy_error_fraction",
        "max_temp_rmse_c",
        "max_final_soc_error",
        "max_final_voltage_error_v",
    ):
        if parameters.get(key) is not None:
            thresholds[key] = float(parameters[key])
    return thresholds


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
            time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
            time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
            time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
            time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
                time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
    elif test_id == "model_validation":
        requested_metrics = [str(metric) for metric in p["metrics"]]
        thresholds = _validation_thresholds(p)
        resolved_datasets = [dict(item) for item in p.get("resolved_datasets", [])]
        if not resolved_datasets and p.get("dataset_path"):
            dataset_path = str(p["dataset_path"])
            dataset = load_truth_dataset(dataset_path)
            resolved_datasets = [{
                "dataset_id": str(dataset.metadata.get("dataset_id", "legacy_truth_dataset")),
                "display_name": str(dataset.metadata.get("display_name", "Legacy Truth Dataset")),
                "dataset_path": dataset_path,
                "source_type": "path",
                "record_count": len(dataset.records),
                "metadata": dict(dataset.metadata),
            }]

        scorecards = []
        primary_label = ""
        primary_dataset_id = ""
        primary_profile_point_count = 0
        primary_duration_s = 0

        for dataset_payload in resolved_datasets:
            dataset_path = str(dataset_payload["dataset_path"])
            dataset = load_truth_dataset(dataset_path)
            validation_config = build_validation_config(base_config, dataset)
            scenario_result = _run(validation_config)

            if primary_result is None:
                primary_result = scenario_result
                primary_label = str(dataset_payload.get("display_name", dataset.metadata.get("display_name", dataset_path)))
                primary_dataset_id = str(dataset_payload.get("dataset_id", dataset.metadata.get("dataset_id", dataset_path)))
                primary_profile_point_count = len(validation_config.current_profile.points) if validation_config.current_profile else 0
                primary_duration_s = validation_config.duration_s

            scorecards.append(
                build_validation_scorecard(
                    scenario_result,
                    dataset,
                    dataset_path=dataset_path,
                    base_config=base_config,
                    requested_metrics=requested_metrics,
                    thresholds=thresholds,
                )
            )

        validation_summary = summarize_validation_scorecards(scorecards)
        summary_metrics = {
            "dataset_record_count": int(p.get("dataset_record_count", 0)),
            "validation_dataset_count": validation_summary.dataset_count,
            "validation_passed_count": validation_summary.passed_count,
            "validation_failed_count": validation_summary.failed_count,
            "validation_overall_status": validation_summary.overall_status,
            "profile_point_count": primary_profile_point_count,
            "validation_duration_s": primary_duration_s,
            "primary_validation_dataset_id": primary_dataset_id,
            "primary_validation_dataset": primary_label,
        }

        if len(scorecards) == 1:
            scorecard = scorecards[0]
            summary_metrics["dataset_record_count"] = len(load_truth_dataset(scorecard.dataset_path).records)
            for metric in scorecard.metric_results:
                summary_key = _MODEL_VALIDATION_SUMMARY_KEY_BY_METRIC.get(metric.metric_id)
                pass_fail_key = _MODEL_VALIDATION_PASS_FAIL_KEY_BY_METRIC.get(metric.metric_id)
                if summary_key is not None:
                    summary_metrics[summary_key] = metric.value
                if pass_fail_key is not None and metric.passed is not None:
                    pass_fail[pass_fail_key] = metric.passed
        else:
            for headline_metric in validation_summary.headline_metrics:
                summary_key = _MODEL_VALIDATION_SUMMARY_KEY_BY_METRIC.get(headline_metric.metric_id)
                if summary_key is None:
                    continue
                summary_metrics[f"average_{summary_key}"] = headline_metric.average_value
                summary_metrics[f"best_{summary_key}"] = headline_metric.best_value
                summary_metrics[f"worst_{summary_key}"] = headline_metric.worst_value

            for metric_name in requested_metrics:
                metric_passes = [
                    metric.passed
                    for scorecard in scorecards
                    for metric in scorecard.metric_results
                    if metric.metric_id == metric_name and metric.passed is not None
                ]
                pass_fail_key = _MODEL_VALIDATION_PASS_FAIL_KEY_BY_METRIC.get(metric_name)
                if pass_fail_key is not None and metric_passes:
                    pass_fail[pass_fail_key] = all(metric_passes)

        pass_fail["validation_passed"] = validation_summary.overall_status == "pass"
        warnings.extend(validation_summary.key_warnings)
        if len(scorecards) > 1 and primary_label:
            warnings.append(
                f"Charts and replay traces show {primary_label}; additional datasets are summarized in the validation scorecard."
            )
    elif test_id == "thermal_stress":
        primary_result = _run(_with_base(
            base_config,
            discharge_current_a=float(p["current_a"]),
            duration_s=int(p["duration_s"]),
            time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
            time_step_s=require_integral_seconds(p["time_step_s"], "time_step_s"),
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
        validation_scorecards=scorecards if test_id == "model_validation" else [],
        validation_summary=validation_summary if test_id == "model_validation" else None,
    )


def virtual_test_result_to_dict(result: VirtualTestResult) -> dict[str, Any]:
    from .bridge import simulation_result_to_dict

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
                "simulation_result": simulation_result_to_dict(item.simulation_result) if item.simulation_result is not None else None,
            }
            for item in result.sub_results
        ],
    }
    if result.validation_scorecards:
        from .validation_scorecard import validation_scorecard_to_dict

        payload["validation_scorecards"] = [
            validation_scorecard_to_dict(scorecard)
            for scorecard in result.validation_scorecards
        ]
    if result.validation_summary is not None:
        from .validation_scorecard import validation_summary_to_dict

        payload["validation_summary"] = validation_summary_to_dict(result.validation_summary)
    if result.primary_result is not None:
        payload["primary_result"] = simulation_result_to_dict(result.primary_result)
    return payload
