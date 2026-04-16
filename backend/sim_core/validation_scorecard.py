from __future__ import annotations

import hashlib
import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from statistics import mean
from typing import Any, Callable, Sequence

from .calibration import (
    TruthDataset,
    compute_energy_error,
    compute_final_soc_error,
    compute_final_voltage_error,
    compute_max_abs_voltage_error,
    compute_rmse_temp,
    compute_rmse_voltage,
)
from .types import SimulationConfig, SimulationResult


DEFAULT_MODEL_VALIDATION_METRICS: tuple[str, ...] = (
    "rmse_voltage",
    "energy_error",
    "temp_rmse",
)


@dataclass(frozen=True)
class ValidationMetricResult:
    metric_id: str
    display_name: str
    value: float
    unit: str
    threshold_key: str
    threshold_value: float | None
    comparator: str = "<="
    passed: bool | None = None


@dataclass(frozen=True)
class ValidationScorecard:
    dataset_id: str
    dataset_display_name: str
    dataset_status: str
    dataset_path: str
    metric_results: tuple[ValidationMetricResult, ...]
    overall_passed: bool
    overall_status: str
    warnings: tuple[str, ...] = ()
    metadata_snapshot: dict[str, Any] = field(default_factory=dict)
    run_timestamp: str = ""
    configuration_fingerprint: str = ""
    configuration_summary: dict[str, Any] = field(default_factory=dict)
    interpretation_text: str = ""


@dataclass(frozen=True)
class ValidationHeadlineMetric:
    metric_id: str
    display_name: str
    unit: str
    average_value: float
    best_value: float
    worst_value: float
    best_dataset_id: str
    worst_dataset_id: str


@dataclass(frozen=True)
class AggregateValidationSummary:
    dataset_count: int
    passed_count: int
    failed_count: int
    overall_status: str
    strongest_dataset_id: str = ""
    strongest_dataset_display_name: str = ""
    weakest_dataset_id: str = ""
    weakest_dataset_display_name: str = ""
    headline_metrics: tuple[ValidationHeadlineMetric, ...] = ()
    key_warnings: tuple[str, ...] = ()
    recommendation_text: str = ""


@dataclass(frozen=True)
class _MetricDefinition:
    metric_id: str
    display_name: str
    unit: str
    threshold_key: str
    calculator: Callable[[SimulationResult, TruthDataset], float]


_METRIC_DEFINITIONS: dict[str, _MetricDefinition] = {
    "rmse_voltage": _MetricDefinition(
        metric_id="rmse_voltage",
        display_name="Voltage RMSE",
        unit="V",
        threshold_key="max_voltage_rmse_v",
        calculator=compute_rmse_voltage,
    ),
    "max_abs_voltage_error": _MetricDefinition(
        metric_id="max_abs_voltage_error",
        display_name="Max abs voltage error",
        unit="V",
        threshold_key="max_abs_voltage_error_v",
        calculator=compute_max_abs_voltage_error,
    ),
    "energy_error": _MetricDefinition(
        metric_id="energy_error",
        display_name="Energy error",
        unit="fraction",
        threshold_key="max_energy_error_fraction",
        calculator=compute_energy_error,
    ),
    "temp_rmse": _MetricDefinition(
        metric_id="temp_rmse",
        display_name="Temperature RMSE",
        unit="C",
        threshold_key="max_temp_rmse_c",
        calculator=compute_rmse_temp,
    ),
    "final_soc_error": _MetricDefinition(
        metric_id="final_soc_error",
        display_name="Final SOC error",
        unit="fraction",
        threshold_key="max_final_soc_error",
        calculator=compute_final_soc_error,
    ),
    "final_voltage_error": _MetricDefinition(
        metric_id="final_voltage_error",
        display_name="Final voltage error",
        unit="V",
        threshold_key="max_final_voltage_error_v",
        calculator=compute_final_voltage_error,
    ),
}


def supported_validation_metrics() -> tuple[str, ...]:
    return tuple(_METRIC_DEFINITIONS.keys())


def normalize_requested_metrics(metrics: Sequence[str] | None) -> tuple[str, ...]:
    cleaned: list[str] = []
    for metric in metrics or DEFAULT_MODEL_VALIDATION_METRICS:
        metric_id = str(metric).strip()
        if not metric_id:
            continue
        if metric_id not in _METRIC_DEFINITIONS:
            raise ValueError(
                "Unsupported model validation metric(s): "
                + ", ".join(metric_id for metric_id in (str(metric).strip() for metric in metrics or ()) if metric_id not in _METRIC_DEFINITIONS)
            )
        if metric_id not in cleaned:
            cleaned.append(metric_id)
    if not cleaned:
        raise ValueError("Model validation requires at least one metric.")
    return tuple(cleaned)


def _json_default(value: Any) -> Any:
    if isinstance(value, tuple):
        return list(value)
    return value


def _configuration_fingerprint(config: SimulationConfig) -> str:
    serialized = json.dumps(asdict(config), sort_keys=True, default=_json_default)
    return hashlib.sha1(serialized.encode("utf-8")).hexdigest()[:12]


def _configuration_summary(config: SimulationConfig) -> dict[str, Any]:
    return {
        "chemistry_name": config.chemistry_name,
        "cells_in_series": config.cells_in_series,
        "cells_in_parallel": config.cells_in_parallel,
        "group_count": config.group_count if config.group_count is not None else config.cells_in_series,
        "electrical_model_type": config.electrical_model.model_type,
        "ambient_temp_c": config.ambient_temp_c,
    }


def _snapshot_metadata(metadata: dict[str, Any]) -> dict[str, Any]:
    keys = (
        "dataset_id",
        "display_name",
        "description",
        "chemistry",
        "form_factor",
        "nominal_voltage_v",
        "nominal_capacity_ah",
        "temperature_range_c",
        "current_profile_type",
        "tags",
        "source",
        "status",
        "created_at",
        "notes",
    )
    return {key: metadata.get(key) for key in keys if metadata.get(key) not in (None, "", ())}


def _interpretation_text(overall_status: str) -> str:
    if overall_status == "pass":
        return "Suitable for comparative trade-study use."
    if overall_status == "warning":
        return "Use caution; validation thresholds were not fully met across all selected datasets."
    return "Not yet suitable for decision support; validation thresholds were not met."


def build_validation_scorecard(
    result: SimulationResult,
    dataset: TruthDataset,
    *,
    dataset_path: str,
    base_config: SimulationConfig,
    requested_metrics: Sequence[str],
    thresholds: dict[str, float],
    extra_warnings: Sequence[str] = (),
) -> ValidationScorecard:
    metric_results: list[ValidationMetricResult] = []
    warnings = list(extra_warnings)
    normalized_metrics = normalize_requested_metrics(requested_metrics)
    metadata = dict(dataset.metadata)

    dataset_status = str(metadata.get("status", "experimental"))
    if dataset_status == "experimental":
        warnings.append("Dataset is marked experimental and should be treated as directional evidence, not a canonical reference.")
    if dataset_status == "deprecated":
        warnings.append("Dataset is marked deprecated and should not anchor final trade-study decisions.")

    dataset_chemistry = str(metadata.get("chemistry", "")).strip()
    if dataset_chemistry and dataset_chemistry != base_config.chemistry_name:
        warnings.append("Truth dataset chemistry metadata does not match the active pack chemistry.")

    for metric_name in normalized_metrics:
        definition = _METRIC_DEFINITIONS[metric_name]
        value = float(definition.calculator(result, dataset))
        threshold_value = thresholds.get(definition.threshold_key)
        passed = value <= threshold_value if threshold_value is not None else None
        if threshold_value is None:
            warnings.append(f"No threshold was supplied for {definition.display_name}.")
        metric_results.append(
            ValidationMetricResult(
                metric_id=definition.metric_id,
                display_name=definition.display_name,
                value=value,
                unit=definition.unit,
                threshold_key=definition.threshold_key,
                threshold_value=threshold_value,
                passed=passed,
            )
        )

    evaluated_passes = [item.passed for item in metric_results if item.passed is not None]
    overall_passed = bool(evaluated_passes) and all(evaluated_passes)
    overall_status = "pass" if overall_passed else ("warning" if any(evaluated_passes) else "fail")

    return ValidationScorecard(
        dataset_id=str(metadata.get("dataset_id", Path(dataset_path).stem if dataset_path else "truth_dataset")),
        dataset_display_name=str(metadata.get("display_name", Path(dataset_path).stem if dataset_path else "Truth Dataset")),
        dataset_status=dataset_status,
        dataset_path=dataset_path,
        metric_results=tuple(metric_results),
        overall_passed=overall_passed,
        overall_status=overall_status,
        warnings=tuple(dict.fromkeys(warnings)),
        metadata_snapshot=_snapshot_metadata(metadata),
        run_timestamp=datetime.now(timezone.utc).isoformat(),
        configuration_fingerprint=_configuration_fingerprint(base_config),
        configuration_summary=_configuration_summary(base_config),
        interpretation_text=_interpretation_text(overall_status),
    )


def _scorecard_strength(scorecard: ValidationScorecard) -> float:
    scores: list[float] = []
    for metric in scorecard.metric_results:
        if metric.threshold_value is None:
            scores.append(-metric.value)
            continue
        threshold = max(metric.threshold_value, 1.0e-9)
        scores.append((metric.threshold_value - metric.value) / threshold)
    base_score = mean(scores) if scores else -1.0
    if scorecard.overall_status == "warning":
        base_score -= 0.25
    if scorecard.overall_status == "fail":
        base_score -= 1.0
    if scorecard.dataset_status == "experimental":
        base_score -= 0.10
    if scorecard.dataset_status == "deprecated":
        base_score -= 0.30
    return base_score


def summarize_validation_scorecards(scorecards: Sequence[ValidationScorecard]) -> AggregateValidationSummary:
    if not scorecards:
        return AggregateValidationSummary(
            dataset_count=0,
            passed_count=0,
            failed_count=0,
            overall_status="warning",
            key_warnings=("No validation datasets were evaluated.",),
            recommendation_text="Validation data is not available yet for decision support.",
        )

    passed_count = sum(1 for item in scorecards if item.overall_passed)
    failed_count = len(scorecards) - passed_count
    overall_status = "pass" if failed_count == 0 else ("warning" if passed_count > 0 else "fail")

    strongest = max(scorecards, key=_scorecard_strength)
    weakest = min(scorecards, key=_scorecard_strength)

    metric_buckets: dict[str, list[tuple[ValidationScorecard, ValidationMetricResult]]] = {}
    for scorecard in scorecards:
        for metric in scorecard.metric_results:
            metric_buckets.setdefault(metric.metric_id, []).append((scorecard, metric))

    headline_metrics: list[ValidationHeadlineMetric] = []
    for metric_id, items in metric_buckets.items():
        values = [metric.value for _, metric in items]
        best_scorecard, best_metric = min(items, key=lambda item: item[1].value)
        worst_scorecard, worst_metric = max(items, key=lambda item: item[1].value)
        headline_metrics.append(
            ValidationHeadlineMetric(
                metric_id=metric_id,
                display_name=items[0][1].display_name,
                unit=items[0][1].unit,
                average_value=mean(values),
                best_value=best_metric.value,
                worst_value=worst_metric.value,
                best_dataset_id=best_scorecard.dataset_id,
                worst_dataset_id=worst_scorecard.dataset_id,
            )
        )
    headline_metrics.sort(key=lambda item: item.display_name.lower())

    key_warnings: list[str] = []
    for scorecard in scorecards:
        key_warnings.extend(scorecard.warnings)
    key_warnings = list(dict.fromkeys(key_warnings))

    recommendation_text = _interpretation_text(overall_status)
    return AggregateValidationSummary(
        dataset_count=len(scorecards),
        passed_count=passed_count,
        failed_count=failed_count,
        overall_status=overall_status,
        strongest_dataset_id=strongest.dataset_id,
        strongest_dataset_display_name=strongest.dataset_display_name,
        weakest_dataset_id=weakest.dataset_id,
        weakest_dataset_display_name=weakest.dataset_display_name,
        headline_metrics=tuple(headline_metrics),
        key_warnings=tuple(key_warnings),
        recommendation_text=recommendation_text,
    )


def validation_scorecard_to_dict(scorecard: ValidationScorecard) -> dict[str, Any]:
    return asdict(scorecard)


def validation_summary_to_dict(summary: AggregateValidationSummary) -> dict[str, Any]:
    return asdict(summary)
