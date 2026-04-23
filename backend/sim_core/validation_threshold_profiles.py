from __future__ import annotations

from dataclasses import asdict, dataclass
from typing import Any


@dataclass(frozen=True)
class ValidationThresholdProfile:
    profile_id: str
    display_name: str
    description: str
    metrics: tuple[str, ...]
    thresholds: dict[str, float]
    validation_basis_label: str
    validation_basis_description: str
    validation_limits: tuple[str, ...]


NASA_SINGLE_CELL_V1_PROFILE = ValidationThresholdProfile(
    profile_id="single_cell_v1_nasa",
    display_name="BMSx v1 NASA Single-Cell Validation",
    description=(
        "Product-facing single-cell validation thresholds for the initial BMSx v1 "
        "NASA Ames benchmark packs."
    ),
    metrics=(
        "rmse_voltage",
        "final_voltage_error",
        "energy_error",
        "temp_rmse",
    ),
    thresholds={
        "max_voltage_rmse_v": 0.050,
        "max_final_voltage_error_v": 0.080,
        "max_energy_error_fraction": 0.080,
        "max_temp_rmse_c": 2.50,
        "max_high_soc_voltage_rmse_v": 0.050,
        "max_mid_soc_voltage_rmse_v": 0.050,
        "max_low_soc_voltage_rmse_v": 0.060,
        "max_last_10_percent_voltage_rmse_v": 0.070,
        "max_cutoff_neighborhood_voltage_rmse_v": 0.080,
    },
    validation_basis_label="NASA Ames single-cell validation basis",
    validation_basis_description=(
        "Validation basis built from curated NASA Ames normalized single-cell discharge datasets."
    ),
    validation_limits=(
        "Validated at single-cell level only.",
        "Not a direct pack layout or module-level validation basis.",
        "Intended for comparative trade-study support only.",
    ),
)


_PROFILES: dict[str, ValidationThresholdProfile] = {
    NASA_SINGLE_CELL_V1_PROFILE.profile_id: NASA_SINGLE_CELL_V1_PROFILE,
}


def list_validation_threshold_profiles() -> list[ValidationThresholdProfile]:
    return list(_PROFILES.values())


def get_validation_threshold_profile(profile_id: str) -> ValidationThresholdProfile:
    key = str(profile_id).strip()
    try:
        return _PROFILES[key]
    except KeyError as exc:
        raise ValueError(
            f"Unknown validation threshold profile '{key}'. "
            f"Available profiles: {', '.join(sorted(_PROFILES)) or 'none'}."
        ) from exc


def validation_threshold_profile_to_dict(profile: ValidationThresholdProfile) -> dict[str, Any]:
    return asdict(profile)
