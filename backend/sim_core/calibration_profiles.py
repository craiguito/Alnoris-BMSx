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

    def as_metric_weights(self) -> dict[str, float]:
        return {
            "rmse_voltage": self.voltage_rmse_weight,
            "final_voltage_error": self.final_voltage_error_weight,
            "energy_error": self.energy_error_weight,
            "temp_rmse": self.temp_rmse_weight,
            "time_to_cutoff_error": self.time_to_cutoff_error_weight,
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

    for metric_id, weight in metric_weights.items():
        if weight <= 0.0:
            continue
        values: list[float] = []
        thresholds: list[float] = []
        for scorecard in scorecards:
            for metric in scorecard.get("metric_results", []):
                if str(metric.get("metric_id", "")) != metric_id:
                    continue
                if metric.get("value") is None or metric.get("threshold_value") in (None, 0.0):
                    continue
                values.append(float(metric["value"]))
                thresholds.append(abs(float(metric["threshold_value"])))
        if not values or not thresholds:
            continue

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
    )
