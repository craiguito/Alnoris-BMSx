# Current Architecture

Alnoris is now organized as a backend-first TwinCore and BatteryTwin platform repository.

## Active Backend Architecture

The active Python backend is:

```text
backend/
  twincore/
  batterytwin/
  sim_core/
  tests/
```

`backend/twincore` and `backend/batterytwin` are the current architecture. `backend/sim_core` is retained as a legacy solver backend.

## TwinCore

`backend/twincore` is the shared platform foundation. It owns:

- stable identities and IDs
- generic asset graphs
- component, material, geometry, scenario, simulation, validation, provenance, and report schemas
- JSON serialization and stable hashing helpers
- SQLite storage and repositories
- solver interfaces and registry
- service-layer projections for UI-ready graph, run, scenario, report, component, and credibility views

TwinCore is domain-neutral. It should not encode battery-specific assumptions.

## BatteryTwin

`backend/batterytwin` is the first vertical built on TwinCore. It owns:

- battery pack, module, cell group, cell, busbar, cooling, and enclosure schemas
- electrical, thermal, degradation, and BMS state schemas
- battery scenarios and result schemas
- preset-to-asset-graph and preset-to-scenario templates
- adapters from BatteryTwin schemas to legacy `sim_core` configs
- `BatteryTwin.PackECM`, the current solver plugin
- BatteryTwin CLI commands and project summary services

## Legacy sim_core

`backend/sim_core` is the legacy Python battery simulation core. It is still required because `BatteryTwin.PackECM` wraps:

```text
backend.sim_core.engine.run_simulation
```

The legacy CLI remains available:

```text
python -m backend.sim_core.cli list-presets
```

New code should prefer TwinCore and BatteryTwin APIs. Legacy sim_core tests remain active as solver safety coverage.

## Archived Desktop

The former Qt/C++ desktop shell has been moved to:

```text
archive/desktop_cpp_legacy/
```

It is preserved for reference, but it is not the active architecture and is not part of the normal Python backend test suite.

## Active CLI Path

The current TwinCore/BatteryTwin workflow is:

```text
python -m backend.batterytwin.cli list-presets
python -m backend.batterytwin.cli init-db
python -m backend.batterytwin.cli create-project
python -m backend.batterytwin.cli create-preset-graph
python -m backend.batterytwin.cli run-preset
python -m backend.batterytwin.cli get-run
python -m backend.batterytwin.cli project-summary
python -m backend.batterytwin.cli graph
python -m backend.batterytwin.cli asset-tree
python -m backend.batterytwin.cli inspect-asset
python -m backend.batterytwin.cli reports
python -m backend.batterytwin.cli run-detail
```

BatteryTwin results include provenance, validation, reports, and credibility cards. Current solver fidelity is screening-level only.
