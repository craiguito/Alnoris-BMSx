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
