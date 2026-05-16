# backend.sim_core

`backend/sim_core` is the legacy battery simulation core.

It is still required because `BatteryTwin.PackECM` wraps `backend.sim_core.engine.run_simulation` while TwinCore and BatteryTwin mature around it.

The package contains:

- equivalent-circuit battery pack simulation
- thermal behavior
- degradation estimates
- balancing logic
- fault injection
- validation and scorecards
- calibration tools
- truth dataset handling
- battery system presets
- virtual tests

This package is not the new platform architecture. New code should prefer:

- `backend/twincore` for platform schemas, persistence, provenance, validation, reports, and services
- `backend/batterytwin` for battery-domain schemas, templates, adapters, solvers, and CLI workflows

Future migration target:

- electrical, thermal, and degradation logic moves into `backend/batterytwin/solvers/`
- validation and calibration move into `backend/batterytwin/validation/`
- presets move into `backend/batterytwin/templates/`
- `bridge` and `cli` remain only as legacy compatibility until replaced
