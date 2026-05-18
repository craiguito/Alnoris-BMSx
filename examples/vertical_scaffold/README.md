# Vertical Scaffold Example

This document shows the intended shape of a future TwinCore vertical. It is documentation only; no `backend/exampletwin` package is required today.

## Package Shape

```text
backend/exampletwin/
  __init__.py
  vertical.py
  schemas/
  templates/
  adapters/
  solvers/
  services/
  reports/
  cli.py
```

## Vertical Definition

```python
from backend.twincore.verticals import VerticalCapability, VerticalDefinition


def get_example_vertical_definition() -> VerticalDefinition:
    return VerticalDefinition(
        vertical_id="exampletwin",
        display_name="Alnoris ExampleTwin",
        description="Example vertical used to document the TwinCore pattern.",
        domain="example_domain",
        package_name="backend.exampletwin",
        template_kinds=("example_template",),
        component_classes=("example_component",),
        scenario_types=("ExampleScenario",),
        solver_ids=("ExampleTwin.ScreeningCore",),
        report_types=("example_screening_summary",),
        maturity="planned",
    )
```

## Template To Asset Graph

```python
def template_to_asset_graph(template, project_id: str):
    # Build project-scoped asset IDs.
    # Add deterministic contains edges.
    # Return a TwinCore AssetGraph.
    ...
```

## Scenario Adapter

```python
def scenario_to_solver_config(scenario):
    # Convert the vertical scenario into the solver's native input.
    # Keep the TwinCore scenario unchanged.
    ...
```

## Solver Plugin

```python
class ExampleSolverPlugin:
    solver_id = "ExampleTwin.ScreeningCore"
    solver_version = "0.1.0"
    fidelity = "screening"

    def run(self, manifest):
        # Convert manifest.scenario to solver input.
        # Run the solver.
        # Wrap results in ResultPackage with provenance and credibility.
        ...
```

## Result Wrapping

```python
def result_to_result_package(result, manifest):
    # Include summary metrics, artifacts, provenance, validation records,
    # and a credibility card that clearly states approved use and limits.
    ...
```

The important part is the lifecycle: template, asset graph, component twins, scenario, run manifest, solver plugin, result package, validation, provenance, and report.
