from __future__ import annotations

from dataclasses import is_dataclass
from typing import Any

from backend.twincore.serialization import dataclass_to_dict


def _as_dict(value: Any) -> dict[str, Any]:
    if value is None:
        return {}
    if is_dataclass(value):
        return dataclass_to_dict(value)
    if isinstance(value, dict):
        if "credibility_card" in value and isinstance(value["credibility_card"], dict):
            return dict(value["credibility_card"])
        if "raw" in value and isinstance(value["raw"], dict):
            return _as_dict(value["raw"])
        return dict(value)
    return {}


def _as_list(value: Any) -> list[Any]:
    if value is None:
        return []
    if isinstance(value, list):
        return value
    if isinstance(value, tuple):
        return list(value)
    return [value]


def credibility_card_to_summary(raw_or_card: Any) -> dict[str, Any]:
    raw = _as_dict(raw_or_card)
    model_class = str(raw.get("model_class") or raw.get("credibility_level") or "unknown")
    approved_use_range = _as_list(raw.get("approved_use_range"))
    assumptions = _as_list(raw.get("assumptions"))
    limitations = _as_list(raw.get("limitations"))
    validation_tier = str(raw.get("validation_tier") or "unvalidated")

    is_screening = model_class.lower() == "screening"
    not_certification_grade = any("not certification-grade" in str(item).lower() for item in approved_use_range)
    warning = "Not certification-grade" if not_certification_grade else ""

    return {
        "model_class": model_class,
        "model_family": raw.get("model_family", ""),
        "solver_id": raw.get("solver_id", ""),
        "solver_version": raw.get("solver_version", ""),
        "validation_tier": validation_tier,
        "uncertainty_class": raw.get("uncertainty_class", ""),
        "approved_use_range": approved_use_range,
        "assumptions": assumptions,
        "limitations": limitations,
        "display": {
            "badge": "Screening" if is_screening else model_class.replace("_", " ").title(),
            "color_hint": "amber" if is_screening else "neutral",
            "warning": warning,
        },
    }
