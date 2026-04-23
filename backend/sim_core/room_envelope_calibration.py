from __future__ import annotations

import math
from dataclasses import asdict, dataclass, field, replace
from pathlib import Path
from typing import Any, Sequence

from .calibration import CalibratedParameters, apply_calibration_to_simulation_config, calibrate_parameters
from .calibration_profiles import (
    CalibrationObjectiveResult,
    CalibrationProfile,
    calibration_objective_result_to_dict,
    calibration_profile_to_dict,
    evaluate_calibration_objective,
    evaluate_scorecard_objective,
    get_calibration_profile,
)
from .calibration_search_plans import (
    CalibrationSearchPlan,
    calibration_search_plan_to_dict,
    get_calibration_search_plan,
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
    candidate_mode: str
    anchor_dataset_ids: tuple[str, ...]
    anchor_priority_score: float
    rc_branch_count: int
    recipe_id: str
    electro_blend: float
    thermal_blend: float
    calibrated_parameters: CalibratedParameters


@dataclass(frozen=True)
class FilteredCandidateRecord:
    candidate_id: str
    reason: str
    details: str
    screening_objective_score: float | None = None


@dataclass(frozen=True)
class CandidateEvaluationSummary:
    candidate_id: str
    candidate_mode: str
    anchor_dataset_ids: tuple[str, ...]
    rc_branch_count: int
    recipe_id: str
    electro_blend: float
    thermal_blend: float
    anchor_priority_score: float
    screening_dataset_ids: tuple[str, ...]
    screening_objective_score: float
    full_objective_score: float
    validation_summary: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class DatasetTransitionSummary:
    dataset_id: str
    dataset_display_name: str
    baseline_status: str
    candidate_status: str
    status_transition: str
    status_change: int
    baseline_objective_score: float
    candidate_objective_score: float
    objective_delta: float
    metric_deltas: dict[str, dict[str, Any]] = field(default_factory=dict)
    biggest_metric_improvement: str = ""
    biggest_metric_regression: str = ""


@dataclass(frozen=True)
class RoomEnvelopeImprovementSummary:
    improved_dataset_count: int
    regressed_dataset_count: int
    unchanged_dataset_count: int
    status_upgrade_count: int
    status_downgrade_count: int
    transition_counts: dict[str, int] = field(default_factory=dict)
    most_improved_dataset_id: str = ""
    most_improved_dataset_display_name: str = ""
    most_worsened_dataset_id: str = ""
    most_worsened_dataset_display_name: str = ""


@dataclass(frozen=True)
class RoomEnvelopeBenchmarkRow:
    result_id: str
    label: str
    calibration_profile_id: str
    search_plan_id: str
    candidate_count: int
    passed_count: int
    warning_count: int
    failed_count: int
    objective_score: float
    average_voltage_rmse_v: float
    average_final_voltage_error_v: float
    average_energy_error_fraction: float
    average_temp_rmse_c: float
    most_improved_dataset: str = ""
    most_worsened_dataset: str = ""


@dataclass(frozen=True)
class RoomEnvelopeBenchmarkComparison:
    rows: tuple[RoomEnvelopeBenchmarkRow, ...]
    markdown: str


@dataclass(frozen=True)
class RoomEnvelopeArtifactPaths:
    primary_json: str = ""
    diagnostics_markdown: str = ""
    parameters_json: str = ""
    benchmark_json: str = ""
    benchmark_markdown: str = ""


@dataclass(frozen=True)
class CandidateSearchPreview:
    selected_anchor_dataset_ids: tuple[str, ...]
    screening_dataset_ids: tuple[str, ...]
    generated_candidate_ids: tuple[str, ...]


@dataclass(frozen=True)
class RoomEnvelopeSummary:
    calibration_profile_id: str
    search_plan_id: str
    raw_candidate_count: int
    generated_candidate_count: int
    filtered_candidate_count: int
    screened_candidate_count: int
    full_validation_candidate_count: int
    best_candidate_id: str
    baseline_status_counts: dict[str, int] = field(default_factory=dict)
    selected_status_counts: dict[str, int] = field(default_factory=dict)
    candidate_search_preview: CandidateSearchPreview | None = None
    improvement_summary: RoomEnvelopeImprovementSummary | None = None
    benchmark_comparison: RoomEnvelopeBenchmarkComparison | None = None
    artifact_paths: RoomEnvelopeArtifactPaths = field(default_factory=RoomEnvelopeArtifactPaths)


@dataclass(frozen=True)
class RoomEnvelopeCalibrationArtifact:
    manifest: ValidationPackManifest
    threshold_profile: ValidationThresholdProfile
    calibration_profile: CalibrationProfile
    search_plan: CalibrationSearchPlan
    truth_data_dir: str
    summary: RoomEnvelopeSummary
    baseline_validation: dict[str, Any]
    selected_validation: dict[str, Any]
    selected_candidate: dict[str, Any]
    candidate_evaluations: tuple[CandidateEvaluationSummary, ...]
    filtered_candidates: tuple[FilteredCandidateRecord, ...]
    per_dataset_diagnostics: tuple[DatasetTransitionSummary, ...]
    recommendation_text: str
    diagnostics_markdown: str

    @property
    def candidate_scores(self) -> tuple[CandidateEvaluationSummary, ...]:
        return self.candidate_evaluations


def _infer_battery_family(metadata: dict[str, Any]) -> str:
    family = str(metadata.get("source_battery_id", "")).strip()
    if family:
        return family
    dataset_id = str(metadata.get("dataset_id", "")).strip().lower()
    for token in dataset_id.split("_"):
        if token.startswith("b") and len(token) == 5:
            return token.upper()
    return dataset_id or "dataset"


def _status_rank(status: str) -> int:
    return {"fail": 0, "warning": 1, "pass": 2}.get(str(status).lower(), -1)


def _status_counts(scorecards: Sequence[dict[str, Any]]) -> dict[str, int]:
    counts = {"pass": 0, "warning": 0, "fail": 0}
    for scorecard in scorecards:
        status = str(scorecard.get("overall_status", "")).lower()
        if status in counts:
            counts[status] += 1
    return counts


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


def _average_metric(scorecards: Sequence[dict[str, Any]], metric_id: str) -> float:
    values: list[float] = []
    for scorecard in scorecards:
        for metric in scorecard.get("metric_results", []):
            if str(metric.get("metric_id", "")) == metric_id and metric.get("value") is not None:
                values.append(float(metric["value"]))
    return sum(values) / len(values) if values else 0.0


def _safe_weight(value: float) -> float:
    if not math.isfinite(value):
        return 1.0
    return max(value, 1.0e-6)


def _weighted_mean(values: Sequence[float], weights: Sequence[float]) -> float:
    total_weight = sum(_safe_weight(weight) for weight in weights)
    if total_weight <= 1.0e-9:
        return sum(values) / max(len(values), 1)
    return sum(value * _safe_weight(weight) for value, weight in zip(values, weights)) / total_weight


def _weighted_lookup_points(curves: Sequence[Sequence[Any]], weights: Sequence[float]) -> tuple[Any, ...]:
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
                    voltage_v=_weighted_mean([float(curve[index].voltage_v) for curve in curves], weights),
                )
            )
        else:
            result.append(
                point_type(
                    soc=float(sample.soc),
                    multiplier=_weighted_mean([float(curve[index].multiplier) for curve in curves], weights),
                )
            )
    return tuple(result)


def _weighted_rc_branches(branch_sets: Sequence[Sequence[Any]], weights: Sequence[float]) -> tuple[Any, ...]:
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
                resistance_ohm=_weighted_mean([float(branches[index].resistance_ohm) for branches in branch_sets], weights),
                capacitance_f=_weighted_mean([float(branches[index].capacitance_f) for branches in branch_sets], weights),
            )
        )
    return tuple(result)


def combine_calibrated_parameters(
    parameters: Sequence[CalibratedParameters],
    *,
    parameter_weights: Sequence[float] | None = None,
) -> CalibratedParameters:
    if not parameters:
        raise ValueError("combine_calibrated_parameters requires at least one parameter set.")
    if len(parameters) == 1:
        return parameters[0]

    weights = list(parameter_weights or [1.0 for _ in parameters])
    if len(weights) != len(parameters):
        raise ValueError("parameter_weights must align with parameters.")

    return CalibratedParameters(
        ocv_curve=_weighted_lookup_points([item.ocv_curve for item in parameters], weights),
        base_resistance_ohm_per_cell=_weighted_mean([item.base_resistance_ohm_per_cell for item in parameters], weights),
        resistance_soc_curve=_weighted_lookup_points([item.resistance_soc_curve for item in parameters], weights),
        resistance_temperature_alpha_per_c=_weighted_mean(
            [item.resistance_temperature_alpha_per_c for item in parameters],
            weights,
        ),
        rc_branches=_weighted_rc_branches([item.rc_branches for item in parameters], weights),
        core_thermal_mass_j_per_k=_weighted_mean([item.core_thermal_mass_j_per_k for item in parameters], weights),
        surface_thermal_mass_j_per_k=_weighted_mean([item.surface_thermal_mass_j_per_k for item in parameters], weights),
        cooling_coeff_w_per_k=_weighted_mean([item.cooling_coeff_w_per_k for item in parameters], weights),
        capacity_fade_per_throughput_ah=_weighted_mean(
            [item.capacity_fade_per_throughput_ah for item in parameters],
            weights,
        ),
        resistance_growth_per_throughput_ah=_weighted_mean(
            [item.resistance_growth_per_throughput_ah for item in parameters],
            weights,
        ),
        calendar_capacity_fade_per_hour=_weighted_mean(
            [item.calendar_capacity_fade_per_hour for item in parameters],
            weights,
        ),
        calendar_resistance_growth_per_hour=_weighted_mean(
            [item.calendar_resistance_growth_per_hour for item in parameters],
            weights,
        ),
        diagnostics={
            "source": "weighted_combination",
            "source_count": len(parameters),
            "source_dataset_ids": [
                item.diagnostics.get("dataset_id")
                for item in parameters
                if item.diagnostics.get("dataset_id")
            ],
            "parameter_weights": [float(weight) for weight in weights],
        },
    )


def _family_representative_anchor_ids(
    manifest: ValidationPackManifest,
    registry: TruthDatasetRegistry,
) -> tuple[str, ...]:
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


def _anchor_priority_scores(
    manifest: ValidationPackManifest,
    baseline_payload: dict[str, Any],
    *,
    calibration_profile: CalibrationProfile,
) -> dict[str, float]:
    baseline_by_dataset = _scorecards_by_dataset(baseline_payload)
    priorities: dict[str, float] = {}
    for dataset_id in manifest.dataset_ids:
        scorecard = baseline_by_dataset.get(dataset_id)
        if scorecard is None:
            continue
        priorities[dataset_id] = evaluate_scorecard_objective(
            scorecard,
            weights=calibration_profile.objective_weights,
        ).total_score
    return priorities


def _select_anchor_dataset_ids(
    manifest: ValidationPackManifest,
    registry: TruthDatasetRegistry,
    baseline_payload: dict[str, Any],
    *,
    calibration_profile: CalibrationProfile,
    search_plan: CalibrationSearchPlan,
) -> tuple[str, ...]:
    family_representatives = _family_representative_anchor_ids(manifest, registry)
    priorities = _anchor_priority_scores(
        manifest,
        baseline_payload,
        calibration_profile=calibration_profile,
    )
    ranked = sorted(
        family_representatives,
        key=lambda dataset_id: (-priorities.get(dataset_id, 0.0), dataset_id),
    )
    selected: list[str] = []
    if manifest.calibration_dataset_id in ranked:
        selected.append(manifest.calibration_dataset_id)
    for dataset_id in ranked:
        if dataset_id in selected:
            continue
        if len(selected) >= max(search_plan.anchor_dataset_limit, 1):
            break
        selected.append(dataset_id)
    if not selected and ranked:
        selected.append(ranked[0])
    return tuple(selected)


def _screening_dataset_ids(
    manifest: ValidationPackManifest,
    selected_anchor_dataset_ids: Sequence[str],
    *,
    search_plan: CalibrationSearchPlan,
) -> tuple[str, ...]:
    ordered: list[str] = []
    for dataset_id in selected_anchor_dataset_ids:
        if dataset_id not in ordered:
            ordered.append(dataset_id)
    for dataset_id in manifest.dataset_ids:
        if dataset_id not in ordered:
            ordered.append(dataset_id)
    limit = max(1, min(search_plan.screening_dataset_limit, len(ordered)))
    return tuple(ordered[:limit])


def _build_anchor_calibration_cache(
    selected_anchor_dataset_ids: Sequence[str],
    registry: TruthDatasetRegistry,
    *,
    search_plan: CalibrationSearchPlan,
) -> dict[tuple[str, int], CalibratedParameters]:
    cache: dict[tuple[str, int], CalibratedParameters] = {}
    for rc_branch_count in search_plan.rc_branch_count_options:
        for dataset_id in selected_anchor_dataset_ids:
            resolved = registry.get_truth_dataset(dataset_id)
            calibrated = calibrate_parameters(resolved.dataset, rc_branch_count=rc_branch_count)
            cache[(dataset_id, rc_branch_count)] = CalibratedParameters(
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
                    "rc_branch_count": rc_branch_count,
                },
            )
    return cache


def _candidate_priority_score(anchor_dataset_ids: Sequence[str], anchor_priority_scores: dict[str, float]) -> float:
    if not anchor_dataset_ids:
        return 0.0
    return sum(anchor_priority_scores.get(dataset_id, 0.0) for dataset_id in anchor_dataset_ids) / len(anchor_dataset_ids)


def _generate_candidates(
    manifest: ValidationPackManifest,
    registry: TruthDatasetRegistry,
    baseline_payload: dict[str, Any],
    *,
    calibration_profile: CalibrationProfile,
    search_plan: CalibrationSearchPlan,
) -> tuple[int, tuple[CalibrationCandidate, ...], tuple[FilteredCandidateRecord, ...], CandidateSearchPreview]:
    anchor_priority_scores = _anchor_priority_scores(
        manifest,
        baseline_payload,
        calibration_profile=calibration_profile,
    )
    selected_anchor_dataset_ids = _select_anchor_dataset_ids(
        manifest,
        registry,
        baseline_payload,
        calibration_profile=calibration_profile,
        search_plan=search_plan,
    )
    screening_dataset_ids = _screening_dataset_ids(
        manifest,
        selected_anchor_dataset_ids,
        search_plan=search_plan,
    )
    calibration_cache = _build_anchor_calibration_cache(
        selected_anchor_dataset_ids,
        registry,
        search_plan=search_plan,
    )

    candidates: list[CalibrationCandidate] = []
    for rc_branch_count in search_plan.rc_branch_count_options:
        if search_plan.include_single_anchor_candidates:
            for dataset_id in selected_anchor_dataset_ids:
                calibrated = calibration_cache[(dataset_id, rc_branch_count)]
                for recipe in search_plan.blend_recipes:
                    candidates.append(
                        CalibrationCandidate(
                            candidate_id=f"{dataset_id}_rc{rc_branch_count}_{recipe.recipe_id}",
                            candidate_mode="single_anchor",
                            anchor_dataset_ids=(dataset_id,),
                            anchor_priority_score=_candidate_priority_score((dataset_id,), anchor_priority_scores),
                            rc_branch_count=rc_branch_count,
                            recipe_id=recipe.recipe_id,
                            electro_blend=recipe.electro_blend,
                            thermal_blend=recipe.thermal_blend,
                            calibrated_parameters=calibrated,
                        )
                    )
        if search_plan.include_weighted_family_blend_candidate and selected_anchor_dataset_ids:
            source_parameters = [
                calibration_cache[(dataset_id, rc_branch_count)]
                for dataset_id in selected_anchor_dataset_ids
            ]
            source_weights = [
                _safe_weight(anchor_priority_scores.get(dataset_id, 1.0))
                for dataset_id in selected_anchor_dataset_ids
            ]
            combined = combine_calibrated_parameters(
                source_parameters,
                parameter_weights=source_weights,
            )
            for recipe in search_plan.blend_recipes:
                candidates.append(
                    CalibrationCandidate(
                        candidate_id=f"weighted_family_blend_rc{rc_branch_count}_{recipe.recipe_id}",
                        candidate_mode="weighted_family_blend",
                        anchor_dataset_ids=tuple(selected_anchor_dataset_ids),
                        anchor_priority_score=_candidate_priority_score(selected_anchor_dataset_ids, anchor_priority_scores),
                        rc_branch_count=rc_branch_count,
                        recipe_id=recipe.recipe_id,
                        electro_blend=recipe.electro_blend,
                        thermal_blend=recipe.thermal_blend,
                        calibrated_parameters=combined,
                    )
                )

    raw_candidate_count = len(candidates)
    filtered: list[FilteredCandidateRecord] = []
    if len(candidates) > search_plan.max_candidates:
        for candidate in candidates[search_plan.max_candidates:]:
            filtered.append(
                FilteredCandidateRecord(
                    candidate_id=candidate.candidate_id,
                    reason="max_candidates_guardrail",
                    details=(
                        f"Candidate exceeds search-plan guardrail of {search_plan.max_candidates} generated candidates."
                    ),
                )
            )
        candidates = candidates[: search_plan.max_candidates]

    preview = CandidateSearchPreview(
        selected_anchor_dataset_ids=tuple(selected_anchor_dataset_ids),
        screening_dataset_ids=tuple(screening_dataset_ids),
        generated_candidate_ids=tuple(candidate.candidate_id for candidate in candidates),
    )
    return raw_candidate_count, tuple(candidates), tuple(filtered), preview


def _candidate_sanity_issues(candidate: CalibrationCandidate) -> tuple[str, ...]:
    calibrated = candidate.calibrated_parameters
    issues: list[str] = []
    scalar_checks = {
        "base_resistance_ohm_per_cell": calibrated.base_resistance_ohm_per_cell,
        "core_thermal_mass_j_per_k": calibrated.core_thermal_mass_j_per_k,
        "surface_thermal_mass_j_per_k": calibrated.surface_thermal_mass_j_per_k,
        "cooling_coeff_w_per_k": calibrated.cooling_coeff_w_per_k,
    }
    for key, value in scalar_checks.items():
        if not math.isfinite(value) or value <= 0.0:
            issues.append(f"{key} must be finite and > 0")
    if not calibrated.ocv_curve:
        issues.append("ocv_curve is empty")
    else:
        last_soc = -1.0
        last_voltage = -math.inf
        for point in calibrated.ocv_curve:
            if not math.isfinite(point.soc) or not math.isfinite(point.voltage_v):
                issues.append("ocv_curve must contain finite points")
                break
            if point.soc <= last_soc:
                issues.append("ocv_curve SOC values must be strictly increasing")
                break
            if point.voltage_v < last_voltage - 0.15:
                issues.append("ocv_curve voltage profile is implausibly non-monotonic")
                break
            last_soc = point.soc
            last_voltage = point.voltage_v
    if candidate.rc_branch_count > 0 and len(calibrated.rc_branches) < candidate.rc_branch_count:
        issues.append("rc_branch_count requested but calibrated branches are missing")
    for branch in calibrated.rc_branches:
        if not math.isfinite(branch.resistance_ohm) or branch.resistance_ohm <= 0.0:
            issues.append("rc branch resistance must be finite and > 0")
        if not math.isfinite(branch.capacitance_f) or branch.capacitance_f <= 0.0:
            issues.append("rc branch capacitance must be finite and > 0")
    return tuple(dict.fromkeys(issues))


def _manifest_subset(
    manifest: ValidationPackManifest,
    dataset_ids: Sequence[str],
) -> ValidationPackManifest:
    chosen = tuple(dataset_id for dataset_id in manifest.dataset_ids if dataset_id in set(dataset_ids))
    calibration_dataset_id = (
        manifest.calibration_dataset_id
        if manifest.calibration_dataset_id in chosen
        else (chosen[0] if chosen else manifest.calibration_dataset_id)
    )
    return replace(
        manifest,
        dataset_ids=chosen,
        calibration_dataset_id=calibration_dataset_id,
    )


def _run_candidate_validation(
    candidate: CalibrationCandidate,
    manifest: ValidationPackManifest,
    threshold_profile: ValidationThresholdProfile,
    *,
    base_config: Any,
    truth_data_dir: str,
    calibration_profile: CalibrationProfile,
    search_plan: CalibrationSearchPlan,
    validation_cache: dict[tuple[str, tuple[str, ...]], dict[str, Any]],
) -> dict[str, Any]:
    cache_key = (candidate.candidate_id, tuple(manifest.dataset_ids))
    if cache_key in validation_cache:
        return validation_cache[cache_key]

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
        truth_data_dir=truth_data_dir,
        anchor_dataset_id=",".join(candidate.anchor_dataset_ids),
        calibration_metadata={
            "calibration_profile_id": calibration_profile.profile_id,
            "objective_weights": calibration_profile.objective_weights.as_metric_weights(),
            "calibration_objective": None,
            "search_plan_id": search_plan.plan_id,
        },
    )
    validation_cache[cache_key] = payload
    return payload


def _benchmark_row_from_payload(
    *,
    result_id: str,
    label: str,
    calibration_profile_id: str,
    search_plan_id: str,
    candidate_count: int,
    validation_payload: dict[str, Any],
    improvement_summary: RoomEnvelopeImprovementSummary | None,
) -> RoomEnvelopeBenchmarkRow:
    scorecards = list(validation_payload.get("validation_scorecards", []))
    counts = _status_counts(scorecards)
    objective_score = float(
        validation_payload.get("validation_summary", {})
        .get("calibration_objective", {})
        .get("total_score", 0.0)
    )
    return RoomEnvelopeBenchmarkRow(
        result_id=result_id,
        label=label,
        calibration_profile_id=calibration_profile_id,
        search_plan_id=search_plan_id,
        candidate_count=candidate_count,
        passed_count=counts["pass"],
        warning_count=counts["warning"],
        failed_count=counts["fail"],
        objective_score=objective_score,
        average_voltage_rmse_v=_average_metric(scorecards, "rmse_voltage"),
        average_final_voltage_error_v=_average_metric(scorecards, "final_voltage_error"),
        average_energy_error_fraction=_average_metric(scorecards, "energy_error"),
        average_temp_rmse_c=_average_metric(scorecards, "temp_rmse"),
        most_improved_dataset=(
            improvement_summary.most_improved_dataset_display_name if improvement_summary is not None else ""
        ),
        most_worsened_dataset=(
            improvement_summary.most_worsened_dataset_display_name if improvement_summary is not None else ""
        ),
    )


def build_room_envelope_benchmark_markdown(benchmark: RoomEnvelopeBenchmarkComparison) -> str:
    lines = [
        "# Room-Envelope Benchmark",
        "",
        "| Result Set | Profile | Search Plan | Candidates | Pass | Warn | Fail | Objective | Avg Voltage RMSE | Avg Final Voltage Error | Avg Energy Error | Avg Temp RMSE | Most Improved | Most Worsened |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for row in benchmark.rows:
        lines.append(
            "| {label} | {profile} | {plan} | {candidate_count} | {passed} | {warning} | {failed} | "
            "{objective:.4f} | {rmse:.4f} | {final_v:.4f} | {energy:.4f} | {temp:.4f} | {improved} | {worsened} |".format(
                label=row.label,
                profile=row.calibration_profile_id,
                plan=row.search_plan_id,
                candidate_count=row.candidate_count,
                passed=row.passed_count,
                warning=row.warning_count,
                failed=row.failed_count,
                objective=row.objective_score,
                rmse=row.average_voltage_rmse_v,
                final_v=row.average_final_voltage_error_v,
                energy=row.average_energy_error_fraction,
                temp=row.average_temp_rmse_c,
                improved=row.most_improved_dataset or "n/a",
                worsened=row.most_worsened_dataset or "n/a",
            )
        )
    return "\n".join(lines)


def build_room_envelope_benchmark(
    artifacts: Sequence[RoomEnvelopeCalibrationArtifact],
) -> RoomEnvelopeBenchmarkComparison:
    if not artifacts:
        return RoomEnvelopeBenchmarkComparison(rows=(), markdown="# Room-Envelope Benchmark\n")

    baseline_payload = artifacts[0].baseline_validation
    baseline_row = _benchmark_row_from_payload(
        result_id="baseline_default_model",
        label="Baseline / Default Model",
        calibration_profile_id="baseline",
        search_plan_id="baseline",
        candidate_count=0,
        validation_payload=baseline_payload,
        improvement_summary=None,
    )
    rows: list[RoomEnvelopeBenchmarkRow] = [baseline_row]
    for artifact in artifacts:
        rows.append(
            _benchmark_row_from_payload(
                result_id=f"{artifact.calibration_profile.profile_id}_best_candidate",
                label=f"{artifact.calibration_profile.display_name} Best Candidate",
                calibration_profile_id=artifact.calibration_profile.profile_id,
                search_plan_id=artifact.search_plan.plan_id,
                candidate_count=artifact.summary.generated_candidate_count,
                validation_payload=artifact.selected_validation,
                improvement_summary=artifact.summary.improvement_summary,
            )
        )
    benchmark = RoomEnvelopeBenchmarkComparison(rows=tuple(rows), markdown="")
    return replace(benchmark, markdown=build_room_envelope_benchmark_markdown(benchmark))


def _screen_and_select_candidates(
    candidates: Sequence[CalibrationCandidate],
    initial_filtered: Sequence[FilteredCandidateRecord],
    manifest: ValidationPackManifest,
    threshold_profile: ValidationThresholdProfile,
    *,
    base_config: Any,
    truth_data_dir: str,
    calibration_profile: CalibrationProfile,
    search_plan: CalibrationSearchPlan,
    screening_dataset_ids: Sequence[str],
) -> tuple[tuple[CandidateEvaluationSummary, ...], tuple[FilteredCandidateRecord, ...]]:
    validation_cache: dict[tuple[str, tuple[str, ...]], dict[str, Any]] = {}
    filtered: list[FilteredCandidateRecord] = list(initial_filtered)
    screen_evaluations: list[tuple[CalibrationCandidate, float]] = []
    screening_manifest = _manifest_subset(manifest, screening_dataset_ids)

    for candidate in candidates:
        issues = _candidate_sanity_issues(candidate)
        if issues:
            filtered.append(
                FilteredCandidateRecord(
                    candidate_id=candidate.candidate_id,
                    reason="invalid_parameters",
                    details="; ".join(issues),
                )
            )
            continue

        payload = _run_candidate_validation(
            candidate,
            screening_manifest,
            threshold_profile,
            base_config=base_config,
            truth_data_dir=truth_data_dir,
            calibration_profile=calibration_profile,
            search_plan=search_plan,
            validation_cache=validation_cache,
        )
        screening_objective = evaluate_calibration_objective(
            payload.get("validation_scorecards", []),
            weights=calibration_profile.objective_weights,
        ).total_score
        screen_evaluations.append((candidate, screening_objective))

    screen_evaluations.sort(key=lambda item: (item[1], item[0].candidate_id))
    shortlisted = screen_evaluations[: search_plan.max_full_validation_candidates]
    for candidate, screening_objective in screen_evaluations[search_plan.max_full_validation_candidates:]:
        filtered.append(
            FilteredCandidateRecord(
                candidate_id=candidate.candidate_id,
                reason="screen_rank_cutoff",
                details=(
                    f"Screened on {len(screening_manifest.dataset_ids)} dataset(s) and ranked outside the "
                    f"top {search_plan.max_full_validation_candidates} full-validation candidates."
                ),
                screening_objective_score=screening_objective,
            )
        )

    evaluations: list[CandidateEvaluationSummary] = []
    for candidate, screening_objective in shortlisted:
        full_payload = _run_candidate_validation(
            candidate,
            manifest,
            threshold_profile,
            base_config=base_config,
            truth_data_dir=truth_data_dir,
            calibration_profile=calibration_profile,
            search_plan=search_plan,
            validation_cache=validation_cache,
        )
        full_objective = evaluate_calibration_objective(
            full_payload.get("validation_scorecards", []),
            weights=calibration_profile.objective_weights,
        )
        full_payload.setdefault("validation_summary", {})
        full_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(full_objective)
        full_payload["validation_summary"]["search_plan_id"] = search_plan.plan_id
        evaluations.append(
            CandidateEvaluationSummary(
                candidate_id=candidate.candidate_id,
                candidate_mode=candidate.candidate_mode,
                anchor_dataset_ids=candidate.anchor_dataset_ids,
                rc_branch_count=candidate.rc_branch_count,
                recipe_id=candidate.recipe_id,
                electro_blend=candidate.electro_blend,
                thermal_blend=candidate.thermal_blend,
                anchor_priority_score=candidate.anchor_priority_score,
                screening_dataset_ids=tuple(screening_dataset_ids),
                screening_objective_score=screening_objective,
                full_objective_score=full_objective.total_score,
                validation_summary=dict(full_payload.get("validation_summary", {})),
            )
        )
    evaluations.sort(key=lambda item: (item.full_objective_score, item.candidate_id))
    return tuple(evaluations), tuple(filtered)


def _compare_dataset_diagnostics(
    baseline_payload: dict[str, Any],
    candidate_payload: dict[str, Any],
    *,
    calibration_profile: CalibrationProfile,
) -> tuple[DatasetTransitionSummary, ...]:
    baseline_by_dataset = _scorecards_by_dataset(baseline_payload)
    candidate_by_dataset = _scorecards_by_dataset(candidate_payload)
    diagnostics: list[DatasetTransitionSummary] = []

    for dataset_id, candidate_scorecard in candidate_by_dataset.items():
        baseline_scorecard = baseline_by_dataset.get(dataset_id, {})
        baseline_metrics = _metric_map(baseline_scorecard)
        candidate_metrics = _metric_map(candidate_scorecard)
        metric_deltas: dict[str, dict[str, Any]] = {}
        ranked_changes: list[tuple[float, str]] = []
        for metric_id, candidate_metric in candidate_metrics.items():
            baseline_metric = baseline_metrics.get(metric_id)
            if baseline_metric is None:
                continue
            baseline_value = float(baseline_metric.get("value", 0.0))
            candidate_value = float(candidate_metric.get("value", 0.0))
            threshold = abs(float(candidate_metric.get("threshold_value") or baseline_metric.get("threshold_value") or 1.0))
            delta = candidate_value - baseline_value
            normalized_delta = delta / max(threshold, 1.0e-9)
            ranked_changes.append((normalized_delta, metric_id))
            metric_deltas[metric_id] = {
                "display_name": candidate_metric.get("display_name", metric_id),
                "baseline_value": baseline_value,
                "candidate_value": candidate_value,
                "delta": delta,
                "normalized_delta": normalized_delta,
                "threshold_value": threshold,
                "baseline_passed": baseline_metric.get("passed"),
                "candidate_passed": candidate_metric.get("passed"),
            }

        ranked_changes.sort(key=lambda item: item[0])
        baseline_objective = evaluate_scorecard_objective(
            baseline_scorecard,
            weights=calibration_profile.objective_weights,
        ).total_score if baseline_scorecard else 0.0
        candidate_objective = evaluate_scorecard_objective(
            candidate_scorecard,
            weights=calibration_profile.objective_weights,
        ).total_score
        baseline_status = str(baseline_scorecard.get("overall_status", ""))
        candidate_status = str(candidate_scorecard.get("overall_status", ""))
        diagnostics.append(
            DatasetTransitionSummary(
                dataset_id=dataset_id,
                dataset_display_name=str(candidate_scorecard.get("dataset_display_name", dataset_id)),
                baseline_status=baseline_status,
                candidate_status=candidate_status,
                status_transition=f"{baseline_status.upper()}->{candidate_status.upper()}",
                status_change=_status_rank(candidate_status) - _status_rank(baseline_status),
                baseline_objective_score=baseline_objective,
                candidate_objective_score=candidate_objective,
                objective_delta=candidate_objective - baseline_objective,
                metric_deltas=metric_deltas,
                biggest_metric_improvement=ranked_changes[0][1] if ranked_changes else "",
                biggest_metric_regression=ranked_changes[-1][1] if ranked_changes else "",
            )
        )

    diagnostics.sort(key=lambda item: item.dataset_id)
    return tuple(diagnostics)


def _build_improvement_summary(
    diagnostics: Sequence[DatasetTransitionSummary],
) -> RoomEnvelopeImprovementSummary:
    improved = [item for item in diagnostics if item.objective_delta < -1.0e-9]
    regressed = [item for item in diagnostics if item.objective_delta > 1.0e-9]
    unchanged = [item for item in diagnostics if abs(item.objective_delta) <= 1.0e-9]
    upgrades = [item for item in diagnostics if item.status_change > 0]
    downgrades = [item for item in diagnostics if item.status_change < 0]
    transition_counts: dict[str, int] = {}
    for item in diagnostics:
        transition_counts[item.status_transition] = transition_counts.get(item.status_transition, 0) + 1
    most_improved = min(improved, key=lambda item: item.objective_delta, default=None)
    most_worsened = max(regressed, key=lambda item: item.objective_delta, default=None)
    return RoomEnvelopeImprovementSummary(
        improved_dataset_count=len(improved),
        regressed_dataset_count=len(regressed),
        unchanged_dataset_count=len(unchanged),
        status_upgrade_count=len(upgrades),
        status_downgrade_count=len(downgrades),
        transition_counts=transition_counts,
        most_improved_dataset_id=most_improved.dataset_id if most_improved is not None else "",
        most_improved_dataset_display_name=most_improved.dataset_display_name if most_improved is not None else "",
        most_worsened_dataset_id=most_worsened.dataset_id if most_worsened is not None else "",
        most_worsened_dataset_display_name=most_worsened.dataset_display_name if most_worsened is not None else "",
    )


def _build_diagnostics_markdown(
    manifest: ValidationPackManifest,
    threshold_profile: ValidationThresholdProfile,
    calibration_profile: CalibrationProfile,
    search_plan: CalibrationSearchPlan,
    diagnostics: Sequence[DatasetTransitionSummary],
    improvement_summary: RoomEnvelopeImprovementSummary,
    recommendation_text: str,
) -> str:
    lines = [
        f"# {manifest.display_name} Calibration Diagnostics",
        "",
        f"- Threshold profile: `{threshold_profile.profile_id}`",
        f"- Calibration profile: `{calibration_profile.profile_id}`",
        f"- Search plan: `{search_plan.plan_id}`",
        f"- Recommendation: {recommendation_text}",
        f"- Improved datasets: {improvement_summary.improved_dataset_count}",
        f"- Regressed datasets: {improvement_summary.regressed_dataset_count}",
        f"- Status upgrades: {improvement_summary.status_upgrade_count}",
        f"- Status downgrades: {improvement_summary.status_downgrade_count}",
        "",
        "| Dataset | Baseline | Candidate | Transition | Biggest Improvement | Biggest Regression |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for item in diagnostics:
        lines.append(
            f"| {item.dataset_display_name} | {item.baseline_status.upper()} | {item.candidate_status.upper()} | "
            f"{item.status_transition} | {item.biggest_metric_improvement or 'n/a'} | "
            f"{item.biggest_metric_regression or 'n/a'} |"
        )
    return "\n".join(lines)


def _recommendation_text(
    baseline_objective: CalibrationObjectiveResult,
    selected_objective: CalibrationObjectiveResult,
    baseline_payload: dict[str, Any],
    selected_payload: dict[str, Any],
) -> str:
    baseline_counts = _status_counts(baseline_payload.get("validation_scorecards", []))
    selected_counts = _status_counts(selected_payload.get("validation_scorecards", []))
    objective_delta = baseline_objective.total_score - selected_objective.total_score
    if selected_counts["pass"] > baseline_counts["pass"]:
        return "Room-temperature validation envelope materially improved and is more suitable for comparative decision support."
    if selected_counts["warning"] > baseline_counts["warning"] and selected_counts["fail"] < baseline_counts["fail"]:
        return "Room-temperature validation envelope materially improved, but it still does not clear pass thresholds across the room canonical pack."
    if objective_delta > 0.03:
        return "Room-temperature validation envelope improved, but still needs more electrical fidelity before it can be treated as a strong pass basis."
    return "Calibration search did not materially improve the room-temperature envelope; voltage fidelity remains the main blocker."


def set_room_envelope_artifact_paths(
    artifact: RoomEnvelopeCalibrationArtifact,
    *,
    primary_json: str = "",
    diagnostics_markdown: str = "",
    parameters_json: str = "",
    benchmark_json: str = "",
    benchmark_markdown: str = "",
    benchmark_comparison: RoomEnvelopeBenchmarkComparison | None = None,
) -> RoomEnvelopeCalibrationArtifact:
    paths = RoomEnvelopeArtifactPaths(
        primary_json=primary_json,
        diagnostics_markdown=diagnostics_markdown,
        parameters_json=parameters_json,
        benchmark_json=benchmark_json,
        benchmark_markdown=benchmark_markdown,
    )
    summary = replace(
        artifact.summary,
        artifact_paths=paths,
        benchmark_comparison=benchmark_comparison or artifact.summary.benchmark_comparison,
    )
    return replace(artifact, summary=summary)


def calibrate_room_envelope(
    *,
    manifest_id_or_path: str = "nasa_room_canonical",
    calibration_profile_id: str | None = None,
    search_plan_id: str | None = None,
    threshold_profile_id: str | None = None,
    truth_data_dir: str | Path | None = None,
    manifest_dir: str | Path | None = None,
) -> RoomEnvelopeCalibrationArtifact:
    manifest = resolve_validation_manifest(manifest_id_or_path, manifest_dir=manifest_dir)
    threshold_profile = get_validation_threshold_profile(threshold_profile_id or manifest.recommended_threshold_profile)
    calibration_profile = get_calibration_profile(calibration_profile_id)
    search_plan = get_calibration_search_plan(search_plan_id, calibration_profile_id=calibration_profile.profile_id)
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
            "search_plan_id": search_plan.plan_id,
        },
    )
    baseline_objective = evaluate_calibration_objective(
        baseline_payload.get("validation_scorecards", []),
        weights=calibration_profile.objective_weights,
    )
    baseline_payload.setdefault("validation_summary", {})
    baseline_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(baseline_objective)
    baseline_payload["validation_summary"]["search_plan_id"] = search_plan.plan_id

    raw_candidate_count, candidates, prefiltered, preview = _generate_candidates(
        manifest,
        registry,
        baseline_payload,
        calibration_profile=calibration_profile,
        search_plan=search_plan,
    )
    candidate_evaluations, filtered_candidates = _screen_and_select_candidates(
        candidates,
        prefiltered,
        manifest,
        threshold_profile,
        base_config=base_config,
        truth_data_dir=str(registry.truth_data_dir),
        calibration_profile=calibration_profile,
        search_plan=search_plan,
        screening_dataset_ids=preview.screening_dataset_ids,
    )

    selected_payload = baseline_payload
    selected_candidate_payload: dict[str, Any] = {
        "candidate_id": "baseline",
        "candidate_mode": "baseline",
        "anchor_dataset_ids": [],
        "rc_branch_count": 0,
        "recipe_id": "",
        "electro_blend": 0.0,
        "thermal_blend": 0.0,
        "objective": calibration_objective_result_to_dict(baseline_objective),
        "base_config_summary": {
            "chemistry_name": base_config.chemistry_name,
            "cell_nominal_voltage_v": base_config.cell_nominal_voltage,
            "cell_capacity_ah": base_config.cell_capacity_ah,
            "electrical_model_type": base_config.electrical_model.model_type,
            "cooling_coeff_w_per_k": base_config.cooling_coeff_w_per_k,
        },
        "calibrated_parameters": None,
    }
    selected_objective = baseline_objective
    selected_candidate = None
    candidate_lookup = {candidate.candidate_id: candidate for candidate in candidates}

    if candidate_evaluations:
        best_evaluation = min(candidate_evaluations, key=lambda item: (item.full_objective_score, item.candidate_id))
        if best_evaluation.full_objective_score < baseline_objective.total_score:
            selected_candidate = candidate_lookup.get(best_evaluation.candidate_id)
            if selected_candidate is not None:
                validation_cache: dict[tuple[str, tuple[str, ...]], dict[str, Any]] = {}
                selected_payload = _run_candidate_validation(
                    selected_candidate,
                    manifest,
                    threshold_profile,
                    base_config=base_config,
                    truth_data_dir=str(registry.truth_data_dir),
                    calibration_profile=calibration_profile,
                    search_plan=search_plan,
                    validation_cache=validation_cache,
                )
                selected_objective = evaluate_calibration_objective(
                    selected_payload.get("validation_scorecards", []),
                    weights=calibration_profile.objective_weights,
                )
                selected_payload.setdefault("validation_summary", {})
                selected_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(selected_objective)
                selected_payload["validation_summary"]["search_plan_id"] = search_plan.plan_id
                selected_candidate_payload = {
                    "candidate_id": selected_candidate.candidate_id,
                    "candidate_mode": selected_candidate.candidate_mode,
                    "anchor_dataset_ids": list(selected_candidate.anchor_dataset_ids),
                    "rc_branch_count": selected_candidate.rc_branch_count,
                    "recipe_id": selected_candidate.recipe_id,
                    "electro_blend": selected_candidate.electro_blend,
                    "thermal_blend": selected_candidate.thermal_blend,
                    "objective": calibration_objective_result_to_dict(selected_objective),
                    "base_config_summary": {
                        "chemistry_name": base_config.chemistry_name,
                        "cell_nominal_voltage_v": base_config.cell_nominal_voltage,
                        "cell_capacity_ah": base_config.cell_capacity_ah,
                    },
                    "calibrated_parameters": asdict(selected_candidate.calibrated_parameters),
                }

    selected_payload["validation_summary"]["calibration_profile_id"] = calibration_profile.profile_id
    selected_payload["validation_summary"]["objective_weights"] = calibration_profile.objective_weights.as_metric_weights()
    baseline_payload["validation_summary"]["calibration_profile_id"] = calibration_profile.profile_id
    baseline_payload["validation_summary"]["objective_weights"] = calibration_profile.objective_weights.as_metric_weights()

    diagnostics = _compare_dataset_diagnostics(
        baseline_payload,
        selected_payload,
        calibration_profile=calibration_profile,
    )
    improvement_summary = _build_improvement_summary(diagnostics)
    recommendation_text = _recommendation_text(
        baseline_objective,
        selected_objective,
        baseline_payload,
        selected_payload,
    )
    diagnostics_markdown = _build_diagnostics_markdown(
        manifest,
        threshold_profile,
        calibration_profile,
        search_plan,
        diagnostics,
        improvement_summary,
        recommendation_text,
    )

    summary = RoomEnvelopeSummary(
        calibration_profile_id=calibration_profile.profile_id,
        search_plan_id=search_plan.plan_id,
        raw_candidate_count=raw_candidate_count,
        generated_candidate_count=len(candidates),
        filtered_candidate_count=len(filtered_candidates),
        screened_candidate_count=(
            len(candidate_evaluations)
            + sum(1 for item in filtered_candidates if item.reason == "screen_rank_cutoff")
        ),
        full_validation_candidate_count=len(candidate_evaluations),
        best_candidate_id=str(selected_candidate_payload["candidate_id"]),
        baseline_status_counts=_status_counts(baseline_payload.get("validation_scorecards", [])),
        selected_status_counts=_status_counts(selected_payload.get("validation_scorecards", [])),
        candidate_search_preview=preview,
        improvement_summary=improvement_summary,
    )

    artifact = RoomEnvelopeCalibrationArtifact(
        manifest=manifest,
        threshold_profile=threshold_profile,
        calibration_profile=calibration_profile,
        search_plan=search_plan,
        truth_data_dir=str(registry.truth_data_dir),
        summary=summary,
        baseline_validation=baseline_payload,
        selected_validation=selected_payload,
        selected_candidate=selected_candidate_payload,
        candidate_evaluations=tuple(candidate_evaluations),
        filtered_candidates=tuple(filtered_candidates),
        per_dataset_diagnostics=tuple(diagnostics),
        recommendation_text=recommendation_text,
        diagnostics_markdown=diagnostics_markdown,
    )
    benchmark = build_room_envelope_benchmark([artifact])
    return replace(artifact, summary=replace(artifact.summary, benchmark_comparison=benchmark))


def room_envelope_benchmark_to_dict(benchmark: RoomEnvelopeBenchmarkComparison) -> dict[str, Any]:
    return asdict(benchmark)


def room_envelope_calibration_to_dict(artifact: RoomEnvelopeCalibrationArtifact) -> dict[str, Any]:
    return {
        "manifest": validation_manifest_to_dict(artifact.manifest),
        "threshold_profile": validation_threshold_profile_to_dict(artifact.threshold_profile),
        "calibration_profile": calibration_profile_to_dict(artifact.calibration_profile),
        "search_plan": calibration_search_plan_to_dict(artifact.search_plan),
        "truth_data_dir": artifact.truth_data_dir,
        "summary": asdict(artifact.summary),
        "baseline_validation": artifact.baseline_validation,
        "selected_validation": artifact.selected_validation,
        "selected_candidate": artifact.selected_candidate,
        "candidate_evaluations": [asdict(item) for item in artifact.candidate_evaluations],
        "filtered_candidates": [asdict(item) for item in artifact.filtered_candidates],
        "per_dataset_diagnostics": [asdict(item) for item in artifact.per_dataset_diagnostics],
        "benchmark_comparison": room_envelope_benchmark_to_dict(
            artifact.summary.benchmark_comparison or RoomEnvelopeBenchmarkComparison(rows=(), markdown="")
        ),
        "recommendation_text": artifact.recommendation_text,
        "diagnostics_markdown": artifact.diagnostics_markdown,
    }
