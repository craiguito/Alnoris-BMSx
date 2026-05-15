from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class ValidationRecord:
    validation_type: str = "screening_regression"
    status: str = "regression_tested"
    run_id: str = ""
    target_id: str = ""
    validation_tier: str = "regression_tested"
    uncertainty_class: str = "engineering_screening"
    benchmark_refs: tuple[str, ...] = ()
    metrics: dict[str, Any] = field(default_factory=dict)
    approved_use_range: tuple[str, ...] = ()
    metric_results: dict[str, Any] = field(default_factory=dict)
    dataset_ids: tuple[str, ...] = ()
    notes: str = ""
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="validation_record"))
    schema_version: str = field(default="twincore.validation_record.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class CredibilityCard:
    credibility_level: str = "screening"
    model_class: str = "screening"
    model_family: str = ""
    solver_id: str = ""
    solver_version: str = ""
    validation_tier: str = "unvalidated"
    uncertainty_class: str = "engineering_screening"
    approved_use_range: tuple[str, ...] = ()
    validation_records: tuple[ValidationRecord, ...] = ()
    assumptions: tuple[str, ...] = ()
    limitations: tuple[str, ...] = ()
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="credibility_card"))
    schema_version: str = field(default="twincore.credibility_card.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
