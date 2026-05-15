# Alnoris BMSx

Alnoris BMSx is an early-stage battery simulation workspace with a Python simulation core and a Qt/C++ desktop shell.

The current production path is intentionally preserved:

- `backend/sim_core` contains the working battery pack ECM simulator.
- `desktop_cpp` launches the Python CLI with `python -m backend.sim_core.cli`.
- The desktop bridge communicates through JSON over stdin/stdout.

Phase 1 of the TwinCore refactor adds an additive architecture layer:

- `backend/twincore` provides identity, asset graph, scenario, validation, provenance, report, and solver package primitives.
- `backend/batterytwin` provides battery-domain twin schemas plus an adapter and solver plugin that wrap the existing `sim_core` engine.

The legacy simulator and CLI remain the compatibility baseline while BatteryTwin grows around them.
