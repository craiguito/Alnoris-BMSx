# Alnoris BMSx

Alnoris BMSx is an early-stage battery simulation workspace with a Python simulation core and a Qt/C++ desktop shell.

The current production path is intentionally preserved:

- `backend/sim_core` contains the working battery pack ECM simulator.
- `desktop_cpp` launches the Python CLI with `python -m backend.sim_core.cli`.
- The desktop bridge communicates through JSON over stdin/stdout.

Phase 1 of the TwinCore refactor adds an additive architecture layer:

- `backend/twincore` provides identity, asset graph, scenario, validation, provenance, report, and solver package primitives.
- `backend/batterytwin` provides battery-domain twin schemas plus an adapter and solver plugin that wrap the existing `sim_core` engine.

Phase 2 adds the first local TwinCore persistence and BatteryTwin template path:

- `backend/twincore/storage` initializes and writes a local SQLite database at `data/twincore/twincore.sqlite` by default.
- Battery system presets can generate TwinCore asset graphs, component twins, geometry references, and runnable BatteryTwin scenarios.
- `backend.batterytwin.cli` runs presets through the new TwinCore/BatteryTwin path and persists scenarios, runs, artifacts, provenance, validation records, and simple screening reports.

Useful commands:

```text
python -m backend.batterytwin.cli list-presets
python -m backend.batterytwin.cli init-db
python -m backend.batterytwin.cli create-project --name "BatteryTwin Demo"
python -m backend.batterytwin.cli create-preset-graph --project-id PROJECT_ID --preset-id generic_cylindrical_pack
python -m backend.batterytwin.cli run-preset --project-id PROJECT_ID --preset-id generic_cylindrical_pack
python -m backend.batterytwin.cli get-run --run-id RUN_ID
python -m backend.batterytwin.cli list-runs --project-id PROJECT_ID
```

The legacy simulator and CLI remain the compatibility baseline while BatteryTwin grows around them.
The BatteryTwin solver wrapper is screening-level engineering software, not certification-grade validation or electrochemical cell design software.
