from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Any, Sequence

from .calibration_search_plans import get_default_search_plan_id


@dataclass(frozen=True)
class CalibrationObjectiveWeights:
    voltage_rmse_weight: float
    final_voltage_error_weight: float
    energy_error_weight: float
    temp_rmse_weight: float
    time_to_cutoff_error_weight: float = 0.0
    high_soc_voltage_rmse_weight: float = 0.0
    mid_soc_voltage_rmse_weight: float = 0.0
    low_soc_voltage_rmse_weight: float = 0.0
    last_10_percent_voltage_rmse_weight: float = 0.0
    cutoff_neighborhood_voltage_rmse_weight: float = 0.0

    def as_metric_weights(self) -> dict[str, float]:
        return {
            "rmse_voltage": self.voltage_rmse_weight,
            "final_voltage_error": self.final_voltage_error_weight,
            "energy_error": self.energy_error_weight,
            "temp_rmse": self.temp_rmse_weight,
            "time_to_cutoff_error": self.time_to_cutoff_error_weight,
            "high_soc_voltage_rmse": self.high_soc_voltage_rmse_weight,
            "mid_soc_voltage_rmse": self.mid_soc_voltage_rmse_weight,
            "low_soc_voltage_rmse": self.low_soc_voltage_rmse_weight,
            "last_10_percent_voltage_rmse": self.last_10_percent_voltage_rmse_weight,
            "cutoff_neighborhood_voltage_rmse": self.cutoff_neighborhood_voltage_rmse_weight,
        }


@dataclass(frozen=True)
class CalibrationStageWeights:
    early_life: float = 1.0
    mid_life: float = 1.0
    late_life: float = 1.0
    unknown: float = 1.0

    def as_dict(self) -> dict[str, float]:
        return asdict(self)

    def weight_for_stage(self, aging_stage: str | None) -> float:
        stage_key = str(aging_stage or "unknown").strip().lower()
        return {
            "early_life": self.early_life,
            "mid_life": self.mid_life,
            "late_life": self.late_life,
            "unknown": self.unknown,
        }.get(stage_key, self.unknown)

    def is_uniform(self) -> bool:
        values = [self.early_life, self.mid_life, self.late_life, self.unknown]
        return max(values) - min(values) <= 1.0e-9


@dataclass(frozen=True)
class CalibrationProfile:
    profile_id: str
    display_name: str
    description: str
    objective_weights: CalibrationObjectiveWeights
    default_search_plan_id: str
    aging_stage_weights: CalibrationStageWeights = field(default_factory=CalibrationStageWeights)


@dataclass(frozen=True)
class CalibrationObjectiveMetricTerm:
    metric_id: str
    average_value: float
    threshold_value: float
    normalized_value: float
    weight: float
    weighted_term: float


@dataclass(frozen=True)
class CalibrationObjectiveResult:
    total_score: float
    normalized_formula: str
    metric_terms: tuple[CalibrationObjectiveMetricTerm, ...]
    metrics_used: tuple[str, ...]
    stage_weighting_active: bool
    stage_weights: dict[str, float]
    stage_counts: dict[str, int]


DEFAULT_CALIBRATION_PROFILE_ID = "electrical_first"
_UNIFORM_STAGE_WEIGHTS = CalibrationStageWeights()


_PROFILES: dict[str, CalibrationProfile] = {
    "electrical_first": CalibrationProfile(
        profile_id="electrical_first",
        display_name="Electrical First",
        description=(
            "Prioritize voltage RMSE and final voltage error first, keep energy error "
            "meaningful, and apply only light thermal fitting."
        ),
        objective_weights=CalibrationObjectiveWeights(
            voltage_rmse_weight=0.45,
            final_voltage_error_weight=0.30,
            energy_error_weight=0.20,
            temp_rmse_weight=0.05,
        ),
        default_search_plan_id=get_default_search_plan_id("electrical_first"),
    ),
    "balanced_electro_thermal": CalibrationProfile(
        profile_id="balanced_electro_thermal",
        display_name="Balanced Electro-Thermal",
        description=(
            "Still prioritize electrical fit, but allow more thermal blending to improve "
            "temperature tracking when it does not overly erode voltage fidelity."
        ),
        objective_weights=CalibrationObjectiveWeights(
            voltage_rmse_weight=0.35,
            final_voltage_error_weight=0.25,
            energy_error_weight=0.20,
            temp_rmse_weight=0.20,
        ),
        default_search_plan_id=get_default_search_plan_id("balanced_electro_thermal"),
    ),
    "electrical_tail_guarded": CalibrationProfile(
        profile_id="electrical_tail_guarded",
        display_name="Electrical Tail Guarded",
        description=(
            "Prioritize room-temperature low-SOC and end-of-discharge electrical fidelity "
            "while still preserving overall voltage shape and usable energy tracking."
        ),
        objective_weights=CalibrationObjectiveWeights(
            voltage_rmse_weight=0.22,
            final_voltage_error_weight=0.22,
            energy_error_weight=0.08,
            temp_rmse_weight=0.02,
            low_soc_voltage_rmse_weight=0.28,
            last_10_percent_voltage_rmse_weight=0.18,
        ),
        default_search_plan_id=get_default_search_plan_id("electrical_first"),
    ),
    "aged_tail_guarded": CalibrationProfile(
        profile_id="aged_tail_guarded",
        display_name="Aged Tail Guarded",
        description=(
            "Prioritize late-life low-SOC and end-of-discharge voltage fidelity while keeping "
            "overall room-temperature electrical performance usable for comparative trade studies."
        ),
        objective_weights=CalibrationObjectiveWeights(
            voltage_rmse_weight=0.14,
            final_voltage_error_weight=0.16,
            energy_error_weight=0.08,
            temp_rmse_weight=0.02,
            low_soc_voltage_rmse_weight=0.22,
            last_10_percent_voltage_rmse_weight=0.18,
            cutoff_neighborhood_voltage_rmse_weight=0.20,
        ),
        default_search_plan_id=get_default_search_plan_id("aged_tail_guarded"),
        aging_stage_weights=CalibrationStageWeights(
            early_life=0.55,
            mid_life=1.00,
            late_life=1.85,
            unknown=0.75,
        ),
    ),
}


def list_calibration_profiles() -> list[CalibrationProfile]:
    return list(_PROFILES.values())


def get_calibration_profile(profile_id: str | None = None) -> CalibrationProfile:
    key = str(profile_id or DEFAULT_CALIBRATION_PROFILE_ID).strip()
    try:
        return _PROFILES[key]
    except KeyError as exc:
        raise ValueError(
            f"Unknown calibration profile '{key}'. Available profiles: {', '.join(sorted(_PROFILES)) or 'none'}."
        ) from exc


def calibration_profile_to_dict(profile: CalibrationProfile) -> dict[str, Any]:
    return asdict(profile)


def calibration_objective_result_to_dict(result: CalibrationObjectiveResult) -> dict[str, Any]:
    return asdict(result)


def _scorecard_metric_observations(scorecard: dict[str, Any]) -> dict[str, list[tuple[float, float]]]:
    observations: dict[str, list[tuple[float, float]]] = {}

    for metric in scorecard.get("metric_results", []):
        metric_id = str(metric.get("metric_id", "")).strip()
        value = metric.get("value")
        threshold = metric.get("threshold_value")
        if not metric_id or value is None or threshold in (None, 0.0):
            continue
        observations.setdefault(metric_id, []).append((float(value), abs(float(threshold))))

    for segment in scorecard.get("segmented_metrics", []):
        segment_id = str(segment.get("segment_id", "")).strip()
        rmse_value = segment.get("voltage_rmse_v")
        rmse_threshold = segment.get("threshold_voltage_rmse_v")
        if segment_id and rmse_value is not None and rmse_threshold not in (None, 0.0):
            observations.setdefault(f"{segment_id}_voltage_rmse", []).append(
                (float(rmse_value), abs(float(rmse_threshold)))
            )

    tail_metrics = scorecard.get("tail_metrics") or {}
    last_10_value = tail_metrics.get("last_10_percent_voltage_rmse_v")
    last_10_threshold = tail_metrics.get("last_10_percent_threshold_v")
    if last_10_value is not None and last_10_threshold not in (None, 0.0):
        observations.setdefault("last_10_percent_voltage_rmse", []).append(
            (float(last_10_value), abs(float(last_10_threshold)))
        )
    cutoff_value = tail_metrics.get("cutoff_neighborhood_voltage_rmse_v")
    cutoff_threshold = tail_metrics.get("cutoff_neighborhood_threshold_v")
    if cutoff_value is not None and cutoff_threshold not in (None, 0.0):
        observations.setdefault("cutoff_neighborhood_voltage_rmse", []).append(
            (float(cutoff_value), abs(float(cutoff_threshold)))
        )

    return observations


def _scorecard_aging_stage(scorecard: dict[str, Any]) -> str:
    return str(scorecard.get("aging_stage", "unknown")).strip().lower() or "unknown"


def _resolved_stage_weights(stage_weights: CalibrationStageWeights | None) -> CalibrationStageWeights:
    return stage_weights or _UNIFORM_STAGE_WEIGHTS


def evaluate_scorecard_objective(
    scorecard: dict[str, Any],
    *,
    weights: CalibrationObjectiveWeights,
    stage_weights: CalibrationStageWeights | None = None,
) -> CalibrationObjectiveResult:
    return evaluate_calibration_objective([scorecard], weights=weights, stage_weights=stage_weights)


def evaluate_calibration_objective(
    scorecards: Sequence[dict[str, Any]],
    *,
    weights: CalibrationObjectiveWeights,
    stage_weights: CalibrationStageWeights | None = None,
) -> CalibrationObjectiveResult:
    metric_weights = weights.as_metric_weights()
    resolved_stage_weights = _resolved_stage_weights(stage_weights)
    terms: list[CalibrationObjectiveMetricTerm] = []
    weighted_sum = 0.0
    total_weight = 0.0
    aggregated_observations: dict[str, dict[str, float]] = {}
    stage_counts = {
        "early_life": 0,
        "mid_life": 0,
        "late_life": 0,
        "unknown": 0,
    }

    for scorecard in scorecards:
        aging_stage = _scorecard_aging_stage(scorecard)
        stage_counts[aging_stage if aging_stage in stage_counts else "unknown"] += 1
        stage_weight = max(resolved_stage_weights.weight_for_stage(aging_stage), 0.0)
        if stage_weight <= 0.0:
            continue
        for metric_id, items in _scorecard_metric_observations(scorecard).items():
            bucket = aggregated_observations.setdefault(
                metric_id,
                {
                    "weighted_value_sum": 0.0,
                    "weighted_threshold_sum": 0.0,
                    "weighted_observation_count": 0.0,
                },
            )
            for value, threshold in items:
                bucket["weighted_value_sum"] += float(value) * stage_weight
                bucket["weighted_threshold_sum"] += float(threshold) * stage_weight
                bucket["weighted_observation_count"] += stage_weight

    for metric_id, weight in metric_weights.items():
        if weight <= 0.0:
            continue
        bucket = aggregated_observations.get(metric_id)
        if not bucket or bucket["weighted_observation_count"] <= 0.0:
            continue
        average_value = bucket["weighted_value_sum"] / bucket["weighted_observation_count"]
        threshold_value = bucket["weighted_threshold_sum"] / bucket["weighted_observation_count"]
        normalized_value = average_value / max(threshold_value, 1.0e-9)
        weighted_term = normalized_value * weight
        weighted_sum += weighted_term
        total_weight += weight
        terms.append(
            CalibrationObjectiveMetricTerm(
                metric_id=metric_id,
                average_value=average_value,
                threshold_value=threshold_value,
                normalized_value=normalized_value,
                weight=weight,
                weighted_term=weighted_term,
            )
        )

    total_score = weighted_sum / max(total_weight, 1.0e-9)
    formula = "objective = sum(weight_i * (avg_metric_i / threshold_i)) / sum(weight_i)"
    if not resolved_stage_weights.is_uniform():
        formula += "; avg_metric_i and threshold_i are aging-stage-weighted averages"
    return CalibrationObjectiveResult(
        total_score=total_score,
        normalized_formula=formula,
        metric_terms=tuple(terms),
        metrics_used=tuple(term.metric_id for term in terms),
        stage_weighting_active=not resolved_stage_weights.is_uniform(),
        stage_weights=resolved_stage_weights.as_dict(),
        stage_counts=stage_counts,
    )
