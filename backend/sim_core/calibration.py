from __future__ import annotations

import json
import math
from dataclasses import dataclass, field, replace
from pathlib import Path
from statistics import median
from typing import Any, Iterable, Sequence

from .physics.electrical import interpolate_ocv_curve
from .types import (
    CurrentProfile,
    CurrentProfilePoint,
    DegradationConfig,
    ElectricalModelConfig,
    OcvLookupPoint,
    PhysicsConfig,
    RcBranchParams,
    SimulationConfig,
    SimulationResult,
    SocLookupPoint,
)


@dataclass(frozen=True)
class TruthRecord:
    time_s: float
    current_a: float
    voltage_v: float
    temp_c: float
    soc: float
    extras: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class TruthDataset:
    metadata: dict[str, Any]
    records: tuple[TruthRecord, ...]


@dataclass(frozen=True)
class ValidationAlignment:
    truth_times_s: tuple[float, ...]
    truth_voltage_v: tuple[float, ...]
    aligned_sim_voltage_v: tuple[float, ...]
    truth_temp_c: tuple[float, ...]
    aligned_sim_temp_c: tuple[float, ...]
    truth_soc: tuple[float, ...]
    normalized_progress: tuple[float, ...]


@dataclass(frozen=True)
class CalibratedParameters:
    ocv_curve: tuple[OcvLookupPoint, ...]
    base_resistance_ohm_per_cell: float
    resistance_soc_curve: tuple[SocLookupPoint, ...]
    resistance_temperature_alpha_per_c: float
    rc_branches: tuple[RcBranchParams, ...]
    core_thermal_mass_j_per_k: float
    surface_thermal_mass_j_per_k: float
    cooling_coeff_w_per_k: float
    capacity_fade_per_throughput_ah: float
    resistance_growth_per_throughput_ah: float
    calendar_capacity_fade_per_hour: float
    calendar_resistance_growth_per_hour: float
    rc_low_soc_multiplier: float = 1.0
    age_conditioned_tail_enabled: bool = False
    age_conditioned_tail_resistance_gain: float = 0.0
    age_conditioned_tail_ocv_drop_v: float = 0.0
    age_conditioned_tail_rc_multiplier_gain: float = 0.0
    diagnostics: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class _RawTruthRecord:
    time_s: float
    current_a: float
    voltage_v: float
    temp_c: float
    soc: float | None
    extras: dict[str, Any]


@dataclass(frozen=True)
class _ResistanceEstimate:
    time_s: float
    soc: float
    temp_c: float
    resistance_ohm: float


TRUTH_DATASET_STATUSES = ("canonical", "trusted", "experimental", "deprecated")
_TRUTH_DATASET_CANONICAL_KEYS = {
    "chemistry_name": "chemistry",
    "cell_form_factor": "form_factor",
    "capacity_ah": "nominal_capacity_ah",
    "cell_capacity_ah": "nominal_capacity_ah",
}
_TRUTH_DATASET_REQUIRED_METADATA_FIELDS = (
    "dataset_id",
    "display_name",
    "chemistry",
    "form_factor",
    "nominal_voltage_v",
    "nominal_capacity_ah",
    "current_profile_type",
    "source",
    "status",
)
_LOW_SOC_FOCUSED_OCV_SOC_KNOTS: tuple[float, ...] = (
    0.0,
    0.02,
    0.05,
    0.09,
    0.14,
    0.22,
    0.33,
    0.50,
    0.66,
    0.82,
    0.94,
    1.0,
)
_LOW_SOC_FOCUSED_RESISTANCE_SOC_KNOTS: tuple[float, ...] = (
    0.0,
    0.03,
    0.08,
    0.15,
    0.25,
    0.40,
    0.55,
    0.70,
    0.85,
    1.0,
)
_REFERENCE_RESISTANCE_SOC_MIN = 0.35
_REFERENCE_RESISTANCE_SOC_MAX = 0.80
_TAIL_RESISTANCE_SOC_MAX = 0.15
_AGE_CONDITIONED_TAIL_SOC_MAX = 0.20


def _clamp(value: float, low: float, high: float) -> float:
    return max(low, min(value, high))


def _blend_scalar(base_value: float, calibrated_value: float, blend: float) -> float:
    alpha = _clamp(float(blend), 0.0, 1.0)
    return float(base_value) + (float(calibrated_value) - float(base_value)) * alpha


def _infer_aging_stage_from_metadata(metadata: dict[str, Any]) -> str:
    cycle_index = metadata.get("source_cycle_index")
    try:
        cycle_value = int(cycle_index)
    except Exception:
        return "unknown"
    if cycle_value <= 200:
        return "early_life"
    if cycle_value <= 500:
        return "mid_life"
    return "late_life"


def _truth_dataset_error(message: str, *, source_path: str | None = None) -> ValueError:
    if source_path:
        return ValueError(f"{message} [dataset={source_path}]")
    return ValueError(message)


def _row_label(row_index: int) -> str:
    return f"row {row_index + 1}"


def _coerce_finite_float(
    value: Any,
    field_name: str,
    *,
    source_path: str | None = None,
    row_index: int | None = None,
) -> float:
    try:
        parsed = float(value)
    except Exception as exc:
        location = f" in {_row_label(row_index)}" if row_index is not None else ""
        raise _truth_dataset_error(f"Truth dataset field '{field_name}'{location} must be numeric: {exc}", source_path=source_path)
    if not math.isfinite(parsed):
        location = f" in {_row_label(row_index)}" if row_index is not None else ""
        raise _truth_dataset_error(f"Truth dataset field '{field_name}'{location} must be finite.", source_path=source_path)
    return parsed


def _coerce_optional_finite_float(
    value: Any,
    field_name: str,
    *,
    source_path: str | None = None,
) -> float | None:
    if value is None:
        return None
    return _coerce_finite_float(value, field_name, source_path=source_path)


def _derive_display_name(value: str) -> str:
    cleaned = value.replace("_", " ").replace("-", " ").strip()
    return " ".join(part.capitalize() for part in cleaned.split()) if cleaned else "Truth Dataset"


def _normalize_truth_dataset_metadata(
    metadata: dict[str, Any],
    *,
    strict_metadata: bool,
    source_path: str | None = None,
) -> dict[str, Any]:
    normalized = dict(metadata)
    for source_key, target_key in _TRUTH_DATASET_CANONICAL_KEYS.items():
        if target_key not in normalized and normalized.get(source_key) not in (None, ""):
            normalized[target_key] = normalized[source_key]

    dataset_id = str(normalized.get("dataset_id", "")).strip()
    if strict_metadata and not dataset_id:
        raise _truth_dataset_error("Truth dataset metadata must define 'dataset_id'.", source_path=source_path)
    if not dataset_id:
        dataset_id = Path(source_path).stem if source_path else "legacy_truth_dataset"
    normalized["dataset_id"] = dataset_id

    display_name = str(normalized.get("display_name", "")).strip()
    if strict_metadata and not display_name:
        raise _truth_dataset_error("Truth dataset metadata must define 'display_name'.", source_path=source_path)
    if not display_name:
        display_name = _derive_display_name(dataset_id)
    normalized["display_name"] = display_name

    description = str(normalized.get("description", "")).strip()
    normalized["description"] = description

    chemistry = str(normalized.get("chemistry", "")).strip()
    if strict_metadata and not chemistry:
        raise _truth_dataset_error("Truth dataset metadata must define 'chemistry'.", source_path=source_path)
    normalized["chemistry"] = chemistry

    form_factor = str(normalized.get("form_factor", "")).strip()
    if strict_metadata and not form_factor:
        raise _truth_dataset_error("Truth dataset metadata must define 'form_factor'.", source_path=source_path)
    normalized["form_factor"] = form_factor

    nominal_voltage_v = _coerce_optional_finite_float(
        normalized.get("nominal_voltage_v"),
        "nominal_voltage_v",
        source_path=source_path,
    )
    if strict_metadata and nominal_voltage_v is None:
        raise _truth_dataset_error("Truth dataset metadata must define 'nominal_voltage_v'.", source_path=source_path)
    normalized["nominal_voltage_v"] = nominal_voltage_v

    nominal_capacity_ah = _coerce_optional_finite_float(
        normalized.get("nominal_capacity_ah"),
        "nominal_capacity_ah",
        source_path=source_path,
    )
    if strict_metadata and nominal_capacity_ah is None:
        raise _truth_dataset_error("Truth dataset metadata must define 'nominal_capacity_ah'.", source_path=source_path)
    normalized["nominal_capacity_ah"] = nominal_capacity_ah

    temperature_range = normalized.get("temperature_range_c")
    if temperature_range in (None, ""):
        normalized["temperature_range_c"] = ()
    else:
        if not isinstance(temperature_range, (list, tuple)) or len(temperature_range) != 2:
            raise _truth_dataset_error(
                "Truth dataset metadata field 'temperature_range_c' must contain [min_c, max_c].",
                source_path=source_path,
            )
        low_c = _coerce_finite_float(temperature_range[0], "temperature_range_c[0]", source_path=source_path)
        high_c = _coerce_finite_float(temperature_range[1], "temperature_range_c[1]", source_path=source_path)
        if high_c < low_c:
            raise _truth_dataset_error(
                "Truth dataset metadata field 'temperature_range_c' must be ordered as [min_c, max_c].",
                source_path=source_path,
            )
        normalized["temperature_range_c"] = (low_c, high_c)

    current_profile_type = str(normalized.get("current_profile_type", "")).strip()
    if strict_metadata and not current_profile_type:
        raise _truth_dataset_error("Truth dataset metadata must define 'current_profile_type'.", source_path=source_path)
    normalized["current_profile_type"] = current_profile_type

    raw_tags = normalized.get("tags") or []
    if isinstance(raw_tags, str):
        tags = tuple(tag.strip() for tag in raw_tags.split(",") if tag.strip())
    elif isinstance(raw_tags, (list, tuple)):
        tags = tuple(str(tag).strip() for tag in raw_tags if str(tag).strip())
    else:
        raise _truth_dataset_error("Truth dataset metadata field 'tags' must be a list of strings.", source_path=source_path)
    normalized["tags"] = tags

    source = str(normalized.get("source", "")).strip()
    if strict_metadata and not source:
        raise _truth_dataset_error("Truth dataset metadata must define 'source'.", source_path=source_path)
    normalized["source"] = source

    status = str(normalized.get("status", "experimental")).strip().lower()
    if status and status not in TRUTH_DATASET_STATUSES:
        raise _truth_dataset_error(
            f"Truth dataset metadata field 'status' must be one of: {', '.join(TRUTH_DATASET_STATUSES)}.",
            source_path=source_path,
        )
    if strict_metadata and not status:
        raise _truth_dataset_error("Truth dataset metadata must define 'status'.", source_path=source_path)
    normalized["status"] = status or "experimental"

    created_at = str(normalized.get("created_at", "")).strip()
    normalized["created_at"] = created_at

    notes = str(normalized.get("notes", "")).strip()
    normalized["notes"] = notes

    if strict_metadata:
        missing_fields = [field_name for field_name in _TRUTH_DATASET_REQUIRED_METADATA_FIELDS if normalized.get(field_name) in (None, "", ())]
        if missing_fields:
            raise _truth_dataset_error(
                "Truth dataset metadata is missing required field(s): " + ", ".join(missing_fields),
                source_path=source_path,
            )

    return normalized


def _median_or(values: Iterable[float], default: float) -> float:
    cleaned = [value for value in values if math.isfinite(value)]
    return median(cleaned) if cleaned else default


def _trapz(xs: Sequence[float], ys: Sequence[float]) -> float:
    if len(xs) != len(ys) or len(xs) < 2:
        return 0.0

    total = 0.0
    for left, right, y_left, y_right in zip(xs[:-1], xs[1:], ys[:-1], ys[1:]):
        total += (right - left) * (y_left + y_right) * 0.5
    return total


def _interpolate_pairs(points: Sequence[tuple[float, float]], x: float) -> float:
    if not points:
        raise ValueError("Interpolation requires at least one point.")
    if x <= points[0][0]:
        return points[0][1]
    if x >= points[-1][0]:
        return points[-1][1]

    for (x0, y0), (x1, y1) in zip(points[:-1], points[1:]):
        if x0 <= x <= x1:
            span = max(x1 - x0, 1e-9)
            weight = (x - x0) / span
            return y0 + (y1 - y0) * weight
    return points[-1][1]


def _interpolate_curve(curve: Sequence[OcvLookupPoint], soc: float) -> float:
    return interpolate_ocv_curve(tuple(curve), _clamp(soc, 0.0, 1.0))


def _positive_time_deltas(records: Sequence[TruthRecord]) -> list[float]:
    deltas: list[float] = []
    for previous, current in zip(records[:-1], records[1:]):
        delta = current.time_s - previous.time_s
        if delta > 0.0:
            deltas.append(delta)
    return deltas


def _bin_center(index: int, bin_count: int) -> float:
    if bin_count <= 1:
        return 0.0
    return index / float(bin_count - 1)


def _nearest_knot_index(soc: float, knots: Sequence[float]) -> int:
    if not knots:
        return 0
    clamped_soc = _clamp(soc, 0.0, 1.0)
    return min(range(len(knots)), key=lambda index: abs(knots[index] - clamped_soc))


def _fill_missing_bins(values: list[float | None], fallback_start: float, fallback_end: float) -> list[float]:
    if not values:
        return []

    known_indices = [index for index, value in enumerate(values) if value is not None]
    if not known_indices:
        return [
            fallback_start + (fallback_end - fallback_start) * _bin_center(index, len(values))
            for index in range(len(values))
        ]

    filled = [float(value) if value is not None else math.nan for value in values]
    first_known = known_indices[0]
    for index in range(0, first_known):
        filled[index] = filled[first_known]

    for left_index, right_index in zip(known_indices[:-1], known_indices[1:]):
        left_value = filled[left_index]
        right_value = filled[right_index]
        gap = right_index - left_index
        for index in range(left_index + 1, right_index):
            weight = (index - left_index) / gap
            filled[index] = left_value + (right_value - left_value) * weight

    last_known = known_indices[-1]
    for index in range(last_known + 1, len(filled)):
        filled[index] = filled[last_known]

    return filled


def _enforce_monotonic_non_decreasing(values: Sequence[float]) -> list[float]:
    if not values:
        return []
    monotonic: list[float] = [float(values[0])]
    for value in values[1:]:
        monotonic.append(max(monotonic[-1], float(value)))
    return monotonic


def _dataset_capacity_ah(metadata: dict[str, Any], raw_records: Sequence[_RawTruthRecord]) -> float:
    for key in ("capacity_ah", "cell_capacity_ah", "nominal_capacity_ah"):
        value = metadata.get(key)
        if value is not None:
            parsed = float(value)
            if parsed > 0.0:
                return parsed

    estimates: list[float] = []
    known_indices = [index for index, record in enumerate(raw_records) if record.soc is not None]
    for start_index, end_index in zip(known_indices[:-1], known_indices[1:]):
        start_soc = raw_records[start_index].soc
        end_soc = raw_records[end_index].soc
        if start_soc is None or end_soc is None:
            continue

        delta_soc = start_soc - end_soc
        if abs(delta_soc) < 1e-6:
            continue

        throughput_ah = 0.0
        for index in range(start_index, end_index):
            dt_s = raw_records[index + 1].time_s - raw_records[index].time_s
            throughput_ah += raw_records[index].current_a * dt_s / 3600.0
        estimate = abs(throughput_ah / delta_soc)
        if estimate > 0.0 and math.isfinite(estimate):
            estimates.append(estimate)

    if estimates:
        return median(estimates)
    return 1.0


def _fill_missing_soc(metadata: dict[str, Any], raw_records: Sequence[_RawTruthRecord]) -> tuple[dict[str, Any], tuple[TruthRecord, ...]]:
    if not raw_records:
        raise ValueError("Truth dataset must contain at least one data row.")

    metadata_copy = dict(metadata)
    capacity_ah = _dataset_capacity_ah(metadata_copy, raw_records)
    metadata_copy.setdefault("estimated_capacity_ah", capacity_ah)

    initial_soc = metadata_copy.get("initial_soc")
    initial_soc_value = float(initial_soc) if initial_soc is not None else None
    current_soc = (
        raw_records[0].soc
        if raw_records[0].soc is not None
        else initial_soc_value
        if initial_soc_value is not None
        else 1.0
    )
    current_soc = _clamp(float(current_soc), 0.0, 1.0)

    records: list[TruthRecord] = [
        TruthRecord(
            time_s=raw_records[0].time_s,
            current_a=raw_records[0].current_a,
            voltage_v=raw_records[0].voltage_v,
            temp_c=raw_records[0].temp_c,
            soc=current_soc,
            extras=dict(raw_records[0].extras),
        )
    ]

    for previous, current in zip(raw_records[:-1], raw_records[1:]):
        dt_s = current.time_s - previous.time_s
        current_soc = _clamp(current_soc - previous.current_a * dt_s / 3600.0 / max(capacity_ah, 1e-9), 0.0, 1.0)
        if current.soc is not None:
            current_soc = _clamp(float(current.soc), 0.0, 1.0)
        records.append(
            TruthRecord(
                time_s=current.time_s,
                current_a=current.current_a,
                voltage_v=current.voltage_v,
                temp_c=current.temp_c,
                soc=current_soc,
                extras=dict(current.extras),
            )
        )

    return metadata_copy, tuple(records)


def truth_dataset_from_dict(
    payload: dict[str, Any],
    *,
    strict_metadata: bool = False,
    source_path: str | None = None,
) -> TruthDataset:
    metadata = _normalize_truth_dataset_metadata(
        dict(payload.get("metadata") or {}),
        strict_metadata=strict_metadata,
        source_path=source_path,
    )
    data = payload.get("records")
    if data is None:
        data = payload.get("data")
    if not isinstance(data, list) or not data:
        raise _truth_dataset_error("Truth dataset must contain a non-empty 'records' array.", source_path=source_path)

    raw_records: list[_RawTruthRecord] = []
    previous_time_s: float | None = None
    for row_index, item in enumerate(data):
        if not isinstance(item, dict):
            raise _truth_dataset_error(
                f"Truth dataset {_row_label(row_index)} must be an object.",
                source_path=source_path,
            )
        for required_field in ("time_s", "current_a", "voltage_v", "temp_c"):
            if required_field not in item:
                raise _truth_dataset_error(
                    f"Truth dataset {_row_label(row_index)} is missing required field '{required_field}'.",
                    source_path=source_path,
                )
        time_s = _coerce_finite_float(item["time_s"], "time_s", source_path=source_path, row_index=row_index)
        if previous_time_s is not None and time_s <= previous_time_s:
            raise _truth_dataset_error(
                "Truth dataset time_s values must be strictly increasing.",
                source_path=source_path,
            )
        previous_time_s = time_s

        current_a = _coerce_finite_float(item["current_a"], "current_a", source_path=source_path, row_index=row_index)
        voltage_v = _coerce_finite_float(item["voltage_v"], "voltage_v", source_path=source_path, row_index=row_index)
        temp_c = _coerce_finite_float(item["temp_c"], "temp_c", source_path=source_path, row_index=row_index)
        soc = (
            _coerce_finite_float(item["soc"], "soc", source_path=source_path, row_index=row_index)
            if item.get("soc") is not None
            else None
        )
        extras = {key: value for key, value in item.items() if key not in {"time_s", "current_a", "voltage_v", "temp_c", "soc"}}
        raw_records.append(
            _RawTruthRecord(
                time_s=time_s,
                current_a=current_a,
                voltage_v=voltage_v,
                temp_c=temp_c,
                soc=soc,
                extras=extras,
            )
        )

    metadata_with_soc, records = _fill_missing_soc(metadata, raw_records)
    return TruthDataset(metadata=metadata_with_soc, records=records)


def load_truth_dataset(path: str, *, strict_metadata: bool = False) -> TruthDataset:
    expanded_path = Path(path).expanduser()
    payload = json.loads(expanded_path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise _truth_dataset_error("Truth dataset root must be a JSON object.", source_path=str(expanded_path))
    return truth_dataset_from_dict(payload, strict_metadata=strict_metadata, source_path=str(expanded_path))


def _reference_resistance_ohm(estimates: Sequence[_ResistanceEstimate]) -> float:
    preferred = [
        item.resistance_ohm
        for item in estimates
        if _REFERENCE_RESISTANCE_SOC_MIN <= item.soc <= _REFERENCE_RESISTANCE_SOC_MAX
    ]
    return _median_or(preferred or (item.resistance_ohm for item in estimates), 0.01)


def _estimate_rc_low_soc_multiplier(
    estimates: Sequence[_ResistanceEstimate],
    *,
    reference_resistance_ohm: float,
) -> float:
    if len(estimates) < 3:
        return 1.0
    low_soc = [item.resistance_ohm for item in estimates if item.soc <= _TAIL_RESISTANCE_SOC_MAX]
    reference = [
        item.resistance_ohm
        for item in estimates
        if _REFERENCE_RESISTANCE_SOC_MIN <= item.soc <= _REFERENCE_RESISTANCE_SOC_MAX
    ]
    if len(low_soc) < 2 or len(reference) < 2:
        return 1.0
    return _clamp(
        _median_or(low_soc, reference_resistance_ohm) / max(_median_or(reference, reference_resistance_ohm), 1.0e-9),
        1.0,
        2.5,
    )


def _estimate_age_conditioned_tail_behavior(
    dataset: TruthDataset,
    *,
    ocv_curve: Sequence[OcvLookupPoint],
    resistance_estimates: Sequence[_ResistanceEstimate],
    reference_resistance_ohm: float,
    rc_low_soc_multiplier: float,
    low_current_threshold_a: float,
) -> tuple[bool, float, float, float]:
    low_soc_estimates = [
        item.resistance_ohm
        for item in resistance_estimates
        if item.soc <= _AGE_CONDITIONED_TAIL_SOC_MAX
    ]
    low_soc_ratio = (
        _median_or(low_soc_estimates, reference_resistance_ohm) / max(reference_resistance_ohm, 1.0e-9)
        if low_soc_estimates
        else 1.0
    )
    resistance_gain = _clamp((low_soc_ratio - 1.0) * 0.45, 0.0, 0.65)

    light_current_limit_a = max(low_current_threshold_a * 2.0, 0.25)
    low_soc_residuals_v = [
        max(_interpolate_curve(ocv_curve, record.soc) - record.voltage_v, 0.0)
        for record in dataset.records
        if record.soc <= _AGE_CONDITIONED_TAIL_SOC_MAX and abs(record.current_a) <= light_current_limit_a
    ]
    tail_ocv_drop_v = _clamp(_median_or(low_soc_residuals_v, 0.0), 0.0, 0.08)

    rc_multiplier_gain = _clamp((rc_low_soc_multiplier - 1.0) * 0.75, 0.0, 1.20)
    enabled = (
        resistance_gain > 1.0e-3
        or tail_ocv_drop_v > 1.0e-3
        or rc_multiplier_gain > 1.0e-3
    )
    return enabled, resistance_gain, tail_ocv_drop_v, rc_multiplier_gain


def _fit_ocv_curve(
    records: Sequence[TruthRecord],
    low_current_threshold_a: float,
    soc_knots: Sequence[float] = _LOW_SOC_FOCUSED_OCV_SOC_KNOTS,
) -> tuple[OcvLookupPoint, ...]:
    candidates = [record for record in records if abs(record.current_a) <= low_current_threshold_a]
    if len(candidates) < max(3, len(soc_knots) // 2):
        candidates = list(records)

    bins: list[list[float]] = [[] for _ in range(len(soc_knots))]
    for record in candidates:
        index = _nearest_knot_index(record.soc, soc_knots)
        bins[index].append(record.voltage_v)

    observed = [median(values) if values else None for values in bins]
    fallback_start = min(record.voltage_v for record in candidates)
    fallback_end = max(record.voltage_v for record in candidates)
    filled = _enforce_monotonic_non_decreasing(_fill_missing_bins(observed, fallback_start, fallback_end))

    return tuple(
        OcvLookupPoint(soc=float(soc_knots[index]), voltage_v=filled[index])
        for index in range(len(soc_knots))
    )


def _estimate_resistances(records: Sequence[TruthRecord], ocv_curve: Sequence[OcvLookupPoint]) -> list[_ResistanceEstimate]:
    current_magnitudes = [abs(record.current_a) for record in records]
    max_current = max(current_magnitudes, default=0.0)
    step_threshold_a = max(0.05, max_current * 0.05)
    load_threshold_a = max(0.1, max_current * 0.02)

    estimates: list[_ResistanceEstimate] = []
    for previous, current in zip(records[:-1], records[1:]):
        delta_current_a = current.current_a - previous.current_a
        if abs(delta_current_a) >= step_threshold_a:
            resistance_ohm = -(current.voltage_v - previous.voltage_v) / delta_current_a
            if resistance_ohm > 0.0 and math.isfinite(resistance_ohm):
                estimates.append(
                    _ResistanceEstimate(
                        time_s=current.time_s,
                        soc=(previous.soc + current.soc) * 0.5,
                        temp_c=(previous.temp_c + current.temp_c) * 0.5,
                        resistance_ohm=resistance_ohm,
                    )
                )

    for record in records:
        if abs(record.current_a) < load_threshold_a:
            continue
        ocv_v = _interpolate_curve(ocv_curve, record.soc)
        resistance_ohm = abs((ocv_v - record.voltage_v) / record.current_a)
        if resistance_ohm > 0.0 and math.isfinite(resistance_ohm):
            estimates.append(
                _ResistanceEstimate(
                    time_s=record.time_s,
                    soc=record.soc,
                    temp_c=record.temp_c,
                    resistance_ohm=resistance_ohm,
                )
            )
    return estimates


def _fit_resistance_soc_curve(
    estimates: Sequence[_ResistanceEstimate],
    *,
    reference_resistance_ohm: float | None = None,
    soc_knots: Sequence[float] = _LOW_SOC_FOCUSED_RESISTANCE_SOC_KNOTS,
) -> tuple[SocLookupPoint, ...]:
    if not estimates:
        return PhysicsConfig().resistance_soc_curve

    reference_resistance = reference_resistance_ohm or _reference_resistance_ohm(estimates)
    bins: list[list[float]] = [[] for _ in range(len(soc_knots))]
    for item in estimates:
        index = _nearest_knot_index(item.soc, soc_knots)
        bins[index].append(item.resistance_ohm / max(reference_resistance, 1e-9))

    observed = [median(values) if values else None for values in bins]
    filled = _fill_missing_bins(observed, 1.0, 1.0)
    return tuple(
        SocLookupPoint(soc=float(soc_knots[index]), multiplier=max(filled[index], 0.05))
        for index in range(len(soc_knots))
    )


def _fit_temperature_alpha(estimates: Sequence[_ResistanceEstimate], reference_temp_c: float) -> float:
    if len(estimates) < 2:
        return 0.0

    baseline = _median_or((item.resistance_ohm for item in estimates), 1.0)
    x_values = [item.temp_c - reference_temp_c for item in estimates]
    y_values = [(item.resistance_ohm / max(baseline, 1e-9)) - 1.0 for item in estimates]
    denominator = sum(value * value for value in x_values)
    if denominator <= 1e-9:
        return 0.0
    slope = sum(x * y for x, y in zip(x_values, y_values)) / denominator
    return _clamp(slope, -0.1, 0.1)


def _fit_rc_branches(
    records: Sequence[TruthRecord],
    ocv_curve: Sequence[OcvLookupPoint],
    base_resistance_ohm: float,
    rc_branch_count: int,
) -> tuple[RcBranchParams, ...]:
    if rc_branch_count <= 0:
        return ()

    current_magnitudes = [abs(record.current_a) for record in records]
    max_current = max(current_magnitudes, default=0.0)
    step_threshold_a = max(0.05, max_current * 0.05)
    fits: list[tuple[float, float]] = []

    for step_index in range(1, len(records) - 3):
        delta_current_a = records[step_index].current_a - records[step_index - 1].current_a
        if abs(delta_current_a) < step_threshold_a:
            continue

        segment_end = step_index + 1
        while (
            segment_end + 1 < len(records)
            and abs(records[segment_end + 1].current_a - records[step_index].current_a) < step_threshold_a * 0.25
        ):
            segment_end += 1
        if segment_end - step_index < 2:
            continue

        times_s: list[float] = []
        residuals_v: list[float] = []
        for index in range(step_index, segment_end + 1):
            record = records[index]
            ocv_v = _interpolate_curve(ocv_curve, record.soc)
            residual_v = (ocv_v - record.voltage_v) - record.current_a * base_resistance_ohm
            times_s.append(record.time_s - records[step_index].time_s)
            residuals_v.append(residual_v)

        final_residual_v = residuals_v[-1]
        deviations_v = [abs(value - final_residual_v) for value in residuals_v]
        initial_deviation_v = deviations_v[0]
        if initial_deviation_v <= 1e-6:
            continue

        tau_s = max(times_s[-1] * 0.5, 1.0)
        target_v = initial_deviation_v / math.e
        for time_s, deviation_v in zip(times_s[1:], deviations_v[1:]):
            if deviation_v <= target_v:
                tau_s = max(time_s, 1.0)
                break

        branch_resistance_ohm = max(initial_deviation_v / max(abs(delta_current_a), 1e-9), 1e-6)
        fits.append((tau_s, branch_resistance_ohm))

    if not fits:
        fits.append((15.0, max(base_resistance_ohm * 0.35, 1e-4)))

    fits.sort(key=lambda item: item[0])
    branches: list[RcBranchParams] = []
    if len(fits) >= rc_branch_count:
        for branch_index in range(rc_branch_count):
            chunk_start = round(branch_index * len(fits) / rc_branch_count)
            chunk_end = round((branch_index + 1) * len(fits) / rc_branch_count)
            chunk = fits[chunk_start:chunk_end] or [fits[min(branch_index, len(fits) - 1)]]
            tau_s = _median_or((item[0] for item in chunk), 15.0 * (branch_index + 1))
            resistance_ohm = _median_or((item[1] for item in chunk), max(base_resistance_ohm * 0.35, 1e-4))
            branches.append(
                RcBranchParams(
                    resistance_ohm=resistance_ohm,
                    capacitance_f=max(tau_s / max(resistance_ohm, 1e-9), 1.0),
                )
            )
    else:
        base_tau_s = _median_or((item[0] for item in fits), 15.0)
        base_branch_resistance_ohm = _median_or((item[1] for item in fits), max(base_resistance_ohm * 0.35, 1e-4))
        for branch_index in range(rc_branch_count):
            tau_s = max(base_tau_s * (3.0**branch_index), 1.0)
            resistance_ohm = max(base_branch_resistance_ohm * (0.55**branch_index), 1e-5)
            branches.append(
                RcBranchParams(
                    resistance_ohm=resistance_ohm,
                    capacitance_f=max(tau_s / resistance_ohm, 1.0),
                )
            )

    return tuple(branches[:rc_branch_count])


def _solve_two_variable_least_squares(x1: Sequence[float], x2: Sequence[float], y: Sequence[float]) -> tuple[float, float]:
    s11 = sum(value * value for value in x1)
    s22 = sum(value * value for value in x2)
    s12 = sum(left * right for left, right in zip(x1, x2))
    sy1 = sum(left * target for left, target in zip(x1, y))
    sy2 = sum(right * target for right, target in zip(x2, y))
    determinant = s11 * s22 - s12 * s12
    if abs(determinant) <= 1e-9:
        return 0.0, 0.0
    return (
        (sy1 * s22 - sy2 * s12) / determinant,
        (s11 * sy2 - s12 * sy1) / determinant,
    )


def _fit_thermal_parameters(
    records: Sequence[TruthRecord],
    ocv_curve: Sequence[OcvLookupPoint],
    ambient_temp_c: float,
    surface_mass_fraction: float = 0.35,
) -> tuple[float, float, float]:
    heat_w: list[float] = []
    cooling_delta_c: list[float] = []
    temperature_rate_c_per_s: list[float] = []

    for previous, current in zip(records[:-1], records[1:]):
        dt_s = current.time_s - previous.time_s
        if dt_s <= 0.0:
            continue
        ocv_v = _interpolate_curve(ocv_curve, previous.soc)
        heat_generation_w = abs(previous.current_a) * abs(ocv_v - previous.voltage_v)
        heat_w.append(heat_generation_w)
        cooling_delta_c.append(-(previous.temp_c - ambient_temp_c))
        temperature_rate_c_per_s.append((current.temp_c - previous.temp_c) / dt_s)

    coefficient_heat, coefficient_cooling = _solve_two_variable_least_squares(heat_w, cooling_delta_c, temperature_rate_c_per_s)
    if coefficient_heat > 1e-9:
        total_thermal_mass_j_per_k = 1.0 / coefficient_heat
    else:
        temp_span_c = max(max((record.temp_c for record in records), default=ambient_temp_c) - ambient_temp_c, 1.0)
        total_heat_j = sum(heat * max(delta, 0.0) for heat, delta in zip(heat_w, _positive_time_deltas(records)))
        total_thermal_mass_j_per_k = max(total_heat_j / temp_span_c, 1.0)

    cooling_coeff_w_per_k = max(coefficient_cooling * total_thermal_mass_j_per_k, 0.0)
    surface_fraction = _clamp(surface_mass_fraction, 0.05, 0.95)
    surface_mass_j_per_k = max(total_thermal_mass_j_per_k * surface_fraction, 1.0)
    core_mass_j_per_k = max(total_thermal_mass_j_per_k - surface_mass_j_per_k, 1.0)
    return core_mass_j_per_k, surface_mass_j_per_k, max(cooling_coeff_w_per_k, 0.01)


def _fit_degradation_parameters(
    records: Sequence[TruthRecord],
    estimates: Sequence[_ResistanceEstimate],
) -> tuple[float, float, float, float]:
    if len(records) < 2:
        return (0.0, 0.0, 0.0, 0.0)

    elapsed_h = max((records[-1].time_s - records[0].time_s) / 3600.0, 1e-9)
    throughput_ah = sum(abs(previous.current_a) * (current.time_s - previous.time_s) / 3600.0 for previous, current in zip(records[:-1], records[1:]))
    throughput_ah = max(throughput_ah, 1e-9)

    early_count = max(len(records) // 3, 1)
    late_records = records[-early_count:]
    early_records = records[:early_count]
    early_voltage_v = _median_or((record.voltage_v for record in early_records), records[0].voltage_v)
    late_voltage_v = _median_or((record.voltage_v for record in late_records), records[-1].voltage_v)
    capacity_fade_fraction = max((early_voltage_v - late_voltage_v) / max(abs(early_voltage_v), 1e-9), 0.0)

    if estimates:
        early_estimates = estimates[: max(len(estimates) // 3, 1)]
        late_estimates = estimates[-max(len(estimates) // 3, 1):]
        early_resistance_ohm = _median_or((item.resistance_ohm for item in early_estimates), estimates[0].resistance_ohm)
        late_resistance_ohm = _median_or((item.resistance_ohm for item in late_estimates), estimates[-1].resistance_ohm)
        resistance_growth_fraction = max(late_resistance_ohm / max(early_resistance_ohm, 1e-9) - 1.0, 0.0)
    else:
        resistance_growth_fraction = 0.0

    return (
        max(capacity_fade_fraction / throughput_ah, 0.0),
        max(resistance_growth_fraction / throughput_ah, 0.0),
        max(capacity_fade_fraction / elapsed_h, 0.0),
        max(resistance_growth_fraction / elapsed_h, 0.0),
    )


def calibrate_parameters(dataset: TruthDataset, rc_branch_count: int = 1) -> CalibratedParameters:
    if rc_branch_count < 0:
        raise ValueError("rc_branch_count must be >= 0.")

    low_current_threshold_a = float(dataset.metadata.get("low_current_threshold_a", 0.05))
    ambient_temp_c = float(dataset.metadata.get("ambient_temp_c", dataset.records[0].temp_c))

    ocv_curve = _fit_ocv_curve(dataset.records, low_current_threshold_a=low_current_threshold_a)
    resistance_estimates = _estimate_resistances(dataset.records, ocv_curve)
    base_resistance_ohm = _reference_resistance_ohm(resistance_estimates) if resistance_estimates else 0.01
    resistance_soc_curve = _fit_resistance_soc_curve(
        resistance_estimates,
        reference_resistance_ohm=base_resistance_ohm,
    )
    resistance_temperature_alpha_per_c = _fit_temperature_alpha(
        resistance_estimates,
        reference_temp_c=ambient_temp_c,
    )
    rc_branches = _fit_rc_branches(
        dataset.records,
        ocv_curve,
        base_resistance_ohm=base_resistance_ohm,
        rc_branch_count=rc_branch_count,
    )
    core_thermal_mass_j_per_k, surface_thermal_mass_j_per_k, cooling_coeff_w_per_k = _fit_thermal_parameters(
        dataset.records,
        ocv_curve,
        ambient_temp_c=ambient_temp_c,
    )
    (
        capacity_fade_per_throughput_ah,
        resistance_growth_per_throughput_ah,
        calendar_capacity_fade_per_hour,
        calendar_resistance_growth_per_hour,
    ) = _fit_degradation_parameters(dataset.records, resistance_estimates)
    rc_low_soc_multiplier = _estimate_rc_low_soc_multiplier(
        resistance_estimates,
        reference_resistance_ohm=base_resistance_ohm,
    )
    (
        age_conditioned_tail_enabled,
        age_conditioned_tail_resistance_gain,
        age_conditioned_tail_ocv_drop_v,
        age_conditioned_tail_rc_multiplier_gain,
    ) = _estimate_age_conditioned_tail_behavior(
        dataset,
        ocv_curve=ocv_curve,
        resistance_estimates=resistance_estimates,
        reference_resistance_ohm=base_resistance_ohm,
        rc_low_soc_multiplier=rc_low_soc_multiplier,
        low_current_threshold_a=low_current_threshold_a,
    )
    aging_stage = _infer_aging_stage_from_metadata(dataset.metadata)

    return CalibratedParameters(
        ocv_curve=ocv_curve,
        base_resistance_ohm_per_cell=base_resistance_ohm,
        resistance_soc_curve=resistance_soc_curve,
        resistance_temperature_alpha_per_c=resistance_temperature_alpha_per_c,
        rc_branches=rc_branches,
        core_thermal_mass_j_per_k=core_thermal_mass_j_per_k,
        surface_thermal_mass_j_per_k=surface_thermal_mass_j_per_k,
        cooling_coeff_w_per_k=cooling_coeff_w_per_k,
        capacity_fade_per_throughput_ah=capacity_fade_per_throughput_ah,
        resistance_growth_per_throughput_ah=resistance_growth_per_throughput_ah,
        calendar_capacity_fade_per_hour=calendar_capacity_fade_per_hour,
        calendar_resistance_growth_per_hour=calendar_resistance_growth_per_hour,
        rc_low_soc_multiplier=rc_low_soc_multiplier,
        age_conditioned_tail_enabled=age_conditioned_tail_enabled,
        age_conditioned_tail_resistance_gain=age_conditioned_tail_resistance_gain,
        age_conditioned_tail_ocv_drop_v=age_conditioned_tail_ocv_drop_v,
        age_conditioned_tail_rc_multiplier_gain=age_conditioned_tail_rc_multiplier_gain,
        diagnostics={
            "record_count": len(dataset.records),
            "low_current_threshold_a": low_current_threshold_a,
            "base_resistance_ohm": base_resistance_ohm,
            "resistance_estimate_count": len(resistance_estimates),
            "reference_resistance_ohm": base_resistance_ohm,
            "rc_low_soc_multiplier": rc_low_soc_multiplier,
            "age_conditioned_tail_enabled": age_conditioned_tail_enabled,
            "age_conditioned_tail_resistance_gain": age_conditioned_tail_resistance_gain,
            "age_conditioned_tail_ocv_drop_v": age_conditioned_tail_ocv_drop_v,
            "age_conditioned_tail_rc_multiplier_gain": age_conditioned_tail_rc_multiplier_gain,
            "aging_stage": aging_stage,
            "ocv_soc_knot_count": len(ocv_curve),
            "resistance_soc_knot_count": len(resistance_soc_curve),
        },
    )


def apply_calibration_to_configs(
    base_physics: PhysicsConfig,
    base_electrical: ElectricalModelConfig,
    base_degradation: DegradationConfig,
    calibrated: CalibratedParameters,
    *,
    electro_blend: float = 1.0,
    thermal_blend: float = 1.0,
) -> tuple[PhysicsConfig, ElectricalModelConfig, DegradationConfig]:
    electro_alpha = _clamp(electro_blend, 0.0, 1.0)
    thermal_alpha = _clamp(thermal_blend, 0.0, 1.0)
    blended_r0 = _blend_scalar(
        base_electrical.r0_ohm_per_cell if base_electrical.r0_ohm_per_cell is not None else calibrated.base_resistance_ohm_per_cell,
        calibrated.base_resistance_ohm_per_cell,
        electro_alpha,
    )
    blended_temp_alpha = _blend_scalar(
        base_physics.resistance_temperature_alpha_per_c,
        calibrated.resistance_temperature_alpha_per_c,
        thermal_alpha,
    )
    blended_rc_low_soc_multiplier = _blend_scalar(
        base_physics.rc_low_soc_multiplier,
        calibrated.rc_low_soc_multiplier,
        electro_alpha,
    )
    blended_age_conditioned_tail_resistance_gain = _blend_scalar(
        base_physics.age_conditioned_tail_resistance_gain,
        calibrated.age_conditioned_tail_resistance_gain,
        electro_alpha,
    )
    blended_age_conditioned_tail_ocv_drop_v = _blend_scalar(
        base_physics.age_conditioned_tail_ocv_drop_v,
        calibrated.age_conditioned_tail_ocv_drop_v,
        electro_alpha,
    )
    blended_age_conditioned_tail_rc_multiplier_gain = _blend_scalar(
        base_physics.age_conditioned_tail_rc_multiplier_gain,
        calibrated.age_conditioned_tail_rc_multiplier_gain,
        electro_alpha,
    )
    physics = replace(
        base_physics,
        ocv_curve=calibrated.ocv_curve if electro_alpha >= 0.5 else base_physics.ocv_curve,
        two_node_thermal_enabled=thermal_alpha > 0.0 or base_physics.two_node_thermal_enabled,
        resistance_vs_soc_enabled=(electro_alpha >= 0.5) or base_physics.resistance_vs_soc_enabled,
        resistance_soc_curve=calibrated.resistance_soc_curve if electro_alpha >= 0.5 else base_physics.resistance_soc_curve,
        resistance_temperature_alpha_per_c=blended_temp_alpha,
        age_conditioned_tail_enabled=(
            (
                electro_alpha >= 0.5
                and calibrated.age_conditioned_tail_enabled
                and (
                    calibrated.age_conditioned_tail_resistance_gain > 1.0e-6
                    or calibrated.age_conditioned_tail_ocv_drop_v > 1.0e-6
                    or calibrated.age_conditioned_tail_rc_multiplier_gain > 1.0e-6
                )
            )
            or base_physics.age_conditioned_tail_enabled
        ),
        age_conditioned_tail_resistance_gain=blended_age_conditioned_tail_resistance_gain,
        age_conditioned_tail_ocv_drop_v=blended_age_conditioned_tail_ocv_drop_v,
        age_conditioned_tail_rc_multiplier_gain=blended_age_conditioned_tail_rc_multiplier_gain,
        rc_state_dependence_enabled=(
            (electro_alpha >= 0.5 and bool(calibrated.rc_branches) and calibrated.rc_low_soc_multiplier > 1.01)
            or base_physics.rc_state_dependence_enabled
        ),
        rc_low_soc_multiplier=blended_rc_low_soc_multiplier,
        core_thermal_mass_j_per_k=_blend_scalar(
            base_physics.core_thermal_mass_j_per_k or calibrated.core_thermal_mass_j_per_k,
            calibrated.core_thermal_mass_j_per_k,
            thermal_alpha,
        ),
        surface_thermal_mass_j_per_k=_blend_scalar(
            base_physics.surface_thermal_mass_j_per_k or calibrated.surface_thermal_mass_j_per_k,
            calibrated.surface_thermal_mass_j_per_k,
            thermal_alpha,
        ),
    )
    electrical = replace(
        base_electrical,
        model_type=(
            ("rint" if not calibrated.rc_branches else f"{min(len(calibrated.rc_branches), 2)}rc")
            if electro_alpha >= 0.5
            else base_electrical.model_type
        ),
        r0_ohm_per_cell=blended_r0,
        rc_branches=tuple(calibrated.rc_branches[:2]) if electro_alpha >= 0.5 else base_electrical.rc_branches,
    )
    degradation = replace(
        base_degradation,
        throughput_capacity_fade_per_ah=calibrated.capacity_fade_per_throughput_ah,
        throughput_resistance_growth_per_ah=calibrated.resistance_growth_per_throughput_ah,
        capacity_fade_per_throughput_ah=calibrated.capacity_fade_per_throughput_ah,
        resistance_growth_per_throughput_ah=calibrated.resistance_growth_per_throughput_ah,
        calendar_capacity_fade_per_hour=calibrated.calendar_capacity_fade_per_hour,
        calendar_resistance_growth_per_hour=calibrated.calendar_resistance_growth_per_hour,
    )
    return physics, electrical, degradation


def apply_calibration_to_simulation_config(
    base_config: SimulationConfig,
    calibrated: CalibratedParameters,
    *,
    electro_blend: float = 1.0,
    thermal_blend: float = 1.0,
) -> SimulationConfig:
    physics, electrical, degradation = apply_calibration_to_configs(
        base_config.physics,
        base_config.electrical_model,
        base_config.degradation,
        calibrated,
        electro_blend=electro_blend,
        thermal_blend=thermal_blend,
    )
    return replace(
        base_config,
        internal_resistance_ohm_per_cell=electrical.r0_ohm_per_cell or calibrated.base_resistance_ohm_per_cell,
        physics=physics,
        electrical_model=electrical,
        degradation=degradation,
        cooling_coeff_w_per_k=_blend_scalar(
            base_config.cooling_coeff_w_per_k,
            calibrated.cooling_coeff_w_per_k,
            thermal_blend,
        ),
    )


def build_current_profile_from_truth(dataset: TruthDataset) -> CurrentProfile:
    points: list[CurrentProfilePoint] = []
    last_time_s: int | None = None
    last_current_a: float | None = None

    for record in dataset.records:
        time_s = int(round(record.time_s))
        if last_time_s is not None and time_s < last_time_s:
            raise ValueError("Truth dataset timestamps must be monotonic after rounding to whole seconds.")
        if not points or last_current_a is None or abs(record.current_a - last_current_a) > 1e-9:
            if points and points[-1].time_s == time_s:
                points[-1] = CurrentProfilePoint(time_s=time_s, current_a=record.current_a)
            else:
                points.append(CurrentProfilePoint(time_s=time_s, current_a=record.current_a))
        last_time_s = time_s
        last_current_a = record.current_a

    if not points:
        points.append(CurrentProfilePoint(time_s=0, current_a=0.0))
    return CurrentProfile(points=tuple(points))


def _aging_stage_scale(physics: PhysicsConfig, aging_stage: str) -> float:
    return {
        "early_life": float(physics.age_conditioned_tail_early_life_scale),
        "mid_life": float(physics.age_conditioned_tail_mid_life_scale),
        "late_life": float(physics.age_conditioned_tail_late_life_scale),
        "unknown": float(physics.age_conditioned_tail_unknown_scale),
    }.get(str(aging_stage).strip().lower(), float(physics.age_conditioned_tail_unknown_scale))


def _apply_age_conditioned_tail_physics(
    physics: PhysicsConfig,
    *,
    aging_stage: str,
) -> PhysicsConfig:
    if not physics.age_conditioned_tail_enabled:
        return physics

    stage_scale = _aging_stage_scale(physics, aging_stage)
    if stage_scale <= 1.0e-9:
        return physics

    soc_threshold = _clamp(float(physics.age_conditioned_tail_soc_threshold), 0.0, 1.0)

    adjusted_resistance_curve = physics.resistance_soc_curve
    if physics.resistance_soc_curve and physics.age_conditioned_tail_resistance_gain > 0.0:
        adjusted_resistance_curve = tuple(
            SocLookupPoint(
                soc=point.soc,
                multiplier=max(
                    point.multiplier
                    * (
                        1.0
                        + _clamp((soc_threshold - point.soc) / max(soc_threshold, 1.0e-9), 0.0, 1.0)
                        * physics.age_conditioned_tail_resistance_gain
                        * stage_scale
                    ),
                    0.05,
                ),
            )
            for point in physics.resistance_soc_curve
        )

    adjusted_ocv_curve = physics.ocv_curve
    if physics.ocv_curve and physics.age_conditioned_tail_ocv_drop_v > 0.0:
        adjusted_ocv_curve = tuple(
            OcvLookupPoint(
                soc=point.soc,
                voltage_v=point.voltage_v
                - (
                    _clamp((soc_threshold - point.soc) / max(soc_threshold, 1.0e-9), 0.0, 1.0)
                    * physics.age_conditioned_tail_ocv_drop_v
                    * stage_scale
                ),
            )
            for point in physics.ocv_curve
        )

    adjusted_rc_low_soc_multiplier = physics.rc_low_soc_multiplier
    if physics.age_conditioned_tail_rc_multiplier_gain > 0.0:
        adjusted_rc_low_soc_multiplier = max(
            physics.rc_low_soc_multiplier * (1.0 + physics.age_conditioned_tail_rc_multiplier_gain * stage_scale),
            1.0,
        )

    return replace(
        physics,
        resistance_soc_curve=adjusted_resistance_curve,
        ocv_curve=adjusted_ocv_curve,
        rc_low_soc_multiplier=adjusted_rc_low_soc_multiplier,
    )


def build_validation_config(base_config: SimulationConfig, dataset: TruthDataset) -> SimulationConfig:
    deltas = _positive_time_deltas(dataset.records)
    rounded_deltas = [max(1, int(round(delta))) for delta in deltas if delta > 0.0]
    time_step_s = min(rounded_deltas) if rounded_deltas else max(base_config.time_step_s, 1)
    duration_s = max(1, int(math.ceil(dataset.records[-1].time_s)))
    initial_soc = float(dataset.metadata.get("initial_soc", dataset.records[0].soc))
    ambient_temp_c = float(dataset.metadata.get("ambient_temp_c", base_config.ambient_temp_c))
    fallback_current_a = dataset.records[0].current_a if dataset.records else base_config.discharge_current_a
    aging_stage = _infer_aging_stage_from_metadata(dataset.metadata)
    validation_physics = _apply_age_conditioned_tail_physics(
        base_config.physics,
        aging_stage=aging_stage,
    )

    return replace(
        base_config,
        duration_s=duration_s,
        time_step_s=time_step_s,
        initial_soc=_clamp(initial_soc, 0.0, 1.0),
        ambient_temp_c=ambient_temp_c,
        discharge_current_a=fallback_current_a,
        current_profile=build_current_profile_from_truth(dataset),
        physics=validation_physics,
    )


def build_validation_alignment(result: SimulationResult, dataset: TruthDataset) -> ValidationAlignment:
    truth_times = tuple(record.time_s for record in dataset.records)
    truth_voltage_v = tuple(record.voltage_v for record in dataset.records)
    truth_temp_c = tuple(record.temp_c for record in dataset.records)
    truth_soc = tuple(_clamp(record.soc, 0.0, 1.0) for record in dataset.records)
    sim_times = [float(point.time_s) for point in result.time_series]
    sim_voltage_v = [point.pack_voltage_v for point in result.time_series]
    sim_temp_c = [point.pack_temp_avg_c for point in result.time_series]
    aligned_voltage_v = tuple(_interpolate_series(sim_times, sim_voltage_v, truth_times))
    aligned_temp_c = tuple(_interpolate_series(sim_times, sim_temp_c, truth_times))

    if len(truth_times) <= 1:
        normalized_progress = tuple(0.0 for _ in truth_times)
    else:
        total_duration_s = max(truth_times[-1] - truth_times[0], 1.0e-9)
        normalized_progress = tuple(
            _clamp((time_s - truth_times[0]) / total_duration_s, 0.0, 1.0)
            for time_s in truth_times
        )

    return ValidationAlignment(
        truth_times_s=truth_times,
        truth_voltage_v=truth_voltage_v,
        aligned_sim_voltage_v=aligned_voltage_v,
        truth_temp_c=truth_temp_c,
        aligned_sim_temp_c=aligned_temp_c,
        truth_soc=truth_soc,
        normalized_progress=normalized_progress,
    )


def _interpolate_series(xs: Sequence[float], ys: Sequence[float], targets: Sequence[float]) -> list[float]:
    if not xs or not ys or len(xs) != len(ys):
        return [0.0 for _ in targets]

    result: list[float] = []
    source_index = 0
    for target in targets:
        if target <= xs[0]:
            result.append(ys[0])
            continue
        if target >= xs[-1]:
            result.append(ys[-1])
            continue
        while source_index + 1 < len(xs) and xs[source_index + 1] < target:
            source_index += 1
        x0 = xs[source_index]
        x1 = xs[source_index + 1]
        y0 = ys[source_index]
        y1 = ys[source_index + 1]
        weight = (target - x0) / max(x1 - x0, 1e-9)
        result.append(y0 + (y1 - y0) * weight)
    return result


def compute_rmse_voltage(result: SimulationResult, dataset: TruthDataset) -> float:
    alignment = build_validation_alignment(result, dataset)
    squared_errors = [
        (sim - truth) ** 2
        for sim, truth in zip(alignment.aligned_sim_voltage_v, alignment.truth_voltage_v)
    ]
    return math.sqrt(sum(squared_errors) / max(len(squared_errors), 1))


def compute_energy_error(result: SimulationResult, dataset: TruthDataset) -> float:
    alignment = build_validation_alignment(result, dataset)
    truth_times = list(alignment.truth_times_s)
    truth_current_a = [record.current_a for record in dataset.records]
    truth_voltage_v = list(alignment.truth_voltage_v)
    aligned_voltage_v = list(alignment.aligned_sim_voltage_v)
    energy_true_wh = _trapz(truth_times, [voltage * current for voltage, current in zip(truth_voltage_v, truth_current_a)]) / 3600.0
    energy_sim_wh = _trapz(truth_times, [voltage * current for voltage, current in zip(aligned_voltage_v, truth_current_a)]) / 3600.0
    return abs(energy_sim_wh - energy_true_wh) / max(abs(energy_true_wh), 1e-9)


def compute_rmse_temp(result: SimulationResult, dataset: TruthDataset) -> float:
    alignment = build_validation_alignment(result, dataset)
    squared_errors = [
        (sim - truth) ** 2
        for sim, truth in zip(alignment.aligned_sim_temp_c, alignment.truth_temp_c)
    ]
    return math.sqrt(sum(squared_errors) / max(len(squared_errors), 1))


def compute_max_abs_voltage_error(result: SimulationResult, dataset: TruthDataset) -> float:
    alignment = build_validation_alignment(result, dataset)
    errors = [
        abs(sim - truth)
        for sim, truth in zip(alignment.aligned_sim_voltage_v, alignment.truth_voltage_v)
    ]
    return max(errors, default=0.0)


def compute_final_soc_error(result: SimulationResult, dataset: TruthDataset) -> float:
    if not dataset.records or not result.time_series:
        return 0.0
    return abs(result.summary.final_soc_avg - dataset.records[-1].soc)


def compute_final_voltage_error(result: SimulationResult, dataset: TruthDataset) -> float:
    if not dataset.records or not result.time_series:
        return 0.0
    alignment = build_validation_alignment(result, dataset)
    if not alignment.aligned_sim_voltage_v:
        return 0.0
    return abs(alignment.aligned_sim_voltage_v[-1] - alignment.truth_voltage_v[-1])
