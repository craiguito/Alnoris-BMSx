from __future__ import annotations


class TwinCoreError(Exception):
    """Base exception for TwinCore platform errors."""


class StorageError(TwinCoreError):
    """Raised when local storage cannot be initialized or accessed."""


class RepositoryError(TwinCoreError):
    """Raised by repository classes when a persistence operation fails."""


class ScenarioError(TwinCoreError):
    """Raised when a scenario cannot be built or resolved."""


class SolverExecutionError(TwinCoreError):
    """Raised when a solver plugin cannot complete a run."""
