# Target TwinCore Architecture

TwinCore is the shared platform layer for digital twin products. BatteryTwin is the first vertical built on it.

## TwinCore Platform

`backend/twincore` should remain domain-neutral. It provides:

- identities and stable IDs
- asset graphs
- component and material records
- geometry and mesh references
- scenario and run manifests
- result packages and artifacts
- validation records and credibility cards
- provenance records
- report records
- persistence and repositories
- solver plugin interfaces
- service projections for UI and automation clients

TwinCore should be reusable by future verticals such as:

- BatteryTwin
- FusionTwin
- FissionTwin
- GridTwin
- AeroTwin

The reusable vertical pattern is formalized through `backend/twincore/verticals`.
BatteryTwin is registered as the active reference vertical; FusionTwin, FissionTwin, GridTwin, and AeroTwin are planned definitions only.

## BatteryTwin Vertical

`backend/batterytwin` owns the battery-specific layer:

- battery pack, module, cell group, cell, busbar, cooling channel, and enclosure schemas
- electrical, thermal, degradation, and BMS state records
- battery scenarios and battery simulation results
- preset templates and asset graph generation
- legacy sim_core adapters
- battery solver plugins
- BatteryTwin CLI and project summary services

BMSx is now treated as the BMS/control/SOC/SOH module inside BatteryTwin rather than the top-level product architecture.

## Vertical Registry

TwinCore exposes a lightweight registry for vertical definitions and capabilities:

```text
python -m backend.twincore.cli list-verticals
python -m backend.twincore.cli vertical --vertical-id batterytwin
python -m backend.twincore.cli capabilities
```

The registry is descriptive. It records active, planned, stub, and legacy capabilities without importing future vertical packages or claiming implementation status.

## Solver Architecture

The current solver is:

```text
BatteryTwin.PackECM
```

It wraps `backend.sim_core.engine.run_simulation` for compatibility. Future solvers should move implementation logic into `backend/batterytwin/solvers/` and use TwinCore result, provenance, and credibility contracts directly.

## Persistence And Services

TwinCore SQLite persistence stores:

- projects
- assets and asset edges
- components
- materials
- geometry references
- scenarios
- simulation runs and artifacts
- validation records
- provenance records
- reports

Service-layer projections provide JSON-ready contracts for:

- project summaries
- graph views
- asset trees
- component inspectors
- run history and run detail
- scenario summaries
- report summaries
- credibility summaries

These services are backend contracts for future desktop or web UI integration. They are not a web API yet.

## Validation Posture

BatteryTwin.PackECM is screening-level engineering software. It is not certification-grade and is not electrochemical cell design validation.

Credibility cards must preserve:

- model class
- model family
- solver ID and version
- validation tier
- uncertainty class
- approved use range
- assumptions
- limitations

## Migration Target

Longer-term migration should:

- move sim_core electrical, thermal, degradation, balancing, and fault logic into BatteryTwin solver modules
- move validation and calibration into BatteryTwin validation modules
- keep legacy CLI compatibility until downstream consumers migrate
- add UI integration after backend contracts stabilize
- keep TwinCore reusable by other vertical products
