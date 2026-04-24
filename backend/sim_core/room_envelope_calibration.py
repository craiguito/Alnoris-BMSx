from __future__ import annotations

import math
from dataclasses import asdict, dataclass, field, replace
from pathlib import Path
from typing import Any, Sequence

from .calibration import CalibratedParameters, apply_calibration_to_simulation_config, calibrate_parameters
from .calibration_profiles import (
    CalibrationObjectiveResult,
    CalibrationObjectiveWeights,
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
    stage_aware_objective_score: float
    low_soc_objective_score: float
    aged_tail_objective_score: float
    average_low_soc_voltage_rmse_v: float = 0.0
    average_last_10_percent_voltage_rmse_v: float = 0.0
    late_life_low_soc_voltage_rmse_v: float = 0.0
    late_life_last_10_percent_voltage_rmse_v: float = 0.0
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
    aging_stage: str = "unknown"
    baseline_segmented_metrics: dict[str, dict[str, Any]] = field(default_factory=dict)
    candidate_segmented_metrics: dict[str, dict[str, Any]] = field(default_factory=dict)
    baseline_tail_metrics: dict[str, Any] = field(default_factory=dict)
    candidate_tail_metrics: dict[str, Any] = field(default_factory=dict)
    metric_deltas: dict[str, dict[str, Any]] = field(default_factory=dict)
    biggest_metric_improvement: str = ""
    biggest_metric_regression: str = ""


@dataclass(frozen=True)
class StageDiagnosticLeaderboardEntry:
    dataset_id: str
    dataset_display_name: str
    aging_stage: str
    status_transition: str
    objective_delta: float
    low_soc_voltage_rmse_delta_v: float | None = None
    last_10_percent_voltage_rmse_delta_v: float | None = None


@dataclass(frozen=True)
class RoomEnvelopeImprovementSummary:
    improved_dataset_count: int
    regressed_dataset_count: int
    unchanged_dataset_count: int
    status_upgrade_count: int
    status_downgrade_count: int
    late_life_improved_count: int = 0
    late_life_regressed_count: int = 0
    early_life_material_regression_count: int = 0
    transition_counts: dict[str, int] = field(default_factory=dict)
    status_upgrade_counts_by_stage: dict[str, int] = field(default_factory=dict)
    most_improved_dataset_id: str = ""
    most_improved_dataset_display_name: str = ""
    most_worsened_dataset_id: str = ""
    most_worsened_dataset_display_name: str = ""
    late_life_improvement_leaderboard: tuple[StageDiagnosticLeaderboardEntry, ...] = ()
    late_life_remaining_error_leaderboard: tuple[StageDiagnosticLeaderboardEntry, ...] = ()
    early_life_regression_leaderboard: tuple[StageDiagnosticLeaderboardEntry, ...] = ()


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
    average_low_soc_voltage_rmse_v: float
    average_last_10_percent_voltage_rmse_v: float
    average_final_voltage_error_v: float
    average_energy_error_fraction: float
    average_temp_rmse_c: float
    aging_stage_status_counts: dict[str, dict[str, int]] = field(default_factory=dict)
    most_improved_dataset: str = ""
    most_worsened_dataset: str = ""


@dataclass(frozen=True)
class RoomEnvelopeStageBenchmarkRow:
    stage_id: str
    result_id: str
    label: str
    dataset_count: int
    passed_count: int
    warning_count: int
    failed_count: int
    average_voltage_rmse_v: float
    average_low_soc_voltage_rmse_v: float
    average_last_10_percent_voltage_rmse_v: float
    average_final_voltage_error_v: float
    average_energy_error_fraction: float


@dataclass(frozen=True)
class RoomEnvelopeBenchmarkComparison:
    rows: tuple[RoomEnvelopeBenchmarkRow, ...]
    stage_rows: tuple[RoomEnvelopeStageBenchmarkRow, ...] = ()
    markdown: str = ""


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
class TailCandidateComparison:
    aggregate_best_candidate_id: str
    aggregate_best_objective_score: float
    aggregate_best_stage_aware_objective_score: float
    aggregate_best_low_soc_objective_score: float
    low_soc_best_candidate_id: str
    low_soc_best_objective_score: float
    low_soc_best_stage_aware_objective_score: float
    low_soc_best_low_soc_objective_score: float
    aged_tail_best_candidate_id: str
    aged_tail_best_objective_score: float
    aged_tail_best_stage_aware_objective_score: float
    aged_tail_best_low_soc_objective_score: float
    same_candidate: bool
    same_as_aged_tail: bool
    tail_matches_aged_tail: bool
    selected_winner_candidate_id: str = ""
    aggregate_best_candidate_summary: dict[str, Any] = field(default_factory=dict)
    low_soc_best_candidate_summary: dict[str, Any] = field(default_factory=dict)
    aged_tail_best_candidate_summary: dict[str, Any] = field(default_factory=dict)


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
    baseline_aging_stage_summaries: tuple[dict[str, Any], ...] = ()
    selected_aging_stage_summaries: tuple[dict[str, Any], ...] = ()
    baseline_stage_aware_objective_score: float = 0.0
    selected_stage_aware_objective_score: float = 0.0
    baseline_low_soc_objective_score: float = 0.0
    selected_low_soc_objective_score: float = 0.0
    baseline_aged_tail_objective_score: float = 0.0
    selected_aged_tail_objective_score: float = 0.0
    candidate_search_preview: CandidateSearchPreview | None = None
    tail_candidate_comparison: TailCandidateComparison | None = None
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


def _cycle_index(metadata: dict[str, Any]) -> int:
    try:
        return int(metadata.get("source_cycle_index", 0))
    except Exception:
        return 0


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


def _segmented_metric_map(scorecard: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(metric.get("segment_id", "")): metric
        for metric in scorecard.get("segmented_metrics", [])
    }


def _tail_metric_map(scorecard: dict[str, Any]) -> dict[str, Any]:
    return dict(scorecard.get("tail_metrics") or {})


def _average_metric(scorecards: Sequence[dict[str, Any]], metric_id: str) -> float:
    values: list[float] = []
    for scorecard in scorecards:
        for metric in scorecard.get("metric_results", []):
            if str(metric.get("metric_id", "")) == metric_id and metric.get("value") is not None:
                values.append(float(metric["value"]))
    return sum(values) / len(values) if values else 0.0


def _average_segment_metric(scorecards: Sequence[dict[str, Any]], segment_id: str, field_name: str) -> float:
    values: list[float] = []
    for scorecard in scorecards:
        for segment in scorecard.get("segmented_metrics", []):
            if str(segment.get("segment_id", "")) == segment_id and segment.get(field_name) is not None:
                values.append(float(segment[field_name]))
    return sum(values) / len(values) if values else 0.0


def _average_tail_metric(scorecards: Sequence[dict[str, Any]], field_name: str) -> float:
    values: list[float] = []
    for scorecard in scorecards:
        tail_metrics = scorecard.get("tail_metrics") or {}
        if tail_metrics.get(field_name) is not None:
            values.append(float(tail_metrics[field_name]))
    return sum(values) / len(values) if values else 0.0


def _aging_stage_status_counts(validation_payload: dict[str, Any]) -> dict[str, dict[str, int]]:
    summaries = validation_payload.get("validation_summary", {}).get("aging_stage_summaries", [])
    result: dict[str, dict[str, int]] = {}
    for summary in summaries:
        stage = str(summary.get("aging_stage", "")).strip()
        if not stage:
            continue
        result[stage] = {
            "pass": int(summary.get("passed_count", 0)),
            "warning": int(summary.get("warning_count", 0)),
            "fail": int(summary.get("failed_count", 0)),
        }
    return result


def _aging_stage_summary_map(validation_payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    summaries = validation_payload.get("validation_summary", {}).get("aging_stage_summaries", [])
    result: dict[str, dict[str, Any]] = {}
    for summary in summaries:
        stage = str(summary.get("aging_stage", "")).strip()
        if not stage:
            continue
        result[stage] = dict(summary)
    return result


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
        rc_low_soc_multiplier=_weighted_mean([item.rc_low_soc_multiplier for item in parameters], weights),
        age_conditioned_tail_enabled=any(item.age_conditioned_tail_enabled for item in parameters),
        age_conditioned_tail_resistance_gain=_weighted_mean(
            [item.age_conditioned_tail_resistance_gain for item in parameters],
            weights,
        ),
        age_conditioned_tail_ocv_drop_v=_weighted_mean(
            [item.age_conditioned_tail_ocv_drop_v for item in parameters],
            weights,
        ),
        age_conditioned_tail_rc_multiplier_gain=_weighted_mean(
            [item.age_conditioned_tail_rc_multiplier_gain for item in parameters],
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
    baseline_payload: dict[str, Any],
    *,
    calibration_profile: CalibrationProfile,
) -> tuple[str, ...]:
    priorities = _anchor_priority_scores(
        manifest,
        baseline_payload,
        calibration_profile=calibration_profile,
    )
    chosen_by_family: dict[str, tuple[float, int, str]] = {}
    for dataset_id in manifest.dataset_ids:
        resolved = registry.get_truth_dataset(dataset_id)
        family = _infer_battery_family(resolved.metadata)
        cycle_index = _cycle_index(resolved.metadata)
        priority_score = priorities.get(dataset_id, 0.0)
        existing = chosen_by_family.get(family)
        candidate = (priority_score, cycle_index, dataset_id)
        if existing is None or candidate[0] > existing[0] or (candidate[0] == existing[0] and candidate[1] > existing[1]) or (
            candidate[0] == existing[0] and candidate[1] == existing[1] and candidate[2] < existing[2]
        ):
            chosen_by_family[family] = candidate
    ordered = [
        item[2]
        for item in sorted(
            chosen_by_family.values(),
            key=lambda entry: (-entry[0], -entry[1], entry[2]),
        )
    ]
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
            stage_weights=calibration_profile.aging_stage_weights,
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
    family_representatives = _family_representative_anchor_ids(
        manifest,
        registry,
        baseline_payload,
        calibration_profile=calibration_profile,
    )
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
    if manifest.calibration_dataset_id in ranked and calibration_profile.aging_stage_weights.is_uniform():
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
                rc_low_soc_multiplier=calibrated.rc_low_soc_multiplier,
                age_conditioned_tail_enabled=calibrated.age_conditioned_tail_enabled,
                age_conditioned_tail_resistance_gain=calibrated.age_conditioned_tail_resistance_gain,
                age_conditioned_tail_ocv_drop_v=calibrated.age_conditioned_tail_ocv_drop_v,
                age_conditioned_tail_rc_multiplier_gain=calibrated.age_conditioned_tail_rc_multiplier_gain,
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
        "rc_low_soc_multiplier": calibrated.rc_low_soc_multiplier,
        "age_conditioned_tail_resistance_gain": calibrated.age_conditioned_tail_resistance_gain,
        "age_conditioned_tail_ocv_drop_v": calibrated.age_conditioned_tail_ocv_drop_v,
        "age_conditioned_tail_rc_multiplier_gain": calibrated.age_conditioned_tail_rc_multiplier_gain,
    }
    for key, value in scalar_checks.items():
        if key.startswith("age_conditioned_"):
            if not math.isfinite(value) or value < 0.0:
                issues.append(f"{key} must be finite and >= 0")
            continue
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
            "aging_stage_weights": calibration_profile.aging_stage_weights.as_dict(),
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
        average_low_soc_voltage_rmse_v=_average_segment_metric(scorecards, "low_soc", "voltage_rmse_v"),
        average_last_10_percent_voltage_rmse_v=_average_tail_metric(scorecards, "last_10_percent_voltage_rmse_v"),
        average_final_voltage_error_v=_average_metric(scorecards, "final_voltage_error"),
        average_energy_error_fraction=_average_metric(scorecards, "energy_error"),
        average_temp_rmse_c=_average_metric(scorecards, "temp_rmse"),
        aging_stage_status_counts=_aging_stage_status_counts(validation_payload),
        most_improved_dataset=(
            improvement_summary.most_improved_dataset_display_name if improvement_summary is not None else ""
        ),
        most_worsened_dataset=(
            improvement_summary.most_worsened_dataset_display_name if improvement_summary is not None else ""
        ),
    )


def _stage_benchmark_rows_from_payload(
    *,
    result_id: str,
    label: str,
    validation_payload: dict[str, Any],
) -> tuple[RoomEnvelopeStageBenchmarkRow, ...]:
    rows: list[RoomEnvelopeStageBenchmarkRow] = []
    for stage_id, summary in sorted(_aging_stage_summary_map(validation_payload).items()):
        rows.append(
            RoomEnvelopeStageBenchmarkRow(
                stage_id=stage_id,
                result_id=result_id,
                label=label,
                dataset_count=int(summary.get("dataset_count", 0)),
                passed_count=int(summary.get("passed_count", 0)),
                warning_count=int(summary.get("warning_count", 0)),
                failed_count=int(summary.get("failed_count", 0)),
                average_voltage_rmse_v=float(summary.get("average_voltage_rmse_v", 0.0) or 0.0),
                average_low_soc_voltage_rmse_v=float(summary.get("average_low_soc_voltage_rmse_v", 0.0) or 0.0),
                average_last_10_percent_voltage_rmse_v=float(summary.get("average_last_10_percent_voltage_rmse_v", 0.0) or 0.0),
                average_final_voltage_error_v=float(summary.get("average_final_voltage_error_v", 0.0) or 0.0),
                average_energy_error_fraction=float(summary.get("average_energy_error_fraction", 0.0) or 0.0),
            )
        )
    return tuple(rows)


def _stage_display_name(stage_id: str) -> str:
    return {
        "early_life": "Early-Life",
        "mid_life": "Mid-Life",
        "late_life": "Late-Life",
        "unknown": "Unknown Stage",
    }.get(stage_id, stage_id.replace("_", " ").title())


def build_room_envelope_benchmark_markdown(benchmark: RoomEnvelopeBenchmarkComparison) -> str:
    lines = [
        "# Room-Envelope Benchmark",
        "",
        "| Result Set | Profile | Search Plan | Candidates | Pass | Warn | Fail | Objective | Avg Voltage RMSE | Avg Low-SOC RMSE | Avg Last 10% RMSE | Avg Final Voltage Error | Avg Energy Error | Avg Temp RMSE | Life Status E/M/L | Most Improved | Most Worsened |",
        "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for row in benchmark.rows:
        early = row.aging_stage_status_counts.get("early_life", {})
        mid = row.aging_stage_status_counts.get("mid_life", {})
        late = row.aging_stage_status_counts.get("late_life", {})
        life_status = (
            f"E {early.get('pass', 0)}/{early.get('warning', 0)}/{early.get('fail', 0)}; "
            f"M {mid.get('pass', 0)}/{mid.get('warning', 0)}/{mid.get('fail', 0)}; "
            f"L {late.get('pass', 0)}/{late.get('warning', 0)}/{late.get('fail', 0)}"
        )
        lines.append(
            "| {label} | {profile} | {plan} | {candidate_count} | {passed} | {warning} | {failed} | "
            "{objective:.4f} | {rmse:.4f} | {low_soc_rmse:.4f} | {tail_rmse:.4f} | {final_v:.4f} | {energy:.4f} | "
            "{temp:.4f} | {life_status} | {improved} | {worsened} |".format(
                label=row.label,
                profile=row.calibration_profile_id,
                plan=row.search_plan_id,
                candidate_count=row.candidate_count,
                passed=row.passed_count,
                warning=row.warning_count,
                failed=row.failed_count,
                objective=row.objective_score,
                rmse=row.average_voltage_rmse_v,
                low_soc_rmse=row.average_low_soc_voltage_rmse_v,
                tail_rmse=row.average_last_10_percent_voltage_rmse_v,
                final_v=row.average_final_voltage_error_v,
                energy=row.average_energy_error_fraction,
                temp=row.average_temp_rmse_c,
                life_status=life_status,
                improved=row.most_improved_dataset or "n/a",
                worsened=row.most_worsened_dataset or "n/a",
            )
        )
    for stage_id in ("early_life", "mid_life", "late_life", "unknown"):
        stage_rows = [row for row in benchmark.stage_rows if row.stage_id == stage_id]
        if not stage_rows:
            continue
        lines.extend(
            [
                "",
                f"## {_stage_display_name(stage_id)}",
                "",
                "| Result Set | Datasets | Pass | Warn | Fail | Avg Voltage RMSE | Avg Low-SOC RMSE | Avg Last 10% RMSE | Avg Final Voltage Error | Avg Energy Error |",
                "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
            ]
        )
        for row in stage_rows:
            lines.append(
                "| {label} | {dataset_count} | {passed} | {warning} | {failed} | {rmse:.4f} | {low_soc_rmse:.4f} | {tail_rmse:.4f} | {final_v:.4f} | {energy:.4f} |".format(
                    label=row.label,
                    dataset_count=row.dataset_count,
                    passed=row.passed_count,
                    warning=row.warning_count,
                    failed=row.failed_count,
                    rmse=row.average_voltage_rmse_v,
                    low_soc_rmse=row.average_low_soc_voltage_rmse_v,
                    tail_rmse=row.average_last_10_percent_voltage_rmse_v,
                    final_v=row.average_final_voltage_error_v,
                    energy=row.average_energy_error_fraction,
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
    stage_rows: list[RoomEnvelopeStageBenchmarkRow] = list(
        _stage_benchmark_rows_from_payload(
            result_id="baseline_default_model",
            label="Baseline / Default Model",
            validation_payload=baseline_payload,
        )
    )
    for artifact in artifacts:
        result_id = f"{artifact.calibration_profile.profile_id}_best_candidate"
        label = f"{artifact.calibration_profile.display_name} Best Candidate"
        rows.append(
            _benchmark_row_from_payload(
                result_id=result_id,
                label=label,
                calibration_profile_id=artifact.calibration_profile.profile_id,
                search_plan_id=artifact.search_plan.plan_id,
                candidate_count=artifact.summary.generated_candidate_count,
                validation_payload=artifact.selected_validation,
                improvement_summary=artifact.summary.improvement_summary,
            )
        )
        stage_rows.extend(
            _stage_benchmark_rows_from_payload(
                result_id=result_id,
                label=label,
                validation_payload=artifact.selected_validation,
            )
        )
    benchmark = RoomEnvelopeBenchmarkComparison(rows=tuple(rows), stage_rows=tuple(stage_rows), markdown="")
    return replace(benchmark, markdown=build_room_envelope_benchmark_markdown(benchmark))


def _tail_objective_weights(calibration_profile: CalibrationProfile) -> CalibrationObjectiveWeights:
    base = calibration_profile.objective_weights
    derived_low_soc_weight = base.low_soc_voltage_rmse_weight if base.low_soc_voltage_rmse_weight > 0.0 else base.voltage_rmse_weight * 0.75
    derived_last_10_weight = (
        base.last_10_percent_voltage_rmse_weight
        if base.last_10_percent_voltage_rmse_weight > 0.0
        else base.final_voltage_error_weight * 0.60
    )
    derived_cutoff_weight = (
        base.cutoff_neighborhood_voltage_rmse_weight
        if base.cutoff_neighborhood_voltage_rmse_weight > 0.0
        else base.final_voltage_error_weight * 0.50
    )
    return CalibrationObjectiveWeights(
        voltage_rmse_weight=base.voltage_rmse_weight * 0.40,
        final_voltage_error_weight=base.final_voltage_error_weight,
        energy_error_weight=base.energy_error_weight * 0.20,
        temp_rmse_weight=0.0,
        low_soc_voltage_rmse_weight=derived_low_soc_weight,
        last_10_percent_voltage_rmse_weight=derived_last_10_weight,
        cutoff_neighborhood_voltage_rmse_weight=derived_cutoff_weight,
    )


def _candidate_comparison_summary(
    evaluation: CandidateEvaluationSummary,
    candidate: CalibrationCandidate | None,
    *,
    selected_winner_candidate_id: str = "",
) -> dict[str, Any]:
    validation_summary = dict(evaluation.validation_summary or {})
    late_life_summary = _aging_stage_summary_map({"validation_summary": validation_summary}).get("late_life", {})
    return {
        "candidate_id": evaluation.candidate_id,
        "candidate_mode": evaluation.candidate_mode,
        "recipe_id": evaluation.recipe_id,
        "rc_branch_count": evaluation.rc_branch_count,
        "anchor_dataset_ids": list(evaluation.anchor_dataset_ids),
        "electro_blend": evaluation.electro_blend,
        "thermal_blend": evaluation.thermal_blend,
        "full_objective_score": evaluation.full_objective_score,
        "stage_aware_objective_score": evaluation.stage_aware_objective_score,
        "low_soc_objective_score": evaluation.low_soc_objective_score,
        "aged_tail_objective_score": evaluation.aged_tail_objective_score,
        "average_low_soc_voltage_rmse_v": evaluation.average_low_soc_voltage_rmse_v,
        "average_last_10_percent_voltage_rmse_v": evaluation.average_last_10_percent_voltage_rmse_v,
        "late_life_low_soc_voltage_rmse_v": evaluation.late_life_low_soc_voltage_rmse_v,
        "late_life_last_10_percent_voltage_rmse_v": evaluation.late_life_last_10_percent_voltage_rmse_v,
        "passed_count": int(validation_summary.get("passed_count", 0)),
        "warning_count": int(validation_summary.get("warning_count", 0)),
        "failed_count": int(validation_summary.get("failed_count", 0)),
        "late_life_status_counts": {
            "pass": int(late_life_summary.get("passed_count", 0)),
            "warning": int(late_life_summary.get("warning_count", 0)),
            "fail": int(late_life_summary.get("failed_count", 0)),
        },
        "selected_winner": evaluation.candidate_id == selected_winner_candidate_id,
        "rc_low_soc_multiplier": (
            float(candidate.calibrated_parameters.rc_low_soc_multiplier)
            if candidate is not None
            else 0.0
        ),
        "age_conditioned_tail_enabled": (
            bool(candidate.calibrated_parameters.age_conditioned_tail_enabled)
            if candidate is not None
            else False
        ),
        "age_conditioned_tail_resistance_gain": (
            float(candidate.calibrated_parameters.age_conditioned_tail_resistance_gain)
            if candidate is not None
            else 0.0
        ),
        "age_conditioned_tail_ocv_drop_v": (
            float(candidate.calibrated_parameters.age_conditioned_tail_ocv_drop_v)
            if candidate is not None
            else 0.0
        ),
        "age_conditioned_tail_rc_multiplier_gain": (
            float(candidate.calibrated_parameters.age_conditioned_tail_rc_multiplier_gain)
            if candidate is not None
            else 0.0
        ),
    }


def _build_tail_candidate_comparison(
    candidate_evaluations: Sequence[CandidateEvaluationSummary],
    candidate_lookup: dict[str, CalibrationCandidate],
    *,
    selected_winner_candidate_id: str = "",
) -> TailCandidateComparison | None:
    if not candidate_evaluations:
        return None
    aggregate_best = min(candidate_evaluations, key=lambda item: (item.full_objective_score, item.candidate_id))
    low_soc_best = min(candidate_evaluations, key=lambda item: (item.low_soc_objective_score, item.candidate_id))
    aged_tail_best = min(candidate_evaluations, key=lambda item: (item.aged_tail_objective_score, item.candidate_id))
    aggregate_candidate = candidate_lookup.get(aggregate_best.candidate_id)
    low_soc_candidate = candidate_lookup.get(low_soc_best.candidate_id)
    aged_tail_candidate = candidate_lookup.get(aged_tail_best.candidate_id)
    return TailCandidateComparison(
        aggregate_best_candidate_id=aggregate_best.candidate_id,
        aggregate_best_objective_score=aggregate_best.full_objective_score,
        aggregate_best_stage_aware_objective_score=aggregate_best.stage_aware_objective_score,
        aggregate_best_low_soc_objective_score=aggregate_best.low_soc_objective_score,
        low_soc_best_candidate_id=low_soc_best.candidate_id,
        low_soc_best_objective_score=low_soc_best.full_objective_score,
        low_soc_best_stage_aware_objective_score=low_soc_best.stage_aware_objective_score,
        low_soc_best_low_soc_objective_score=low_soc_best.low_soc_objective_score,
        aged_tail_best_candidate_id=aged_tail_best.candidate_id,
        aged_tail_best_objective_score=aged_tail_best.full_objective_score,
        aged_tail_best_stage_aware_objective_score=aged_tail_best.stage_aware_objective_score,
        aged_tail_best_low_soc_objective_score=aged_tail_best.low_soc_objective_score,
        same_candidate=aggregate_best.candidate_id == low_soc_best.candidate_id,
        same_as_aged_tail=aggregate_best.candidate_id == aged_tail_best.candidate_id,
        tail_matches_aged_tail=low_soc_best.candidate_id == aged_tail_best.candidate_id,
        selected_winner_candidate_id=selected_winner_candidate_id,
        aggregate_best_candidate_summary=_candidate_comparison_summary(
            aggregate_best,
            aggregate_candidate,
            selected_winner_candidate_id=selected_winner_candidate_id,
        ),
        low_soc_best_candidate_summary=_candidate_comparison_summary(
            low_soc_best,
            low_soc_candidate,
            selected_winner_candidate_id=selected_winner_candidate_id,
        ),
        aged_tail_best_candidate_summary=_candidate_comparison_summary(
            aged_tail_best,
            aged_tail_candidate,
            selected_winner_candidate_id=selected_winner_candidate_id,
        ),
    )


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
    tail_weights = _tail_objective_weights(calibration_profile)

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
            stage_weights=calibration_profile.aging_stage_weights,
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
            stage_weights=calibration_profile.aging_stage_weights,
        )
        stage_aware_objective = evaluate_calibration_objective(
            full_payload.get("validation_scorecards", []),
            weights=calibration_profile.objective_weights,
            stage_weights=calibration_profile.aging_stage_weights,
        )
        low_soc_objective = evaluate_calibration_objective(
            full_payload.get("validation_scorecards", []),
            weights=tail_weights,
        )
        aged_tail_objective = evaluate_calibration_objective(
            full_payload.get("validation_scorecards", []),
            weights=tail_weights,
            stage_weights=calibration_profile.aging_stage_weights,
        )
        full_payload.setdefault("validation_summary", {})
        full_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(full_objective)
        full_payload["validation_summary"]["stage_aware_calibration_objective"] = calibration_objective_result_to_dict(stage_aware_objective)
        full_payload["validation_summary"]["low_soc_calibration_objective"] = calibration_objective_result_to_dict(low_soc_objective)
        full_payload["validation_summary"]["aged_tail_calibration_objective"] = calibration_objective_result_to_dict(aged_tail_objective)
        full_payload["validation_summary"]["search_plan_id"] = search_plan.plan_id
        full_payload["validation_summary"]["aging_stage_weights"] = calibration_profile.aging_stage_weights.as_dict()
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
                stage_aware_objective_score=stage_aware_objective.total_score,
                low_soc_objective_score=low_soc_objective.total_score,
                aged_tail_objective_score=aged_tail_objective.total_score,
                average_low_soc_voltage_rmse_v=_average_segment_metric(
                    full_payload.get("validation_scorecards", []),
                    "low_soc",
                    "voltage_rmse_v",
                ),
                average_last_10_percent_voltage_rmse_v=_average_tail_metric(
                    full_payload.get("validation_scorecards", []),
                    "last_10_percent_voltage_rmse_v",
                ),
                late_life_low_soc_voltage_rmse_v=float(
                    _aging_stage_summary_map(full_payload).get("late_life", {}).get("average_low_soc_voltage_rmse_v", 0.0) or 0.0
                ),
                late_life_last_10_percent_voltage_rmse_v=float(
                    _aging_stage_summary_map(full_payload).get("late_life", {}).get("average_last_10_percent_voltage_rmse_v", 0.0) or 0.0
                ),
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
    tail_weights = _tail_objective_weights(calibration_profile)

    for dataset_id, candidate_scorecard in candidate_by_dataset.items():
        baseline_scorecard = baseline_by_dataset.get(dataset_id, {})
        baseline_metrics = _metric_map(baseline_scorecard)
        candidate_metrics = _metric_map(candidate_scorecard)
        baseline_segmented = _segmented_metric_map(baseline_scorecard)
        candidate_segmented = _segmented_metric_map(candidate_scorecard)
        baseline_tail = _tail_metric_map(baseline_scorecard)
        candidate_tail = _tail_metric_map(candidate_scorecard)
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

        for segment_id, candidate_segment in candidate_segmented.items():
            baseline_segment = baseline_segmented.get(segment_id)
            if baseline_segment is None:
                continue
            baseline_value = baseline_segment.get("voltage_rmse_v")
            candidate_value = candidate_segment.get("voltage_rmse_v")
            threshold = abs(float(candidate_segment.get("threshold_voltage_rmse_v") or baseline_segment.get("threshold_voltage_rmse_v") or 1.0))
            if baseline_value is not None and candidate_value is not None:
                delta = float(candidate_value) - float(baseline_value)
                normalized_delta = delta / max(threshold, 1.0e-9)
                metric_id = f"{segment_id}_voltage_rmse"
                ranked_changes.append((normalized_delta, metric_id))
                metric_deltas[metric_id] = {
                    "display_name": f"{segment_id.replace('_', ' ').title()} voltage RMSE",
                    "baseline_value": float(baseline_value),
                    "candidate_value": float(candidate_value),
                    "delta": delta,
                    "normalized_delta": normalized_delta,
                    "threshold_value": threshold,
                    "baseline_passed": baseline_segment.get("voltage_rmse_passed"),
                    "candidate_passed": candidate_segment.get("voltage_rmse_passed"),
                }

        for metric_id in ("last_10_percent_voltage_rmse_v", "cutoff_neighborhood_voltage_rmse_v"):
            baseline_value = baseline_tail.get(metric_id)
            candidate_value = candidate_tail.get(metric_id)
            if baseline_value is None or candidate_value is None:
                continue
            threshold_key = "last_10_percent_threshold_v" if metric_id == "last_10_percent_voltage_rmse_v" else "cutoff_neighborhood_threshold_v"
            threshold = abs(float(candidate_tail.get(threshold_key) or baseline_tail.get(threshold_key) or 1.0))
            delta = float(candidate_value) - float(baseline_value)
            normalized_delta = delta / max(threshold, 1.0e-9)
            normalized_metric_id = metric_id.removesuffix("_v")
            ranked_changes.append((normalized_delta, normalized_metric_id))
            metric_deltas[normalized_metric_id] = {
                "display_name": normalized_metric_id.replace("_", " ").title(),
                "baseline_value": float(baseline_value),
                "candidate_value": float(candidate_value),
                "delta": delta,
                "normalized_delta": normalized_delta,
                "threshold_value": threshold,
                "baseline_passed": baseline_tail.get("last_10_percent_passed" if metric_id == "last_10_percent_voltage_rmse_v" else "cutoff_neighborhood_passed"),
                "candidate_passed": candidate_tail.get("last_10_percent_passed" if metric_id == "last_10_percent_voltage_rmse_v" else "cutoff_neighborhood_passed"),
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
        baseline_low_soc_objective = evaluate_scorecard_objective(
            baseline_scorecard,
            weights=tail_weights,
        ).total_score if baseline_scorecard else 0.0
        candidate_low_soc_objective = evaluate_scorecard_objective(
            candidate_scorecard,
            weights=tail_weights,
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
                aging_stage=str(candidate_scorecard.get("aging_stage", baseline_scorecard.get("aging_stage", "unknown"))),
                baseline_segmented_metrics=baseline_segmented,
                candidate_segmented_metrics=candidate_segmented,
                baseline_tail_metrics={
                    **baseline_tail,
                    "low_soc_objective_score": baseline_low_soc_objective,
                },
                candidate_tail_metrics={
                    **candidate_tail,
                    "low_soc_objective_score": candidate_low_soc_objective,
                },
                metric_deltas=metric_deltas,
                biggest_metric_improvement=ranked_changes[0][1] if ranked_changes else "",
                biggest_metric_regression=ranked_changes[-1][1] if ranked_changes else "",
            )
        )

    diagnostics.sort(key=lambda item: item.dataset_id)
    return tuple(diagnostics)


def _delta_or_none(after_value: Any, before_value: Any) -> float | None:
    if after_value is None or before_value is None:
        return None
    return float(after_value) - float(before_value)


def _leaderboard_entry(item: DatasetTransitionSummary) -> StageDiagnosticLeaderboardEntry:
    return StageDiagnosticLeaderboardEntry(
        dataset_id=item.dataset_id,
        dataset_display_name=item.dataset_display_name,
        aging_stage=item.aging_stage,
        status_transition=item.status_transition,
        objective_delta=float(item.objective_delta),
        low_soc_voltage_rmse_delta_v=_delta_or_none(
            item.candidate_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v"),
            item.baseline_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v"),
        ),
        last_10_percent_voltage_rmse_delta_v=_delta_or_none(
            item.candidate_tail_metrics.get("last_10_percent_voltage_rmse_v"),
            item.baseline_tail_metrics.get("last_10_percent_voltage_rmse_v"),
        ),
    )


def _candidate_tail_pain(item: DatasetTransitionSummary) -> float:
    candidate_tail = item.candidate_tail_metrics.get("last_10_percent_voltage_rmse_v")
    candidate_low_soc = item.candidate_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v")
    if candidate_tail is not None:
        return float(candidate_tail)
    if candidate_low_soc is not None:
        return float(candidate_low_soc)
    return -1.0


def _is_material_early_life_regression(item: DatasetTransitionSummary) -> bool:
    if item.aging_stage != "early_life":
        return False
    if item.status_change < 0 or item.objective_delta > 0.05:
        return True
    low_soc_delta = _delta_or_none(
        item.candidate_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v"),
        item.baseline_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v"),
    )
    tail_delta = _delta_or_none(
        item.candidate_tail_metrics.get("last_10_percent_voltage_rmse_v"),
        item.baseline_tail_metrics.get("last_10_percent_voltage_rmse_v"),
    )
    return (low_soc_delta is not None and low_soc_delta > 0.015) or (tail_delta is not None and tail_delta > 0.015)


def _build_improvement_summary(
    diagnostics: Sequence[DatasetTransitionSummary],
) -> RoomEnvelopeImprovementSummary:
    improved = [item for item in diagnostics if item.objective_delta < -1.0e-9]
    regressed = [item for item in diagnostics if item.objective_delta > 1.0e-9]
    unchanged = [item for item in diagnostics if abs(item.objective_delta) <= 1.0e-9]
    upgrades = [item for item in diagnostics if item.status_change > 0]
    downgrades = [item for item in diagnostics if item.status_change < 0]
    late_life_improved = [item for item in diagnostics if item.aging_stage == "late_life" and item.objective_delta < -1.0e-9]
    late_life_regressed = [item for item in diagnostics if item.aging_stage == "late_life" and item.objective_delta > 1.0e-9]
    early_life_material_regressions = [item for item in diagnostics if _is_material_early_life_regression(item)]
    transition_counts: dict[str, int] = {}
    status_upgrade_counts_by_stage: dict[str, int] = {}
    for item in diagnostics:
        transition_counts[item.status_transition] = transition_counts.get(item.status_transition, 0) + 1
        if item.status_change > 0:
            status_upgrade_counts_by_stage[item.aging_stage] = status_upgrade_counts_by_stage.get(item.aging_stage, 0) + 1
    most_improved = min(improved, key=lambda item: item.objective_delta, default=None)
    most_worsened = max(regressed, key=lambda item: item.objective_delta, default=None)
    return RoomEnvelopeImprovementSummary(
        improved_dataset_count=len(improved),
        regressed_dataset_count=len(regressed),
        unchanged_dataset_count=len(unchanged),
        status_upgrade_count=len(upgrades),
        status_downgrade_count=len(downgrades),
        late_life_improved_count=len(late_life_improved),
        late_life_regressed_count=len(late_life_regressed),
        early_life_material_regression_count=len(early_life_material_regressions),
        transition_counts=transition_counts,
        status_upgrade_counts_by_stage=status_upgrade_counts_by_stage,
        most_improved_dataset_id=most_improved.dataset_id if most_improved is not None else "",
        most_improved_dataset_display_name=most_improved.dataset_display_name if most_improved is not None else "",
        most_worsened_dataset_id=most_worsened.dataset_id if most_worsened is not None else "",
        most_worsened_dataset_display_name=most_worsened.dataset_display_name if most_worsened is not None else "",
        late_life_improvement_leaderboard=tuple(
            _leaderboard_entry(item)
            for item in sorted(late_life_improved, key=lambda diagnostic: (diagnostic.objective_delta, diagnostic.dataset_id))[:3]
        ),
        late_life_remaining_error_leaderboard=tuple(
            _leaderboard_entry(item)
            for item in sorted(
                [diagnostic for diagnostic in diagnostics if diagnostic.aging_stage == "late_life"],
                key=lambda diagnostic: (-_candidate_tail_pain(diagnostic), diagnostic.dataset_id),
            )[:3]
        ),
        early_life_regression_leaderboard=tuple(
            _leaderboard_entry(item)
            for item in sorted(early_life_material_regressions, key=lambda diagnostic: (-diagnostic.objective_delta, diagnostic.dataset_id))[:3]
        ),
    )


def _build_diagnostics_markdown(
    manifest: ValidationPackManifest,
    threshold_profile: ValidationThresholdProfile,
    calibration_profile: CalibrationProfile,
    search_plan: CalibrationSearchPlan,
    diagnostics: Sequence[DatasetTransitionSummary],
    tail_candidate_comparison: TailCandidateComparison | None,
    improvement_summary: RoomEnvelopeImprovementSummary,
    recommendation_text: str,
) -> str:
    lines = [
        f"# {manifest.display_name} Calibration Diagnostics",
        "",
        f"- Threshold profile: `{threshold_profile.profile_id}`",
        f"- Calibration profile: `{calibration_profile.profile_id}`",
        f"- Search plan: `{search_plan.plan_id}`",
        f"- Aging-stage weighting active: `{str(not calibration_profile.aging_stage_weights.is_uniform()).lower()}`",
        f"- Recommendation: {recommendation_text}",
        f"- Improved datasets: {improvement_summary.improved_dataset_count}",
        f"- Regressed datasets: {improvement_summary.regressed_dataset_count}",
        f"- Status upgrades: {improvement_summary.status_upgrade_count}",
        f"- Status downgrades: {improvement_summary.status_downgrade_count}",
        f"- Late-life improved: {improvement_summary.late_life_improved_count}",
        f"- Late-life regressed: {improvement_summary.late_life_regressed_count}",
        f"- Early-life materially regressed: {improvement_summary.early_life_material_regression_count}",
    ]
    if tail_candidate_comparison is not None:
        lines.extend(
            [
                f"- Aggregate-best candidate: `{tail_candidate_comparison.aggregate_best_candidate_id}`",
                f"- Tail-best candidate: `{tail_candidate_comparison.low_soc_best_candidate_id}`",
                f"- Aged-tail-best candidate: `{tail_candidate_comparison.aged_tail_best_candidate_id}`",
                f"- Aggregate-best equals tail-best: `{str(tail_candidate_comparison.same_candidate).lower()}`",
                f"- Aggregate-best equals aged-tail-best: `{str(tail_candidate_comparison.same_as_aged_tail).lower()}`",
                f"- Tail-best equals aged-tail-best: `{str(tail_candidate_comparison.tail_matches_aged_tail).lower()}`",
            ]
        )
    if improvement_summary.late_life_improvement_leaderboard:
        lines.extend(["", "## Late-Life Improvements", ""])
        for item in improvement_summary.late_life_improvement_leaderboard:
            lines.append(
                "- `{dataset}` {transition} objective {delta:+.4f} low_soc {low_soc} tail {tail}".format(
                    dataset=item.dataset_id,
                    transition=item.status_transition,
                    delta=item.objective_delta,
                    low_soc=(
                        "n/a"
                        if item.low_soc_voltage_rmse_delta_v is None
                        else f"{item.low_soc_voltage_rmse_delta_v:+.4f}"
                    ),
                    tail=(
                        "n/a"
                        if item.last_10_percent_voltage_rmse_delta_v is None
                        else f"{item.last_10_percent_voltage_rmse_delta_v:+.4f}"
                    ),
                )
            )
    if improvement_summary.late_life_remaining_error_leaderboard:
        lines.extend(["", "## Late-Life Remaining Tail Error", ""])
        for item in improvement_summary.late_life_remaining_error_leaderboard:
            lines.append(
                f"- `{item.dataset_id}` remains a dominant late-life tail dataset ({item.status_transition})"
            )
    if improvement_summary.early_life_regression_leaderboard:
        lines.extend(["", "## Early-Life Regressions", ""])
        for item in improvement_summary.early_life_regression_leaderboard:
            lines.append(
                "- `{dataset}` {transition} objective {delta:+.4f} low_soc {low_soc} tail {tail}".format(
                    dataset=item.dataset_id,
                    transition=item.status_transition,
                    delta=item.objective_delta,
                    low_soc=(
                        "n/a"
                        if item.low_soc_voltage_rmse_delta_v is None
                        else f"{item.low_soc_voltage_rmse_delta_v:+.4f}"
                    ),
                    tail=(
                        "n/a"
                        if item.last_10_percent_voltage_rmse_delta_v is None
                        else f"{item.last_10_percent_voltage_rmse_delta_v:+.4f}"
                    ),
                )
            )
    lines.extend(
        [
            "",
            "| Dataset | Aging Stage | Baseline | Candidate | Transition | Biggest Improvement | Biggest Regression | Low-SOC RMSE (Base->Cand) | Tail RMSE (Base->Cand) |",
            "| --- | --- | --- | --- | --- | --- | --- | --- | --- |",
        ]
    )
    for item in diagnostics:
        baseline_low_soc = item.baseline_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v")
        candidate_low_soc = item.candidate_segmented_metrics.get("low_soc", {}).get("voltage_rmse_v")
        baseline_tail = item.baseline_tail_metrics.get("last_10_percent_voltage_rmse_v")
        candidate_tail = item.candidate_tail_metrics.get("last_10_percent_voltage_rmse_v")
        lines.append(
            f"| {item.dataset_display_name} | {item.aging_stage} | {item.baseline_status.upper()} | {item.candidate_status.upper()} | "
            f"{item.status_transition} | {item.biggest_metric_improvement or 'n/a'} | "
            f"{item.biggest_metric_regression or 'n/a'} | "
            f"{('n/a' if baseline_low_soc is None else f'{baseline_low_soc:.4f}')} -> {('n/a' if candidate_low_soc is None else f'{candidate_low_soc:.4f}')} | "
            f"{('n/a' if baseline_tail is None else f'{baseline_tail:.4f}')} -> {('n/a' if candidate_tail is None else f'{candidate_tail:.4f}')} |"
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
    tail_weights = _tail_objective_weights(calibration_profile)
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
            "aging_stage_weights": calibration_profile.aging_stage_weights.as_dict(),
            "calibration_objective": None,
            "search_plan_id": search_plan.plan_id,
        },
    )
    baseline_objective = evaluate_calibration_objective(
        baseline_payload.get("validation_scorecards", []),
        weights=calibration_profile.objective_weights,
        stage_weights=calibration_profile.aging_stage_weights,
    )
    baseline_stage_aware_objective = evaluate_calibration_objective(
        baseline_payload.get("validation_scorecards", []),
        weights=calibration_profile.objective_weights,
        stage_weights=calibration_profile.aging_stage_weights,
    )
    baseline_low_soc_objective = evaluate_calibration_objective(
        baseline_payload.get("validation_scorecards", []),
        weights=tail_weights,
    )
    baseline_aged_tail_objective = evaluate_calibration_objective(
        baseline_payload.get("validation_scorecards", []),
        weights=tail_weights,
        stage_weights=calibration_profile.aging_stage_weights,
    )
    baseline_payload.setdefault("validation_summary", {})
    baseline_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(baseline_objective)
    baseline_payload["validation_summary"]["stage_aware_calibration_objective"] = calibration_objective_result_to_dict(baseline_stage_aware_objective)
    baseline_payload["validation_summary"]["low_soc_calibration_objective"] = calibration_objective_result_to_dict(baseline_low_soc_objective)
    baseline_payload["validation_summary"]["aged_tail_calibration_objective"] = calibration_objective_result_to_dict(baseline_aged_tail_objective)
    baseline_payload["validation_summary"]["search_plan_id"] = search_plan.plan_id
    baseline_payload["validation_summary"]["aging_stage_weights"] = calibration_profile.aging_stage_weights.as_dict()

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
        "stage_aware_objective": calibration_objective_result_to_dict(baseline_stage_aware_objective),
        "low_soc_objective": calibration_objective_result_to_dict(baseline_low_soc_objective),
        "aged_tail_objective": calibration_objective_result_to_dict(baseline_aged_tail_objective),
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
    selected_stage_aware_objective = baseline_stage_aware_objective
    selected_low_soc_objective = baseline_low_soc_objective
    selected_aged_tail_objective = baseline_aged_tail_objective
    selected_candidate = None
    candidate_lookup = {candidate.candidate_id: candidate for candidate in candidates}
    tail_candidate_comparison = None

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
                    stage_weights=calibration_profile.aging_stage_weights,
                )
                selected_stage_aware_objective = evaluate_calibration_objective(
                    selected_payload.get("validation_scorecards", []),
                    weights=calibration_profile.objective_weights,
                    stage_weights=calibration_profile.aging_stage_weights,
                )
                selected_low_soc_objective = evaluate_calibration_objective(
                    selected_payload.get("validation_scorecards", []),
                    weights=tail_weights,
                )
                selected_aged_tail_objective = evaluate_calibration_objective(
                    selected_payload.get("validation_scorecards", []),
                    weights=tail_weights,
                    stage_weights=calibration_profile.aging_stage_weights,
                )
                selected_payload.setdefault("validation_summary", {})
                selected_payload["validation_summary"]["calibration_objective"] = calibration_objective_result_to_dict(selected_objective)
                selected_payload["validation_summary"]["stage_aware_calibration_objective"] = calibration_objective_result_to_dict(selected_stage_aware_objective)
                selected_payload["validation_summary"]["low_soc_calibration_objective"] = calibration_objective_result_to_dict(selected_low_soc_objective)
                selected_payload["validation_summary"]["aged_tail_calibration_objective"] = calibration_objective_result_to_dict(selected_aged_tail_objective)
                selected_payload["validation_summary"]["search_plan_id"] = search_plan.plan_id
                selected_payload["validation_summary"]["aging_stage_weights"] = calibration_profile.aging_stage_weights.as_dict()
                selected_candidate_payload = {
                    "candidate_id": selected_candidate.candidate_id,
                    "candidate_mode": selected_candidate.candidate_mode,
                    "anchor_dataset_ids": list(selected_candidate.anchor_dataset_ids),
                    "rc_branch_count": selected_candidate.rc_branch_count,
                    "recipe_id": selected_candidate.recipe_id,
                    "electro_blend": selected_candidate.electro_blend,
                    "thermal_blend": selected_candidate.thermal_blend,
                    "objective": calibration_objective_result_to_dict(selected_objective),
                    "stage_aware_objective": calibration_objective_result_to_dict(selected_stage_aware_objective),
                    "low_soc_objective": calibration_objective_result_to_dict(selected_low_soc_objective),
                    "aged_tail_objective": calibration_objective_result_to_dict(selected_aged_tail_objective),
                    "base_config_summary": {
                        "chemistry_name": base_config.chemistry_name,
                        "cell_nominal_voltage_v": base_config.cell_nominal_voltage,
                        "cell_capacity_ah": base_config.cell_capacity_ah,
                    },
                    "calibrated_parameters": asdict(selected_candidate.calibrated_parameters),
                }

    selected_payload["validation_summary"]["calibration_profile_id"] = calibration_profile.profile_id
    selected_payload["validation_summary"]["objective_weights"] = calibration_profile.objective_weights.as_metric_weights()
    selected_payload["validation_summary"]["tail_objective_weights"] = tail_weights.as_metric_weights()
    selected_payload["validation_summary"]["aging_stage_weights"] = calibration_profile.aging_stage_weights.as_dict()
    baseline_payload["validation_summary"]["calibration_profile_id"] = calibration_profile.profile_id
    baseline_payload["validation_summary"]["objective_weights"] = calibration_profile.objective_weights.as_metric_weights()
    baseline_payload["validation_summary"]["tail_objective_weights"] = tail_weights.as_metric_weights()
    baseline_payload["validation_summary"]["aging_stage_weights"] = calibration_profile.aging_stage_weights.as_dict()
    if "low_soc_calibration_objective" not in selected_payload["validation_summary"]:
        selected_payload["validation_summary"]["low_soc_calibration_objective"] = calibration_objective_result_to_dict(selected_low_soc_objective)
    if "stage_aware_calibration_objective" not in selected_payload["validation_summary"]:
        selected_payload["validation_summary"]["stage_aware_calibration_objective"] = calibration_objective_result_to_dict(selected_stage_aware_objective)
    if "aged_tail_calibration_objective" not in selected_payload["validation_summary"]:
        selected_payload["validation_summary"]["aged_tail_calibration_objective"] = calibration_objective_result_to_dict(selected_aged_tail_objective)

    tail_candidate_comparison = _build_tail_candidate_comparison(
        candidate_evaluations,
        candidate_lookup,
        selected_winner_candidate_id=str(selected_candidate_payload["candidate_id"]),
    )

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
        tail_candidate_comparison,
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
        baseline_aging_stage_summaries=tuple(
            dict(item) for item in baseline_payload.get("validation_summary", {}).get("aging_stage_summaries", [])
        ),
        selected_aging_stage_summaries=tuple(
            dict(item) for item in selected_payload.get("validation_summary", {}).get("aging_stage_summaries", [])
        ),
        baseline_stage_aware_objective_score=baseline_stage_aware_objective.total_score,
        selected_stage_aware_objective_score=selected_stage_aware_objective.total_score,
        baseline_low_soc_objective_score=baseline_low_soc_objective.total_score,
        selected_low_soc_objective_score=selected_low_soc_objective.total_score,
        baseline_aged_tail_objective_score=baseline_aged_tail_objective.total_score,
        selected_aged_tail_objective_score=selected_aged_tail_objective.total_score,
        candidate_search_preview=preview,
        tail_candidate_comparison=tail_candidate_comparison,
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
