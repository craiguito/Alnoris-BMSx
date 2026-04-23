from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path
from statistics import median
from typing import Any, Sequence

from .calibration import CalibratedParameters, apply_calibration_to_simulation_config, calibrate_parameters
from .calibration_profiles import (
    CalibrationObjectiveResult,
    CalibrationProfile,
    calibration_objective_result_to_dict,
    calibration_profile_to_dict,
    evaluate_calibration_objective,
    get_calibration_profile,
)
from .truth_data_manager import DEFAULT_TRUTH_DATA_DIR, TruthDatasetRegistry
from .validation_pack import (
    ValidationPackManifest,
    build_single_cell_validation_base_config,
    resolve_validation_manifest,
    run_validation_manifest_case,
    validation_manifest_to_dict,
)
from .validation_threshold_profiles import (
    ValidationThresholdProfile,
    get_validation_threshold_profile,
    validation_threshold_profile_to_dict,
)


@dataclass(frozen=True)
class CalibrationCandidate:
    candidate_id: str
    anchor_dataset_ids: tuple[str, ...]
    rc_branch_count: int
    electro_blend: float
    thermal_blend: float
    calibrated_parameters: CalibratedParameters


@dataclass(frozen=True)
class RoomEnvelopeCalibrationArtifact:
    manifest: ValidationPackManifest
    threshold_profile: ValidationThresholdProfile
    calibration_profile: CalibrationProfile
    truth_data_dir: str
    baseline_validation: dict[str, Any]
    selected_validation: dict[str, Any]
    selected_candidate: dict[str, Any]
    candidate_scores: tuple[dict[str, Any], ...]
    per_dataset_diagnostics: tuple[dict[str, Any], ...]
    recommendation_text: str
    diagnostics_markdown: str


def _infer_battery_family(metadata: dict[str, Any]) -> str:
    family = str(metadata.get("source_battery_id", "")).strip()
    if family:
        return family
    dataset_id = str(metadata.get("dataset_id", "")).strip().lower()
    for token in dataset_id.split("_"):
        if token.startswith("b") and len(token) == 5:
            return token.upper()
    return dataset_id or "dataset"


def _representative_anchor_ids(manifest: ValidationPackManifest, registry: TruthDatasetRegistry) -> tuple[str, ...]:
    chosen_by_family: dict[str, tuple[int, str]] = {}
    for dataset_id in manifest.dataset_ids:
        resolved = registry.get_truth_dataset(dataset_id)
        family = _infer_battery_family(resolved.metadata)
        cycle_index = int(resolved.metadata.get("source_cycle_index", 0))
        existing = chosen_by_family.get(family)
        if existing is None or cycle_index < existing[0]:
            chosen_by_family[family] = (cycle_index, dataset_id)
    ordered = [item[1] for item in sorted(chosen_by_family.values(), key=lambda entry: (entry[0], entry[1]))]
    if manifest.calibration_dataset_id not in ordered:
        ordered.insert(0, manifest.calibration_dataset_id)
    return tuple(dict.fromkeys(ordered))


def _median_lookup_points(curves: Sequence[Sequence[Any]]) -> tuple[Any, ...]:
    if not curves:
        return ()
    point_count = min(len(curve) for curve in curves)
    if point_count == 0:
        return ()
    point_type = type(curves[0][0])
    result: list[Any] = []
    for index in range(point_count):
        sample = curves[0][index]
        if hasattr(sample, "voltage_v"):
            result.append(
                point_type(
                    soc=float(sample.soc),
                    voltage_v=median(float(curve[index].voltage_v) for curve in curves),
                )
            )
        else:
            result.append(
                point_type(
                    soc=float(sample.soc),
                    multiplier=median(float(curve[index].multiplier) for curve in curves),
                )
            )
    return tuple(result)


def _median_rc_branches(branch_sets: Sequence[Sequence[Any]]) -> tuple[Any, ...]:
    if not branch_sets:
        return ()
    point_count = min(len(branches) for branches in branch_sets)
    if point_count == 0:
        return ()
    branch_type = type(branch_sets[0][0])
    result: list[Any] = []
    for index in range(point_count):
        result.append(
            branch_type(
                resistance_ohm=median(float(branches[index].resistance_ohm) for branches in branch_sets),
                capacitance_f=median(float(branches[index].capacitance_f) for branches in branch_sets),
            )
        )
    return tuple(result)


def combine_calibrated_parameters(parameters: Sequence[CalibratedParameters]) -> CalibratedParameters:
    if not parameters:
        raise ValueError("combine_calibrated_parameters requires at least one parameter set.")
    if len(parameters) == 1:
        return parameters[0]
    return CalibratedParameters(
        ocv_curve=_median_lookup_points([item.ocv_curve for item in parameters]),
        base_resistance_ohm_per_cell=median(item.base_resistance_ohm_per_cell for item in parameters),
        resistance_soc_curve=_median_lookup_points([item.resistance_soc_curve for item in parameters]),
        resistance_temperature_alpha_per_c=median(item.resistance_temperature_alpha_per_c for item in parameters),
        rc_branches=_median_rc_branches([item.rc_branches for item in parameters]),
        core_thermal_mass_j_per_k=median(item.core_thermal_mass_j_per_k for item in parameters),
        surface_thermal_mass_j_per_k=median(item.surface_thermal_mass_j_per_k for item in parameters),
        cooling_coeff_w_per_k=median(item.cooling_coeff_w_per_k for item in parameters),
        capacity_fade_per_throughput_ah=median(item.capacity_fade_per_throughput_ah for item in parameters),
        resistance_growth_per_throughput_ah=median(item.resistance_growth_per_throughput_ah for item in parameters),
        calendar_capacity_fade_per_hour=median(item.calendar_capacity_fade_per_hour for item in parameters),
        calendar_resistance_growth_per_hour=median(item.calendar_resistance_growth_per_hour for item in parameters),
        diagnostics={
            "source": "median_combination",
            "source_count": len(parameters),
            "source_dataset_ids": [
                item.diagnostics.get("dataset_id")
                for item in parameters
                if item.diagnostics.get("dataset_id")
            ],
        },
    )


def _candidate_pool(
    manifest: ValidationPackManifest,
    profile: CalibrationProfile,
    registry: TruthDatasetRegistry,
) -> tuple[CalibrationCandidate, ...]:
    representative_ids = _representative_anchor_ids(manifest, registry)
    candidates: list[CalibrationCandidate] = []
    for rc_branch_count in profile.rc_branch_count_options:
        single_anchor_params: list[tuple[str, CalibratedParameters]] = []
        for dataset_id in representative_ids:
            resolved = registry.get_truth_dataset(dataset_id)
            calibrated = calibrate_parameters(resolved.dataset, rc_branch_count=rc_branch_count)
            calibrated = CalibratedParameters(
                ocv_curve=calibrated.ocv_curve,
                base_resistance_ohm_per_cell=calibrated.base_resistance_ohm_per_cell,
                resistance_soc_curve=calibrated.resistance_soc_curve,
                resistance_temperature_alpha_per_c=calibrated.resistance_temperature_alpha_per_c,
                rc_branches=calibrated.rc_branches,
                core_thermal_mass_j_per_k=calibrated.core_thermal_mass_j_per_k,
                surface_thermal_mass_j_per_k=calibrated.surface_thermal_mass_j_per_k,
                cooling_coeff_w_per_k=calibrated.cooling_coeff_w_per_k,
                capacity_fade_per_throughput_ah=calibrated.capacity_fade_per_throughput_ah,
                resistance_growth_per_throughput_ah=calibrated.resistance_growth_per_throughput_ah,
                calendar_capacity_fade_per_hour=calibrated.calendar_capacity_fade_per_hour,
                calendar_resistance_growth_per_hour=calibrated.calendar_resistance_growth_per_hour,
                diagnostics={
                    **dict(calibrated.diagnostics),
                    "dataset_id": dataset_id,
                    "display_name": resolved.dataset_display_name,
                    "rc_branch_count": rc_branch_count,
                },
            )
            single_anchor_params.append((dataset_id, calibrated))
            for electro_blend in profile.electro_blend_options:
                for thermal_blend in profile.thermal_blend_options:
                    candidates.append(
                        CalibrationCandidate(
                            candidate_id=(
                                f"{dataset_id}_rc{rc_branch_count}_eb{int(round(electro_blend * 100)):02d}"
                                f"_tb{int(round(thermal_blend * 100)):02d}"
                            ),
                            anchor_dataset_ids=(dataset_id,),
                            rc_branch_count=rc_branch_count,
                            electro_blend=electro_blend,
                            thermal_blend=thermal_blend,
                            calibrated_parameters=calibrated,
                        )
                    )
        if profile.use_family_median_candidate and single_anchor_params:
            combined = combine_calibrated_parameters([item[1] for item in single_anchor_params])
            for electro_blend in profile.electro_blend_options:
                for thermal_blend in profile.thermal_blend_options:
                    candidates.append(
                        CalibrationCandidate(
                            candidate_id=(
                                f"family_median_rc{rc_branch_count}_eb{int(round(electro_blend * 100)):02d}"
                                f"_tb{int(round(thermal_blend * 100)):02d}"
                            ),
                            anchor_dataset_ids=tuple(item[0] for item in single_anchor_params),
                            rc_branch_count=rc_branch_count,
                            electro_blend=electro_blend,
                            thermal_blend=thermal_blend,
                            calibrated_parameters=combined,
                        )
                    )
    return tuple(candidates)


def _scorecards_by_dataset(payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(scorecard.get("dataset_id", "")): scorecard
        for scorecard in payload.get("validation_scorecards", [])
    }


def _metric_map(scorecard: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(metric.get("metric_id", "")): metric
        for metric in scorecard.get("metric_results", [])
    }


def _status_rank(status: str) -> int:
    return {"fail": 0, "warning": 1, "pass": 2}.get(str(status).lower(), -1)


def _compare_dataset_diagnostics(pre_payload: dict[str, Any], post_payload: dict[str, Any]) -> tuple[dict[str, Any], ...]:
    diagnostics: list[dict[str, Any]] = []
    pre_by_dataset = _scorecards_by_dataset(pre_payload)
    post_by_dataset = _scorecards_by_dataset(post_payload)
    for dataset_id, post_scorecard in post_by_dataset.items():
        pre_scorecard = pre_by_dataset.get(dataset_id, {})
        pre_metrics = _metric_map(pre_scorecard)
        post_metrics = _metric_map(post_scorecard)
        metric_deltas: dict[str, dict[str, Any]] = {}
        ranked_changes: list[tuple[float, str]] = []
        for metric_id, post_metric in post_metrics.items():
            pre_metric = pre_metrics.get(metric_id)
            if pre_metric is None:
                continue
            pre_value = float(pre_metric.get("value", 0.0))
            post_value = float(post_metric.get("value", 0.0))
            threshold = abs(float(post_metric.get("threshold_value") or pre_metric.get("threshold_value") or 1.0))
            raw_delta = post_value - pre_value
            normalized_delta = raw_delta / max(threshold, 1.0e-9)
            ranked_changes.append((normalized_delta, metric_id))
            metric_deltas[metric_id] = {
                "display_name": post_metric.get("display_name", metric_id),
                "pre_value": pre_value,
                "post_value": post_value,
                "delta": raw_delta,
                "normalized_delta": normalized_delta,
                "threshold_value": threshold,
                "pre_passed": pre_metric.get("passed"),
                "post_passed": post_metric.get("passed"),
            }
        ranked_changes.sort(key=lambda item: item[0])
        most_improved = ranked_changes[0][1] if ranked_changes else ""
        most_worsened = ranked_changes[-1][1] if ranked_changes else ""
        diagnostics.append(
            {
                "dataset_id": dataset_id,
                "dataset_display_name": post_scorecard.get("dataset_display_name", dataset_id),
                "pre_overall_status": pre_scorecard.get("overall_status", ""),
                "post_overall_status": post_scorecard.get("overall_status", ""),
                "status_change": _status_rank(str(post_scorecard.get("overall_status", ""))) - _status_rank(str(pre_scorecard.get("overall_status", ""))),
                "metric_deltas": metric_deltas,
                "most_improved_metric": most_improved,
                "most_worsened_metric": most_worsened,
            }
        )
    return tuple(diagnostics)


def _diagnostics_markdown(
    manifest: ValidationPackManifest,
    threshold_profile: ValidationThresholdProfile,
    calibration_profile: CalibrationProfile,
    diagnostics: Sequence[dict[str, Any]],
    recommendation_text: str,
) -> str:
    lines = [
        f"# {manifest.display_name} Calibration Diagnostics",
        "",
        f"- Threshold profile: `{threshold_profile.profile_id}`",
        f"- Calibration profile: `{calibration_profile.profile_id}`",
        f"- Recommendation: {recommendation_text}",
        "",
        "| Dataset | Pre | Post | Best improvement | Worst regression |",
        "| --- | --- | --- | --- | --- |",
    ]
    for item in diagnostics:
        lines.append(
            f"| {item['dataset_display_name']} | {str(item['pre_overall_status']).upper()} | "
            f"{str(item['post_overall_status']).upper()} | {item['most_improved_metric'] or 'n/a'} | "
            f"{item['most_worsened_metric'] or 'n/a'} |"
        )
    return "\n".join(lines)


def _recommendation_text(
    baseline_objective: CalibrationObjectiveResult,
    selected_objective: CalibrationObjectiveResult,
    baseline_payload: dict[str, Any],
    selected_payload: dict[str, Any],
) -> str:
    baseline_summary = baseline_payload.get("validation_summary", {})
    selected_summary = selected_payload.get("validation_summary", {})
    objective_delta = baseline_objective.total_score - selected_objective.total_score
    pass_delta = int(selected_summary.get("passed_count", 0)) - int(baseline_summary.get("passed_count", 0))
    if pass_delta > 0:
        return "Room-temperature validation envelope materially improved and is more suitable for comparative decision support."
    if objective_delta > 0.15:
        return "Room-temperature validation envelope materially improved, but it still does not clear pass thresholds across the room canonical pack."
    if objective_delta > 0.03:
        return "Room-temperature validation envelope improved, but still needs more electrical fidelity before it can be treated as a strong pass basis."
    return "Calibration search did not materially improve the room-temperature envelope; voltage fidelity remains the main blocker."


def calibrate_room_envelope(
    *,
    manifest_id_or_path: str = "nasa_room_canonical",
    calibration_profile_id: str | None = None,
    threshold_profile_id: str | None = None,
    truth_data_dir: str | Path | None = None,
    manifest_dir: str | Path | None = None,
) -> RoomEnvelopeCalibrationArtifact:
    manifest = resolve_validation_manifest(manifest_id_or_path, manifest_dir=manifest_dir)
    threshold_profile = get_validation_threshold_profile(threshold_profile_id or manifest.recommended_threshold_profile)
    calibration_profile = get_calibration_profile(calibration_profile_id)
    registry = TruthDatasetRegistry(truth_data_dir or DEFAULT_TRUTH_DATA_DIR)
    anchor = registry.get_truth_dataset(manifest.calibration_dataset_id)
    base_config = build_single_cell_validation_base_config(anchor.metadata)

    baseline_payload = run_validation_manifest_case(
        manifest,
        threshold_profile,
        base_config=base_config,
        truth_data_dir=registry.truth_data_dir,
        anchor_dataset_id=manifest.calibration_dataset_id,
        calibration_metadata={
            "calibration_profile_id": calibration_profile.profile_id,
            "objective_weights": calibration_profile.objective_weights.as_metric_weights(),
            "calibration_objective": None,
        },
    )
    baseline_objective = evaluate_calibration_objective(
        baseline_payload.get("validation_scorecards", []),
        weights=calibration_profile.objective_weights,
    )

    candidate_scores: list[dict[str, Any]] = []
    selected_payload = baseline_payload
    selected_candidate: CalibrationCandidate | None = None
    selected_objective = baseline_objective
    selected_config = base_config

    for candidate in _candidate_pool(manifest, calibration_profile, registry):
        calibrated_config = apply_calibration_to_simulation_config(
            base_config,
            candidate.calibrated_parameters,
            electro_blend=candidate.electro_blend,
            thermal_blend=candidate.thermal_blend,
        )
        payload = run_validation_manifest_case(
            manifest,
            threshold_profile,
            base_config=calibrated_config,
            truth_data_dir=registry.truth_data_dir,
            anchor_dataset_id=",".join(candidate.anchor_dataset_ids),
            calibration_metadata={
                "calibration_profile_id": calibration_profile.profile_id,
                "objective_weights": calibration_profile.objective_weights.as_metric_weights(),
                "calibration_objective": None,
            },
        )
        objective = evaluate_calibration_objective(
            payload.get("validation_scorecards", []),
            weights=calibration_profile.objective_weights,
        )
        payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(objective)
        candidate_scores.append(
            {
                "candidate_id": candidate.candidate_id,
                "anchor_dataset_ids": list(candidate.anchor_dataset_ids),
                "rc_branch_count": candidate.rc_branch_count,
                "electro_blend": candidate.electro_blend,
                "thermal_blend": candidate.thermal_blend,
                "objective": calibration_objective_result_to_dict(objective),
                "validation_summary": payload.get("validation_summary", {}),
            }
        )
        if objective.total_score < selected_objective.total_score:
            selected_candidate = candidate
            selected_payload = payload
            selected_objective = objective
            selected_config = calibrated_config

    recommendation_text = _recommendation_text(
        baseline_objective,
        selected_objective,
        baseline_payload,
        selected_payload,
    )
    diagnostics = _compare_dataset_diagnostics(baseline_payload, selected_payload)
    markdown = _diagnostics_markdown(
        manifest,
        threshold_profile,
        calibration_profile,
        diagnostics,
        recommendation_text,
    )

    selected_candidate_payload = {
        "candidate_id": selected_candidate.candidate_id if selected_candidate is not None else "baseline",
        "anchor_dataset_ids": list(selected_candidate.anchor_dataset_ids) if selected_candidate is not None else [],
        "rc_branch_count": selected_candidate.rc_branch_count if selected_candidate is not None else 0,
        "electro_blend": selected_candidate.electro_blend if selected_candidate is not None else 0.0,
        "thermal_blend": selected_candidate.thermal_blend if selected_candidate is not None else 0.0,
        "objective": calibration_objective_result_to_dict(selected_objective),
        "base_config_summary": {
            "chemistry_name": selected_config.chemistry_name,
            "cell_nominal_voltage_v": selected_config.cell_nominal_voltage,
            "cell_capacity_ah": selected_config.cell_capacity_ah,
            "electrical_model_type": selected_config.electrical_model.model_type,
            "cooling_coeff_w_per_k": selected_config.cooling_coeff_w_per_k,
        },
        "calibrated_parameters": asdict(selected_candidate.calibrated_parameters) if selected_candidate is not None else None,
    }
    selected_payload["validation_summary"]["calibration_profile_id"] = calibration_profile.profile_id
    selected_payload["validation_summary"]["objective_weights"] = calibration_profile.objective_weights.as_metric_weights()
    selected_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(selected_objective)

    baseline_payload["validation_summary"]["calibration_profile_id"] = calibration_profile.profile_id
    baseline_payload["validation_summary"]["objective_weights"] = calibration_profile.objective_weights.as_metric_weights()
    baseline_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(baseline_objective)

    return RoomEnvelopeCalibrationArtifact(
        manifest=manifest,
        threshold_profile=threshold_profile,
        calibration_profile=calibration_profile,
        truth_data_dir=str(registry.truth_data_dir),
        baseline_validation=baseline_payload,
        selected_validation=selected_payload,
        selected_candidate=selected_candidate_payload,
        candidate_scores=tuple(candidate_scores),
        per_dataset_diagnostics=diagnostics,
        recommendation_text=recommendation_text,
        diagnostics_markdown=markdown,
    )


def room_envelope_calibration_to_dict(artifact: RoomEnvelopeCalibrationArtifact) -> dict[str, Any]:
    return {
        "manifest": validation_manifest_to_dict(artifact.manifest),
        "threshold_profile": validation_threshold_profile_to_dict(artifact.threshold_profile),
        "calibration_profile": calibration_profile_to_dict(artifact.calibration_profile),
        "truth_data_dir": artifact.truth_data_dir,
        "baseline_validation": artifact.baseline_validation,
        "selected_validation": artifact.selected_validation,
        "selected_candidate": artifact.selected_candidate,
        "candidate_scores": list(artifact.candidate_scores),
        "per_dataset_diagnostics": list(artifact.per_dataset_diagnostics),
        "recommendation_text": artifact.recommendation_text,
        "diagnostics_markdown": artifact.diagnostics_markdown,
    }
