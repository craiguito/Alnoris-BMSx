# Migration Plan

## Phase 1: Schemas And Solver Wrapper Complete

Added:

- TwinCore identity, asset graph, component, scenario, simulation, validation, provenance, and report schemas
- BatteryTwin battery component, state, scenario, and result schemas
- `BatteryTwin.PackECM` solver plugin
- adapter from BatteryTwin scenarios to legacy `backend.sim_core.types.SimulationConfig`
- result wrapping into TwinCore `ResultPackage`

## Phase 2: Persistence And CLI Complete

Added:

- TwinCore SQLite persistence
- repositories for projects, graphs, components, geometry, scenarios, runs, validation, provenance, and reports
- BatteryTwin preset-to-asset-graph conversion
- BatteryTwin preset-to-scenario conversion
- BatteryTwin CLI
- run persistence from scenario to manifest to solver to result package to database records

## Phase 2.1: ID And Idempotency Hardening Complete

Added:

- project-scoped asset, component, and geometry IDs
- deterministic semantic edge IDs
- idempotent `create-preset-graph`
- graph reuse in `run-preset`
- `--refresh-graph`
- atomic preset graph save transaction
- enriched `get-run` output

## Phase 3: Services And UI-Ready Contracts Complete

Added:

- asset graph query service
- component inspector service
- run history service
- scenario summary service
- report summary service
- credibility projection
- BatteryTwin project summary service
- CLI commands exposing UI-ready JSON projections

## Phase 4: Web Or Desktop UI Integration

Next UI phase:

- choose desktop or web presentation layer
- consume TwinCore/BatteryTwin service projections
- add component inspector screens
- add graph and asset tree views
- add run history and report views
- keep backend CLI contracts stable

## Phase 5: Migrate sim_core Internals Into BatteryTwin

Future solver migration:

- move electrical, thermal, degradation, balancing, and fault logic into `backend/batterytwin/solvers/`
- move validation and calibration into `backend/batterytwin/validation/`
- move remaining preset logic into `backend/batterytwin/templates/`
- reduce `backend/sim_core` to compatibility shims, then retire it when downstream users are migrated

## Cleanup Status

The active architecture is backend-first TwinCore plus BatteryTwin. The old Qt desktop shell is archived under `archive/desktop_cpp_legacy` and is not part of the current backend architecture.
