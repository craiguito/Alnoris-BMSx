from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

from backend.twincore.schemas.simulation import ResultPackage, RunManifest


@dataclass(frozen=True)
class SolverDescriptor:
    solver_id: str
    solver_version: str
    fidelity: str


class SolverPlugin(Protocol):
    solver_id: str
    solver_version: str
    fidelity: str

    def run(self, manifest: RunManifest) -> ResultPackage:
        ...
