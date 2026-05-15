# Target TwinCore Architecture

The target architecture separates platform concepts from battery-domain concepts while preserving the existing simulator as the first solver backend.

## TwinCore

`backend/twincore` is the platform layer. It owns:

- stable identity references
- asset graph nodes and edges
- generic component, material, geometry, mesh, scenario, manifest, artifact, result, validation, provenance, and report records
- solver plugin interfaces and a registry

TwinCore should stay domain-neutral. It should know how to describe assets, runs, results, credibility, and provenance, but it should not encode battery-specific assumptions.

## BatteryTwin

`backend/batterytwin` is the battery product/domain layer. It owns:

- pack, module, cell group, cell, busbar, cooling channel, and enclosure schemas
- electrical, thermal, degradation, and BMS state records
- battery scenarios and battery simulation results
- adapters from BatteryTwin schemas to legacy sim_core configs
- solver plugins such as `BatteryTwin.PackECM`

## Phase 1 Runtime

In Phase 1, `BatteryPackECMSolverPlugin` is a compatibility wrapper:

1. Accept a `RunManifest` containing a `BatteryScenario`.
2. Convert the `BatteryScenario` to `backend.sim_core.types.SimulationConfig`.
3. Call `backend.sim_core.engine.run_simulation`.
4. Convert the native `SimulationResult` into a `BatterySimulationResult`.
5. Return a TwinCore `ResultPackage` with run id, solver identity, provenance, credibility card, and artifacts.

This keeps the existing simulator and desktop CLI intact while establishing the target package boundaries.

## Phase 2 Persistence

Phase 2 adds local SQLite persistence under `backend/twincore/storage`. The default database path is:

```text
data/twincore/twincore.sqlite
```

The schema stores projects, assets, asset edges, components, materials, geometry references, scenarios, simulation runs, artifacts, validation records, provenance records, and reports. Each table keeps lookup columns for common queries and preserves the full dataclass payload in `raw_json`.

## BatteryTwin Template Generation

Battery system presets from `backend.sim_core.system_presets` can now generate:

- a TwinCore `AssetGraph`
- pack/module/cell-group hierarchy nodes
- cooling, BMS, and enclosure assets
- component twins tied to asset ids
- generated parametric battery layout `GeometryRef` records
- default `BatteryScenario` objects that can still be converted into legacy `SimulationConfig`

Representative cell-group nodes are used by default. The converter does not create thousands of individual cell assets.

Generated IDs are scoped by project and preset, for example `batterytwin:{project_id}:{preset_id}:pack`. Asset edges use deterministic IDs derived from project, source asset, relationship, and target asset so graph upserts remain stable across repeated runs.

## BatteryTwin CLI

The new CLI is separate from the legacy desktop path:

```text
python -m backend.batterytwin.cli list-presets
python -m backend.batterytwin.cli init-db
python -m backend.batterytwin.cli create-project
python -m backend.batterytwin.cli create-preset-graph
python -m backend.batterytwin.cli run-preset
python -m backend.batterytwin.cli run-preset --refresh-graph
python -m backend.batterytwin.cli get-run
python -m backend.batterytwin.cli list-runs
```

`backend.sim_core.cli` remains preserved for the Qt desktop bridge. The BatteryTwin CLI is the new TwinCore path for local projects, asset graphs, scenarios, runs, provenance, validation records, and screening reports.
`create-preset-graph` is idempotent for a project and preset. `run-preset` reuses an existing graph by default and only regenerates/upserts graph records when `--refresh-graph` is provided.

Every `ResultPackage` includes provenance and a credibility card. The current model class is screening-level equivalent-circuit pack modeling with regression-test evidence only; it is not certification-grade and should not be presented as electrochemical cell design validation.
