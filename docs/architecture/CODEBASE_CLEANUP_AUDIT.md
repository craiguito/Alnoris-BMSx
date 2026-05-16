# Codebase Cleanup Audit

This audit records the repository cleanup posture for the TwinCore and BatteryTwin architecture migration.

## Keep: New Architecture Folders

- `backend/twincore`
  - Shared platform foundation for identity, schemas, serialization, persistence, solver interfaces, validation, provenance, reporting, and UI-ready service projections.
- `backend/batterytwin`
  - Battery vertical layer for battery schemas, templates, adapters, solver plugins, project services, and CLI workflows.

## Keep: Legacy But Still Required

- `backend/sim_core`
  - Legacy battery simulation core.
  - Required because `BatteryTwin.PackECM` currently wraps `backend.sim_core.engine.run_simulation`.
  - Keep legacy safety tests until solver internals are migrated.

## Archive

- `archive/desktop_cpp_legacy`
  - Former `desktop_cpp` Qt/C++ desktop shell.
  - Archived because it is no longer part of the active TwinCore/BatteryTwin backend architecture.
  - Do not run archived desktop tests as part of the normal backend suite.

## Delete Generated Build Outputs

- `build-desktop`
- `build-desktop-test-artifacts`
- `build`
- `dist`
- CMake build folders such as `cmake-build-*`

## Delete Or Ignore Runtime / Generated Artifacts

- `data/twincore/*.sqlite`
- `data/twincore/*.sqlite-wal`
- `data/twincore/*.sqlite-shm`
- `Data/twincore/*.sqlite`
- `Data/twincore/*.sqlite-wal`
- `Data/twincore/*.sqlite-shm`
- `__pycache__`
- `.pytest_cache`
- `*.pyc`
- `*.log`
- temporary files and package artifacts

Tracked truth datasets under `Data/_extracted/` are not removed by this cleanup because they may support legacy validation workflows. They are excluded from clean source packages.

## Keep / Update Docs

- `README.md`
- `docs/architecture/ARCHITECTURE_CURRENT.md`
- `docs/architecture/ARCHITECTURE_TARGET_TWINCORE.md`
- `docs/migration/MIGRATION_PLAN.md`
- `docs/architecture/CODEBASE_CLEANUP_AUDIT.md`

## Keep Tests

- `backend/tests/test_sim_core_*.py`
  - Legacy solver safety coverage.
- `backend/tests/test_twincore_*.py`
  - TwinCore schema, storage, and service coverage.
- `backend/tests/test_batterytwin_*.py`
  - BatteryTwin templates, adapters, solver wrapper, CLI, and service coverage.

## Remove Or Update Later

- Desktop-only tests in the archived Qt shell.
  - They are preserved in `archive/desktop_cpp_legacy/tests` for reference but are not part of the active backend test suite.

## Migrate Later

- Move sim_core electrical, thermal, degradation, balancing, and fault logic into `backend/batterytwin/solvers/`.
- Move sim_core validation and calibration features into `backend/batterytwin/validation/`.
- Keep `backend.sim_core.cli` available until downstream compatibility consumers no longer need it.
