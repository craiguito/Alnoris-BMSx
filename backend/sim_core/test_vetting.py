from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from .calibration import load_truth_dataset
from .reference_cells import REFERENCE_CELLS
from .test_catalog import get_test_definition
from .tests_framework import TestVettingResult
from .types import SimulationConfig


def _parse_parameter(param_type: str, value: Any) -> Any:
    if param_type in {"string", "path"}:
        return str(value)
    if param_type == "int":
        return int(value)
    if param_type == "float":
        return float(value)
    if param_type == "bool":
        return bool(value)
    if param_type == "list_float":
        if isinstance(value, str):
            return [float(item.strip()) for item in value.split(",") if item.strip()]
        return [float(item) for item in value]
    if param_type == "json":
        if isinstance(value, str):
            return json.loads(value)
        return value
    return value


def vet_virtual_test(test_id: str, parameters: dict[str, Any], base_config: SimulationConfig) -> TestVettingResult:
    definition = get_test_definition(test_id)
    errors: list[str] = []
    warnings: list[str] = []
    normalized: dict[str, Any] = {}

    for parameter in definition.parameters:
        raw_value = parameters.get(parameter.key, parameter.default_value)
        if parameter.required and raw_value in (None, ""):
            errors.append(f"Missing required parameter: {parameter.label}")
            continue
        try:
            value = _parse_parameter(parameter.param_type, raw_value)
        except Exception as exc:
            errors.append(f"Could not parse {parameter.label}: {exc}")
            continue

        if parameter.allowed_values and value not in parameter.allowed_values:
            errors.append(f"{parameter.label} must be one of: {', '.join(parameter.allowed_values)}")
        if parameter.min_value is not None and value < parameter.min_value:
            errors.append(f"{parameter.label} must be >= {parameter.min_value}.")
        if parameter.max_value is not None and value > parameter.max_value:
            errors.append(f"{parameter.label} must be <= {parameter.max_value}.")
        normalized[parameter.key] = value

    current_like = normalized.get("current_a", normalized.get("pulse_current_a", normalized.get("preload_current_a")))
    recommended_current = max(
        (cell.recommended_discharge_current_a for cell in REFERENCE_CELLS.values() if abs(cell.cell_capacity_ah - base_config.cell_capacity_ah) < 0.5),
        default=None,
    )
    if recommended_current is not None and isinstance(current_like, (int, float)) and current_like > recommended_current * 2.5:
        warnings.append("Requested current is well above the recommended range of the closest reference cell.")

    time_step_s = float(normalized.get("time_step_s", base_config.time_step_s))
    if test_id in {"pulse_power", "ocv_relaxation"}:
        if test_id == "pulse_power" and time_step_s > max(normalized.get("pulse_duration_s", 1.0) / 5.0, 1.0):
            warnings.append("time_step_s is coarse relative to the pulse duration; voltage sag may be under-resolved.")
        if test_id == "ocv_relaxation" and time_step_s > max(normalized.get("preload_duration_s", 1.0) / 5.0, 1.0):
            warnings.append("time_step_s is coarse relative to the preload duration; transient relaxation detail may be limited.")

    if test_id == "constant_current_charge" and float(normalized.get("initial_soc", base_config.initial_soc)) > 0.95:
        warnings.append("Initial SOC is already high for a charge test; the useful charge window may be short.")
    if test_id == "thermal_stress" and float(normalized.get("duration_s", 0.0)) < 300.0:
        warnings.append("Thermal stress duration is short; heating trends may not fully emerge.")
    if test_id == "storage_self_discharge" and float(normalized.get("self_discharge_per_day", 0.0)) == 0.0:
        warnings.append("Self-discharge is zero; this storage test will mostly show calendar aging only.")
    if test_id == "balancing_effectiveness" and float(normalized.get("initial_soc_spread", 0.0)) <= 0.0:
        warnings.append("Initial SOC spread is zero, so balancing has little to correct.")
    if test_id == "fault_response" and not normalized.get("fault_specs"):
        errors.append("Fault Response Test requires at least one fault spec.")
    if test_id == "rate_capability" and not normalized.get("current_list_a"):
        errors.append("Rate Capability Test requires at least one current value.")
    if test_id == "model_validation":
        dataset_path = Path(str(normalized.get("dataset_path", ""))).expanduser()
        if not dataset_path.exists():
            errors.append("Model Validation Test requires a dataset_path that exists.")
        else:
            try:
                dataset = load_truth_dataset(str(dataset_path))
            except Exception as exc:
                errors.append(f"Could not load truth dataset: {exc}")
            else:
                normalized["dataset_path"] = str(dataset_path)
                normalized["dataset_record_count"] = len(dataset.records)
                metrics = normalized.get("metrics", [])
                if not isinstance(metrics, list):
                    errors.append("Metrics must be a JSON list.")
                else:
                    allowed_metrics = {"rmse_voltage", "energy_error", "temp_rmse"}
                    invalid_metrics = [metric for metric in metrics if metric not in allowed_metrics]
                    if invalid_metrics:
                        errors.append(
                            "Unsupported model validation metric(s): " + ", ".join(str(metric) for metric in invalid_metrics)
                        )
                    elif not metrics:
                        errors.append("Model Validation Test requires at least one metric.")
                dataset_chemistry = dataset.metadata.get("chemistry_name") or dataset.metadata.get("chemistry")
                if dataset_chemistry and str(dataset_chemistry) != base_config.chemistry_name:
                    warnings.append(
                        "Truth dataset chemistry metadata does not match the base configuration chemistry_name."
                    )

    return TestVettingResult(is_valid=not errors, errors=errors, warnings=warnings, normalized_parameters=normalized)
