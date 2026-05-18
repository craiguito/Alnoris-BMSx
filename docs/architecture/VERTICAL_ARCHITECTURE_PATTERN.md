# TwinCore Vertical Architecture Pattern

TwinCore is the domain-neutral foundation for Alnoris digital twin products. A vertical product adds domain schemas, templates, adapters, solvers, services, reports, and validation packs while reusing the same TwinCore lifecycle.

## Shared Lifecycle

Every vertical should follow the same path:

```text
Template
  -> AssetGraph
  -> ComponentTwins and GeometryRefs
  -> Scenario
  -> RunManifest
  -> SolverPlugin
  -> ResultPackage
  -> Validation, Provenance, and Report records
```

BatteryTwin is the reference implementation of this lifecycle. New verticals should copy the pattern, not the battery-specific details.

## What A Vertical Provides

A future vertical package should provide:

- `vertical.py` with a `VerticalDefinition` and declared capabilities
- domain component schemas
- template-to-asset-graph builders
- scenario schemas and scenario factories
- solver plugins or adapters
- service projections for UI-ready JSON
- report summaries
- validation packs and credibility-card policies

The vertical registry is intentionally lightweight. It describes what exists or is planned, but it does not force deep inheritance.

## Credibility Rules

All verticals share the same credibility posture:

- never overclaim model validity
- show model class, model family, solver ID, solver version, validation tier, and uncertainty class
- show approved use range, assumptions, and limitations
- use clear warning language for screening models
- do not imply certification-grade verification unless the validation tier supports it

## Storage Rules

TwinCore persistence rules are shared across verticals:

- IDs should be project-scoped when records can be generated from templates
- asset edge IDs should be deterministic for semantic relationships
- `raw_json` stores the full dataclass payload
- normalized columns should exist only for common query fields
- vertical-specific detail belongs in metadata or vertical schemas

## Naming Rules

- TwinCore is the shared platform layer.
- BatteryTwin is the active product vertical.
- BMSx is the BMS/control/SOC/SOH module inside BatteryTwin.
- FusionTwin, FissionTwin, GridTwin, and AeroTwin are future planned verticals.

## Reference Implementation

BatteryTwin currently demonstrates:

- preset template generation
- project-scoped asset graph and component creation
- scenario conversion to the legacy solver backend
- `BatteryTwin.PackECM` solver plugin
- result packages with provenance, validation, and credibility cards
- report records and UI-ready service projections

Future vertical work should begin with definitions, testable contracts, and small adapters before adding heavy physics.
