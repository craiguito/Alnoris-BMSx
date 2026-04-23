from __future__ import annotations

from dataclasses import asdict, dataclass
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
        }


@dataclass(frozen=True)
class CalibrationProfile:
    profile_id: str
    display_name: str
    description: str
    objective_weights: CalibrationObjectiveWeights
    default_search_plan_id: str


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


DEFAULT_CALIBRATION_PROFILE_ID = "electrical_first"


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

    return observations


def evaluate_scorecard_objective(
    scorecard: dict[str, Any],
    *,
    weights: CalibrationObjectiveWeights,
) -> CalibrationObjectiveResult:
    return evaluate_calibration_objective([scorecard], weights=weights)


def evaluate_calibration_objective(
    scorecards: Sequence[dict[str, Any]],
    *,
    weights: CalibrationObjectiveWeights,
) -> CalibrationObjectiveResult:
    metric_weights = weights.as_metric_weights()
    terms: list[CalibrationObjectiveMetricTerm] = []
    weighted_sum = 0.0
    total_weight = 0.0
    aggregated_observations: dict[str, list[tuple[float, float]]] = {}

    for scorecard in scorecards:
        for metric_id, items in _scorecard_metric_observations(scorecard).items():
            aggregated_observations.setdefault(metric_id, []).extend(items)

    for metric_id, weight in metric_weights.items():
        if weight <= 0.0:
            continue
        items = aggregated_observations.get(metric_id, [])
        if not items:
            continue
        values = [value for value, _ in items]
        thresholds = [threshold for _, threshold in items]
        average_value = sum(values) / len(values)
        threshold_value = sum(thresholds) / len(thresholds)
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
    return CalibrationObjectiveResult(
        total_score=total_score,
        normalized_formula=(
            "objective = sum(weight_i * (avg_metric_i / threshold_i)) / sum(weight_i)"
        ),
        metric_terms=tuple(terms),
        metrics_used=tuple(term.metric_id for term in terms),
    )
