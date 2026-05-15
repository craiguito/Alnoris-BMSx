# Current Architecture

Alnoris BMSx currently has two main runtime surfaces.

## Python Simulation Core

`backend/sim_core` is the working simulation engine. It models battery packs with a reduced-order ECM and includes:

- electrical pack and group behavior
- thermal behavior
- degradation estimates
- balancing and fault injection
- validation, scorecards, truth datasets, and virtual tests
- system presets and calibration utilities

The native Python API is centered on `SimulationConfig` and `SimulationResult` in `backend.sim_core.types`. `backend.sim_core.engine.run_simulation` takes a `SimulationConfig` and returns a `SimulationResult`.

## Desktop Bridge

`desktop_cpp` is the Qt/C++ desktop shell. It invokes:

```text
python -m backend.sim_core.cli
```

using `QProcess`, then exchanges JSON over stdin/stdout. The Python bridge in `backend.sim_core.bridge` converts JSON payloads into native sim_core dataclasses and serializes results back to the desktop-facing schema.

## Compatibility Constraint

The desktop bridge, CLI entry point, and sim_core dataclasses are the stable baseline. New architecture work must wrap this path instead of replacing it until the migration reaches a later phase.
