from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .identity import IdentityRef


@dataclass(frozen=True)
class ReportRecord:
    title: str
    report_type: str = "simulation"
    project_id: str = ""
    run_id: str = ""
    result_package_id: str = ""
    sections: tuple[dict[str, Any], ...] = ()
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="report"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.report_record.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id
