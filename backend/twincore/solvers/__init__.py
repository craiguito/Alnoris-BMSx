from __future__ import annotations

from .base import SolverDescriptor, SolverPlugin
from .registry import DEFAULT_SOLVER_REGISTRY, SolverRegistry

__all__ = [
    "DEFAULT_SOLVER_REGISTRY",
    "SolverDescriptor",
    "SolverPlugin",
    "SolverRegistry",
]
