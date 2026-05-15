# Migration Plan

1. Add schemas

   Add TwinCore and BatteryTwin dataclasses for identity, asset graphs, components, scenarios, manifests, results, validation, provenance, and reports.

2. Wrap legacy solver

   Add a BatteryTwin Pack ECM solver plugin that adapts BatteryTwin scenarios into existing `backend.sim_core` configurations and wraps native results in TwinCore result packages.

3. Add persistence

   Introduce SQLite storage for projects, asset graphs, components, geometry references, scenarios, manifests, result packages, validation records, provenance, and reports without changing the legacy CLI contract.

4. Convert presets to asset graph

   Represent existing battery presets as BatteryTwin components inside TwinCore asset graphs while keeping legacy preset aliases available. Use representative cell-group nodes by default to avoid graph explosion.

5. Add component inspector

   Add UI and API surfaces for inspecting pack, module, cell group, cell, busbar, cooling, and enclosure records.

6. Add validation/provenance reports

   Generate report records from validation scorecards, truth datasets, solver metadata, assumptions, limitations, and provenance records.

7. Rename product layer from BMSx to BatteryTwin while keeping BMSx module

   Move product-facing naming toward BatteryTwin, but preserve BMSx compatibility modules and aliases until downstream users and the desktop bridge have migrated.

## Phase 2 Status

Phase 2 adds:

- local SQLite persistence at `data/twincore/twincore.sqlite`
- repositories for projects, asset graphs, components, scenarios, simulation runs, validation, provenance, and reports
- JSON serialization and stable hashing helpers
- BatteryTwin preset-to-asset-graph and preset-to-scenario conversion
- `python -m backend.batterytwin.cli` for the new TwinCore path
- persisted credibility, provenance, validation, artifacts, and simple screening report records for `run-preset`
- project-scoped preset asset/component/geometry IDs and deterministic semantic edge IDs
- idempotent preset graph upserts, with `run-preset` reusing existing graphs unless `--refresh-graph` is passed

The legacy `python -m backend.sim_core.cli` path remains unchanged and is still the desktop bridge path. BatteryTwin CLI runs are screening-level engineering studies, not certification-grade analyses.
