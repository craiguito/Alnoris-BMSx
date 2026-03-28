from __future__ import annotations

"""Virtual battery testbench structures.

These tests are engineering workflows built on top of the ECM simulator.
They are useful for early-stage design exploration and regression checks,
but they are not certification-grade standards implementations.
"""

from dataclasses import dataclass, field
from typing import Any

from .types import SimulationResult


@dataclass(frozen=True)
class TestParameterDefinition:
    key: str
    label: str
    param_type: str
    default_value: Any
    required: bool = True
    min_value: float | None = None
    max_value: float | None = None
    allowed_values: tuple[str, ...] = ()
    unit: str = ""
    tooltip: str = ""


@dataclass(frozen=True)
class VirtualTestDefinition:
    test_id: str
    display_name: str
    description: str
    category: str
    difficulty: str
    recommended_for_pack_level: bool
    supports_faults: bool = False
    supports_balancing: bool = False
    supports_thermal_zones: bool = False
    supports_profile_input: bool = False
    parameters: tuple[TestParameterDefinition, ...] = ()


@dataclass(frozen=True)
class TestVettingResult:
    is_valid: bool
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    normalized_parameters: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class VirtualTestScenarioResult:
    name: str
    summary_metrics: dict[str, Any]
    simulation_result: SimulationResult | None = None


@dataclass(frozen=True)
class VirtualTestResult:
    test_id: str
    test_name: str
    parameters_used: dict[str, Any]
    vetting_result: TestVettingResult
    summary_metrics: dict[str, Any]
    warnings: list[str] = field(default_factory=list)
    pass_fail_indicators: dict[str, bool] = field(default_factory=dict)
    primary_result: SimulationResult | None = None
    sub_results: list[VirtualTestScenarioResult] = field(default_factory=list)
    comparison_series: list[dict[str, Any]] = field(default_factory=list)
