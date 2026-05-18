# Alnoris BatteryTwin

Alnoris BatteryTwin is the battery vertical built on the Alnoris TwinCore platform foundation.

BMSx is now treated as the BMS, control, SOC, and SOH module inside BatteryTwin. The older `backend/sim_core` simulator remains in the repository because `BatteryTwin.PackECM` currently wraps it as the first legacy solver backend.

## Current Architecture

- `backend/twincore`: shared platform schemas, IDs, serialization, SQLite persistence, solver interfaces, validation, provenance, reporting, and UI-ready service projections.
- `backend/batterytwin`: battery-domain schemas, preset templates, scenario adapters, solver plugin, project summary services, and CLI workflow.
- `backend/sim_core`: legacy battery simulation core retained for compatibility with `BatteryTwin.PackECM` and `python -m backend.sim_core.cli`.
- `archive/desktop_cpp_legacy`: archived Qt/C++ desktop shell from the former BMSx desktop path. It is not part of the active backend architecture.

## Multi-Vertical TwinCore Roadmap

BatteryTwin is the active reference vertical for the TwinCore pattern. Future vertical definitions are registered for FusionTwin, FissionTwin, GridTwin, and AeroTwin so the architecture can be discussed and tested without creating placeholder physics packages.

View the registry:

```text
python -m backend.twincore.cli list-verticals
python -m backend.twincore.cli vertical --vertical-id batterytwin
python -m backend.twincore.cli capabilities
```

Do not build future vertical implementations until BatteryTwin service contracts and solver migration are stable.

## Quickstart

Run the backend tests:

```text
python -m pytest backend/tests -q
```

Check the legacy simulator CLI:

```text
python -m backend.sim_core.cli list-presets
```

Check the BatteryTwin CLI:

```text
python -m backend.batterytwin.cli list-presets
```

Create and run a BatteryTwin project:

```text
python -m backend.batterytwin.cli init-db --db data/twincore/twincore.sqlite
python -m backend.batterytwin.cli create-project --db data/twincore/twincore.sqlite --name "BatteryTwin Demo"
python -m backend.batterytwin.cli create-preset-graph --db data/twincore/twincore.sqlite --project-id PROJECT_ID --preset-id generic_cylindrical_pack
python -m backend.batterytwin.cli run-preset --db data/twincore/twincore.sqlite --project-id PROJECT_ID --preset-id generic_cylindrical_pack
python -m backend.batterytwin.cli get-run --db data/twincore/twincore.sqlite --run-id RUN_ID
```

UI-ready projection commands:

```text
python -m backend.batterytwin.cli project-summary --db data/twincore/twincore.sqlite --project-id PROJECT_ID
python -m backend.batterytwin.cli graph --db data/twincore/twincore.sqlite --project-id PROJECT_ID
python -m backend.batterytwin.cli asset-tree --db data/twincore/twincore.sqlite --project-id PROJECT_ID
python -m backend.batterytwin.cli inspect-asset --db data/twincore/twincore.sqlite --asset-id ASSET_ID
python -m backend.batterytwin.cli reports --db data/twincore/twincore.sqlite --project-id PROJECT_ID
python -m backend.batterytwin.cli run-detail --db data/twincore/twincore.sqlite --run-id RUN_ID
```

## Architecture Docs

- [Current Architecture](docs/architecture/ARCHITECTURE_CURRENT.md)
- [Target TwinCore Architecture](docs/architecture/ARCHITECTURE_TARGET_TWINCORE.md)
- [Vertical Architecture Pattern](docs/architecture/VERTICAL_ARCHITECTURE_PATTERN.md)
- [Future Verticals](docs/architecture/FUTURE_VERTICALS.md)
- [Migration Plan](docs/migration/MIGRATION_PLAN.md)
- [Codebase Cleanup Audit](docs/architecture/CODEBASE_CLEANUP_AUDIT.md)
- [BatteryTwin CLI Workflow](examples/batterytwin/cli_workflow.md)
- [Vertical Scaffold Example](examples/vertical_scaffold/README.md)

## Legacy Status

`backend/sim_core` is legacy but still required. It contains the working ECM, thermal, degradation, balancing, faults, validation, calibration, truth dataset, preset, and virtual test machinery used by `BatteryTwin.PackECM`.

The old Qt desktop shell has been archived under `archive/desktop_cpp_legacy`. New work should target `backend/twincore` and `backend/batterytwin`.

## Validation Posture

`BatteryTwin.PackECM` is screening-level engineering software. It is not certification-grade and is not electrochemical cell design software.

BatteryTwin result packages include provenance, validation records, reports, and credibility cards so downstream tools can show assumptions, limitations, and approved-use boundaries clearly.
