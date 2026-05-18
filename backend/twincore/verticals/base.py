from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Protocol


_CAPABILITY_TYPES = {"template", "solver", "adapter", "report", "service", "validation_pack"}
_CAPABILITY_STATUSES = {"active", "planned", "stub", "legacy"}


@dataclass(frozen=True)
class VerticalDefinition:
    vertical_id: str
    display_name: str
    description: str
    domain: str
    package_name: str
    template_kinds: tuple[str, ...]
    component_classes: tuple[str, ...]
    scenario_types: tuple[str, ...]
    solver_ids: tuple[str, ...]
    report_types: tuple[str, ...]
    maturity: str
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = "twincore.vertical_definition.v1"


@dataclass(frozen=True)
class VerticalTemplateSummary:
    template_id: str
    display_name: str
    description: str
    component_classes: tuple[str, ...]
    default_solver_ids: tuple[str, ...]
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = "twincore.vertical_template_summary.v1"


@dataclass(frozen=True)
class VerticalCapability:
    capability_id: str
    display_name: str
    capability_type: str
    vertical_id: str
    status: str
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = "twincore.vertical_capability.v1"

    def __post_init__(self) -> None:
        if self.capability_type not in _CAPABILITY_TYPES:
            raise ValueError(f"Unsupported vertical capability type: {self.capability_type}")
        if self.status not in _CAPABILITY_STATUSES:
            raise ValueError(f"Unsupported vertical capability status: {self.status}")


class TemplateProvider(Protocol):
    vertical_id: str

    def list_templates(self) -> tuple[VerticalTemplateSummary, ...]:
        ...


class ScenarioFactory(Protocol):
    vertical_id: str

    def build_default_scenario(self, template_id: str) -> Any:
        ...


class SolverCatalog(Protocol):
    vertical_id: str

    def list_solver_ids(self) -> tuple[str, ...]:
        ...


class ReportPack(Protocol):
    vertical_id: str

    def list_report_types(self) -> tuple[str, ...]:
        ...


class ServiceBundle(Protocol):
    vertical_id: str

    def list_service_names(self) -> tuple[str, ...]:
        ...
