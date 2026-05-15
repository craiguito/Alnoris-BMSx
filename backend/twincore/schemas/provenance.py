from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any

from .identity import IdentityRef


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


@dataclass(frozen=True)
class ProvenanceRecord:
    source: str
    generated_by: str = ""
    input_ids: tuple[str, ...] = ()
    created_at_iso: str = field(default_factory=utc_now_iso)
    metadata: dict[str, Any] = field(default_factory=dict)
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="provenance"))
    schema_version: str = field(default="twincore.provenance_record.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
