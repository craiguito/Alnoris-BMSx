from __future__ import annotations

from backend.twincore.schemas.simulation import ResultPackage, RunManifest

from .base import SolverPlugin


class SolverRegistry:
    def __init__(self) -> None:
        self._plugins: dict[str, SolverPlugin] = {}

    def register(self, plugin: SolverPlugin) -> SolverPlugin:
        self._plugins[plugin.solver_id] = plugin
        return plugin

    def get(self, solver_id: str) -> SolverPlugin:
        try:
            return self._plugins[solver_id]
        except KeyError as exc:
            raise KeyError(f"No TwinCore solver registered for {solver_id}.") from exc

    def run(self, manifest: RunManifest) -> ResultPackage:
        if not manifest.solver_id:
            raise ValueError("RunManifest.solver_id is required when running through SolverRegistry.")
        return self.get(manifest.solver_id).run(manifest)

    def solver_ids(self) -> tuple[str, ...]:
        return tuple(sorted(self._plugins))


DEFAULT_SOLVER_REGISTRY = SolverRegistry()
