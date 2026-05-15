from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class ValidationRecord:
    validation_type: str
    status: str
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
    validation_records: tuple[ValidationRecord, ...] = ()
    assumptions: tuple[str, ...] = ()
    limitations: tuple[str, ...] = ()
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="credibility_card"))
    schema_version: str = field(default="twincore.credibility_card.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
