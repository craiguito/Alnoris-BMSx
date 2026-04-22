from __future__ import annotations

import json
import os
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

from .calibration import apply_calibration_to_simulation_config, calibrate_parameters
from .chemistry import get_chemistry_preset
from .reference_cells import REFERENCE_CELLS
from .test_runner import run_virtual_test, virtual_test_result_to_dict
from .truth_data_manager import DEFAULT_TRUTH_DATA_DIR, TruthDatasetRegistry
from .types import ElectricalModelConfig, PhysicsConfig, SimulationConfig
from .validation import validate_simulation_config
from .validation_threshold_profiles import (
    ValidationThresholdProfile,
    get_validation_threshold_profile,
    validation_threshold_profile_to_dict,
)


DEFAULT_VALIDATION_MANIFEST_DIR = Path(__file__).resolve().parent / "validation_manifests"


@dataclass(frozen=True)
class ValidationPackManifest:
    manifest_id: str
    display_name: str
    description: str
    dataset_ids: tuple[str, ...]
    recommended_threshold_profile: str
    calibration_dataset_id: str
    source: str = ""
    status: str = "trusted"
    tags: tuple[str, ...] = ()
    temperature_band: str = ""
    validation_basis_label: str = ""
    validation_basis_description: str = ""
    validation_limits: tuple[str, ...] = ()


@dataclass(frozen=True)
class ValidationPackCaseResult:
    case_id: str
    case_display_name: str
    calibration_mode: str
    anchor_dataset_id: str
    base_config_summary: dict[str, Any]
    result_payload: dict[str, Any]


@dataclass(frozen=True)
class ValidationPackComparisonDatasetDelta:
    dataset_id: str
    dataset_display_name: str
    overall_status_default: str
    overall_status_calibrated: str
    metric_deltas: dict[str, float]


@dataclass(frozen=True)
class ValidationPackRunArtifact:
    manifest: ValidationPackManifest
    threshold_profile: ValidationThresholdProfile
    truth_data_dir: str
    default_model: ValidationPackCaseResult
    calibrated_model: ValidationPackCaseResult | None
    comparison_deltas: tuple[ValidationPackComparisonDatasetDelta, ...]
    comparison_markdown: str


def _normalize_manifest_dir(manifest_dir: str | Path | None) -> Path:
    return Path(manifest_dir or DEFAULT_VALIDATION_MANIFEST_DIR).expanduser().resolve()


def _manifest_error(path: Path, message: str) -> ValueError:
    return ValueError(f"Validation manifest '{path}': {message}")


def _load_manifest_from_path(path: Path) -> ValidationPackManifest:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        raise _manifest_error(path, f"could not read JSON: {exc}") from exc

    required = (
        "manifest_id",
        "display_name",
        "description",
        "dataset_ids",
        "recommended_threshold_profile",
        "calibration_dataset_id",
    )
    missing = [key for key in required if payload.get(key) in (None, "", [])]
    if missing:
        raise _manifest_error(path, f"missing required field(s): {', '.join(missing)}")

    dataset_ids = tuple(str(item).strip() for item in payload.get("dataset_ids", []) if str(item).strip())
    if not dataset_ids:
        raise _manifest_error(path, "dataset_ids must contain at least one dataset_id.")

    calibration_dataset_id = str(payload.get("calibration_dataset_id", "")).strip()
    if calibration_dataset_id not in dataset_ids:
        raise _manifest_error(path, "calibration_dataset_id must also appear in dataset_ids.")

    return ValidationPackManifest(
        manifest_id=str(payload["manifest_id"]).strip(),
        display_name=str(payload["display_name"]).strip(),
        description=str(payload["description"]).strip(),
        dataset_ids=dataset_ids,
        recommended_threshold_profile=str(payload["recommended_threshold_profile"]).strip(),
        calibration_dataset_id=calibration_dataset_id,
        source=str(payload.get("source", "")).strip(),
        status=str(payload.get("status", "trusted")).strip() or "trusted",
        tags=tuple(str(item).strip() for item in payload.get("tags", []) if str(item).strip()),
        temperature_band=str(payload.get("temperature_band", "")).strip(),
        validation_basis_label=str(payload.get("validation_basis_label", "")).strip(),
        validation_basis_description=str(payload.get("validation_basis_description", "")).strip(),
        validation_limits=tuple(str(item).strip() for item in payload.get("validation_limits", []) if str(item).strip()),
    )


def list_validation_manifests(manifest_dir: str | Path | None = None) -> list[ValidationPackManifest]:
    root = _normalize_manifest_dir(manifest_dir)
    if not root.exists():
        return []
    manifests = [_load_manifest_from_path(path) for path in sorted(root.rglob("*.json"))]
    return sorted(manifests, key=lambda item: item.display_name.lower())


def resolve_validation_manifest(
    manifest_id_or_path: str,
    *,
    manifest_dir: str | Path | None = None,
) -> ValidationPackManifest:
    raw_value = str(manifest_id_or_path).strip()
    if not raw_value:
        raise ValueError("Validation manifest input must be a manifest_id or JSON path.")

    candidate_path = Path(raw_value).expanduser()
    looks_like_path = candidate_path.suffix.lower() == ".json" or any(sep in raw_value for sep in ("/", "\\"))
    if candidate_path.exists() or looks_like_path:
        resolved_path = candidate_path.resolve()
        if not resolved_path.exists():
            raise ValueError(f"Validation manifest path does not exist: {resolved_path}")
        return _load_manifest_from_path(resolved_path)

    for manifest in list_validation_manifests(manifest_dir):
        if manifest.manifest_id == raw_value:
            return manifest

    available = ", ".join(item.manifest_id for item in list_validation_manifests(manifest_dir))
    raise ValueError(f"Unknown validation manifest '{raw_value}'. Available manifests: {available or 'none'}.")


def validation_manifest_to_dict(manifest: ValidationPackManifest) -> dict[str, Any]:
    return asdict(manifest)


def _default_reference_cell_key_for_chemistry(chemistry_name: str) -> str:
    return "a123_anr26650m1b" if chemistry_name == "lfp" else "panasonic_ncr18650b"


def _metadata_chemistry_name(metadata: dict[str, Any]) -> str:
    raw = str(metadata.get("chemistry_name", metadata.get("chemistry", ""))).strip().lower()
    return "lfp" if "lfp" in raw else "generic_liion"


def build_single_cell_validation_base_config(dataset_metadata: dict[str, Any]) -> SimulationConfig:
    chemistry_name = _metadata_chemistry_name(dataset_metadata)
    chemistry = get_chemistry_preset(chemistry_name)
    reference_cell = REFERENCE_CELLS[_default_reference_cell_key_for_chemistry(chemistry_name)]
    nominal_voltage_v = float(dataset_metadata.get("nominal_voltage_v", chemistry.cell_nominal_voltage))
    nominal_capacity_ah = float(dataset_metadata.get("nominal_capacity_ah", reference_cell.cell_capacity_ah))
    ambient_temp_c = float(dataset_metadata.get("ambient_temp_c", 25.0))

    return validate_simulation_config(
        SimulationConfig(
            cell_nominal_voltage=nominal_voltage_v,
            cell_full_voltage=chemistry.cell_full_voltage,
            cell_empty_voltage=chemistry.cell_empty_voltage,
            cell_cutoff_voltage=chemistry.cell_cutoff_voltage,
            cell_capacity_ah=nominal_capacity_ah,
            cells_in_series=1,
            cells_in_parallel=1,
            internal_resistance_ohm_per_cell=reference_cell.internal_resistance_ohm_per_cell,
            ambient_temp_c=ambient_temp_c,
            discharge_current_a=reference_cell.recommended_discharge_current_a,
            duration_s=1800,
            time_step_s=1,
            initial_soc=1.0,
            pack_mass_kg=reference_cell.pack_mass_kg,
            pack_heat_capacity_j_per_kgk=reference_cell.pack_heat_capacity_j_per_kgk,
            cooling_coeff_w_per_k=reference_cell.cooling_coeff_w_per_k,
            electrical_model=ElectricalModelConfig(
                model_type="rint",
                r0_ohm_per_cell=reference_cell.internal_resistance_ohm_per_cell,
            ),
            group_count=1,
            chemistry_name=chemistry.name,
            physics=PhysicsConfig(),
        )
    )


def _case_base_config_summary(config: SimulationConfig) -> dict[str, Any]:
    return {
        "chemistry_name": config.chemistry_name,
        "cell_nominal_voltage_v": config.cell_nominal_voltage,
        "cell_capacity_ah": config.cell_capacity_ah,
        "cells_in_series": config.cells_in_series,
        "cells_in_parallel": config.cells_in_parallel,
        "electrical_model_type": config.electrical_model.model_type,
        "ambient_temp_c": config.ambient_temp_c,
    }


def _run_manifest_case(
    manifest: ValidationPackManifest,
    profile: ValidationThresholdProfile,
    *,
    base_config: SimulationConfig,
    anchor_dataset_id: str,
    truth_data_dir: str,
) -> dict[str, Any]:
    parameters: dict[str, Any] = {
        "dataset_ids": list(manifest.dataset_ids),
        "metrics": list(profile.metrics),
    }
    parameters.update(profile.thresholds)
    previous_truth_dir = os.environ.get("ALNORIS_TRUTH_DATA_DIR")
    os.environ["ALNORIS_TRUTH_DATA_DIR"] = truth_data_dir
    try:
        result = run_virtual_test("model_validation", parameters, base_config)
    finally:
        if previous_truth_dir is None:
            os.environ.pop("ALNORIS_TRUTH_DATA_DIR", None)
        else:
            os.environ["ALNORIS_TRUTH_DATA_DIR"] = previous_truth_dir
    payload = virtual_test_result_to_dict(result)
    payload["validation_basis_label"] = manifest.validation_basis_label or profile.validation_basis_label
    payload["validation_basis_description"] = (
        manifest.validation_basis_description or profile.validation_basis_description
    )
    payload["validation_limits"] = list(manifest.validation_limits or profile.validation_limits)
    if "validation_summary" in payload:
        payload["validation_summary"]["validation_basis_label"] = payload["validation_basis_label"]
        payload["validation_summary"]["validation_basis_description"] = payload["validation_basis_description"]
        payload["validation_summary"]["validation_limits"] = payload["validation_limits"]
        payload["validation_summary"]["threshold_profile_id"] = profile.profile_id
        payload["validation_summary"]["validation_manifest_id"] = manifest.manifest_id
        payload["validation_summary"]["calibration_anchor_dataset_id"] = anchor_dataset_id
    return payload


def _metric_map(scorecard: dict[str, Any]) -> dict[str, float]:
    return {
        str(metric["metric_id"]): float(metric["value"])
        for metric in scorecard.get("metric_results", [])
        if metric.get("value") is not None
    }


def _build_comparison_deltas(
    default_scorecards: list[dict[str, Any]],
    calibrated_scorecards: list[dict[str, Any]],
) -> tuple[ValidationPackComparisonDatasetDelta, ...]:
    calibrated_by_id = {
        str(scorecard.get("dataset_id", "")): scorecard
        for scorecard in calibrated_scorecards
    }
    deltas: list[ValidationPackComparisonDatasetDelta] = []
    for default_scorecard in default_scorecards:
        dataset_id = str(default_scorecard.get("dataset_id", ""))
        calibrated_scorecard = calibrated_by_id.get(dataset_id)
        if calibrated_scorecard is None:
            continue
        default_metrics = _metric_map(default_scorecard)
        calibrated_metrics = _metric_map(calibrated_scorecard)
        metric_deltas = {
            metric_id: calibrated_metrics[metric_id] - default_metrics[metric_id]
            for metric_id in sorted(default_metrics)
            if metric_id in calibrated_metrics
        }
        deltas.append(
            ValidationPackComparisonDatasetDelta(
                dataset_id=dataset_id,
                dataset_display_name=str(default_scorecard.get("dataset_display_name", dataset_id)),
                overall_status_default=str(default_scorecard.get("overall_status", "")),
                overall_status_calibrated=str(calibrated_scorecard.get("overall_status", "")),
                metric_deltas=metric_deltas,
            )
        )
    return tuple(deltas)


def _build_comparison_markdown(
    manifest: ValidationPackManifest,
    profile: ValidationThresholdProfile,
    default_payload: dict[str, Any],
    calibrated_payload: dict[str, Any] | None,
    deltas: tuple[ValidationPackComparisonDatasetDelta, ...],
) -> str:
    lines = [
        f"# {manifest.display_name}",
        "",
        f"- Manifest: `{manifest.manifest_id}`",
        f"- Threshold profile: `{profile.profile_id}`",
        f"- Validation basis: {manifest.validation_basis_label or profile.validation_basis_label}",
        "",
        "## Default Model",
        "",
        f"- Overall status: {str(default_payload.get('validation_summary', {}).get('overall_status', 'n/a')).upper()}",
        f"- Datasets evaluated: {default_payload.get('validation_summary', {}).get('dataset_count', 0)}",
    ]
    if calibrated_payload is not None:
        lines.extend(
            [
                "",
                "## Calibrated Model",
                "",
                f"- Overall status: {str(calibrated_payload.get('validation_summary', {}).get('overall_status', 'n/a')).upper()}",
                f"- Datasets evaluated: {calibrated_payload.get('validation_summary', {}).get('dataset_count', 0)}",
            ]
        )
    if deltas:
        lines.extend(
            [
                "",
                "## Dataset Deltas",
                "",
                "| Dataset | Default | Calibrated | Metric delta summary |",
                "| --- | --- | --- | --- |",
            ]
        )
        for item in deltas:
            metric_summary = ", ".join(
                f"{metric_id}={delta:+.4f}" for metric_id, delta in item.metric_deltas.items()
            ) or "n/a"
            lines.append(
                f"| {item.dataset_display_name} | {item.overall_status_default.upper()} | "
                f"{item.overall_status_calibrated.upper()} | {metric_summary} |"
            )
    lines.extend(["", "## Validation Limits", ""])
    for item in manifest.validation_limits or profile.validation_limits:
        lines.append(f"- {item}")
    return "\n".join(lines)


def _pack_case_result(
    *,
    case_id: str,
    case_display_name: str,
    calibration_mode: str,
    anchor_dataset_id: str,
    base_config: SimulationConfig,
    payload: dict[str, Any],
) -> ValidationPackCaseResult:
    return ValidationPackCaseResult(
        case_id=case_id,
        case_display_name=case_display_name,
        calibration_mode=calibration_mode,
        anchor_dataset_id=anchor_dataset_id,
        base_config_summary=_case_base_config_summary(base_config),
        result_payload=payload,
    )


def run_validation_pack(
    manifest_id_or_path: str,
    *,
    threshold_profile_id: str | None = None,
    truth_data_dir: str | Path | None = None,
    manifest_dir: str | Path | None = None,
    include_calibrated: bool = True,
    rc_branch_count: int = 1,
) -> ValidationPackRunArtifact:
    manifest = resolve_validation_manifest(manifest_id_or_path, manifest_dir=manifest_dir)
    profile = get_validation_threshold_profile(threshold_profile_id or manifest.recommended_threshold_profile)
    registry = TruthDatasetRegistry(truth_data_dir or DEFAULT_TRUTH_DATA_DIR)
    anchor = registry.get_truth_dataset(manifest.calibration_dataset_id)
    base_config = build_single_cell_validation_base_config(anchor.metadata)

    default_payload = _run_manifest_case(
        manifest,
        profile,
        base_config=base_config,
        anchor_dataset_id=manifest.calibration_dataset_id,
        truth_data_dir=str(registry.truth_data_dir),
    )
    default_case = _pack_case_result(
        case_id="default_model",
        case_display_name="Default Model",
        calibration_mode="default",
        anchor_dataset_id=manifest.calibration_dataset_id,
        base_config=base_config,
        payload=default_payload,
    )

    calibrated_case: ValidationPackCaseResult | None = None
    comparison_deltas: tuple[ValidationPackComparisonDatasetDelta, ...] = ()
    calibrated_payload: dict[str, Any] | None = None
    if include_calibrated:
        calibrated_parameters = calibrate_parameters(anchor.dataset, rc_branch_count=rc_branch_count)
        calibrated_config = apply_calibration_to_simulation_config(base_config, calibrated_parameters)
        calibrated_payload = _run_manifest_case(
            manifest,
            profile,
            base_config=calibrated_config,
            anchor_dataset_id=manifest.calibration_dataset_id,
            truth_data_dir=str(registry.truth_data_dir),
        )
        calibrated_case = _pack_case_result(
            case_id="calibrated_model",
            case_display_name="Calibrated Model",
            calibration_mode="anchored_single_dataset",
            anchor_dataset_id=manifest.calibration_dataset_id,
            base_config=calibrated_config,
            payload=calibrated_payload,
        )
        comparison_deltas = _build_comparison_deltas(
            list(default_payload.get("validation_scorecards", [])),
            list(calibrated_payload.get("validation_scorecards", [])),
        )

    comparison_markdown = _build_comparison_markdown(
        manifest,
        profile,
        default_payload,
        calibrated_payload,
        comparison_deltas,
    )
    return ValidationPackRunArtifact(
        manifest=manifest,
        threshold_profile=profile,
        truth_data_dir=str(registry.truth_data_dir),
        default_model=default_case,
        calibrated_model=calibrated_case,
        comparison_deltas=comparison_deltas,
        comparison_markdown=comparison_markdown,
    )


def validation_pack_run_to_dict(artifact: ValidationPackRunArtifact) -> dict[str, Any]:
    return {
        "manifest": validation_manifest_to_dict(artifact.manifest),
        "threshold_profile": validation_threshold_profile_to_dict(artifact.threshold_profile),
        "truth_data_dir": artifact.truth_data_dir,
        "default_model": asdict(artifact.default_model),
        "calibrated_model": asdict(artifact.calibrated_model) if artifact.calibrated_model is not None else None,
        "comparison_deltas": [asdict(item) for item in artifact.comparison_deltas],
        "comparison_markdown": artifact.comparison_markdown,
    }
