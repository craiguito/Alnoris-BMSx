from __future__ import annotations

from backend.twincore.solvers import DEFAULT_SOLVER_REGISTRY

from .pack_ecm import BatteryPackECMSolverPlugin


PACK_ECM_SOLVER_PLUGIN = BatteryPackECMSolverPlugin()
DEFAULT_SOLVER_REGISTRY.register(PACK_ECM_SOLVER_PLUGIN)

__all__ = [
    "BatteryPackECMSolverPlugin",
    "PACK_ECM_SOLVER_PLUGIN",
]
