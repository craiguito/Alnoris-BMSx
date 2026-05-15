from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from backend.twincore.ids import new_id, require_valid_id

from .identity import IdentityRef
from .provenance import ProvenanceRecord, utc_now_iso
from .validation import CredibilityCard


@dataclass(frozen=True)
class SimulationArtifact:
    artifact_type: str
    name: str = ""
    payload: Any | None = None
    uri: str = ""
    media_type: str = "application/json"
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="simulation_artifact"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.simulation_artifact.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class RunManifest:
    scenario: Any
    solver_id: str = ""
    run_id: str = ""
    asset_graph: Any | None = None
    requested_at_iso: str = field(default_factory=utc_now_iso)
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.run_manifest.v1", init=False)

    def __post_init__(self) -> None:
        run_id = self.run_id or new_id("run")
        require_valid_id(run_id, "run_id")
        object.__setattr__(self, "run_id", run_id)


@dataclass(frozen=True)
class ResultPackage:
    solver_id: str
    solver_version: str
    run_id: str = ""
    provenance_id: str = ""
    credibility_card: CredibilityCard = field(default_factory=CredibilityCard)
    artifacts: tuple[SimulationArtifact, ...] = ()
    summary: dict[str, Any] = field(default_factory=dict)
    manifest: RunManifest | None = None
    provenance: ProvenanceRecord | None = None
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="twincore.result_package.v1", init=False)

    def __post_init__(self) -> None:
        run_id = self.run_id or new_id("run")
        require_valid_id(run_id, "run_id")
        object.__setattr__(self, "run_id", run_id)
        if self.provenance_id:
            require_valid_id(self.provenance_id, "provenance_id")

    def first_artifact(self, artifact_type: str) -> SimulationArtifact | None:
        return next((artifact for artifact in self.artifacts if artifact.artifact_type == artifact_type), None)
