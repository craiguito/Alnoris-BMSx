# BatteryTwin CLI Workflow

This workflow exercises the new TwinCore-backed BatteryTwin path.

1. List available presets.

   ```text
   python -m backend.batterytwin.cli list-presets
   ```

2. Initialize the local TwinCore database.

   ```text
   python -m backend.batterytwin.cli init-db --db data/twincore/twincore.sqlite
   ```

3. Create a project.

   ```text
   python -m backend.batterytwin.cli create-project --db data/twincore/twincore.sqlite --name "BatteryTwin Demo"
   ```

4. Create a preset asset graph.

   ```text
   python -m backend.batterytwin.cli create-preset-graph --db data/twincore/twincore.sqlite --project-id PROJECT_ID --preset-id generic_cylindrical_pack
   ```

5. Run the preset.

   ```text
   python -m backend.batterytwin.cli run-preset --db data/twincore/twincore.sqlite --project-id PROJECT_ID --preset-id generic_cylindrical_pack
   ```

6. Get the run record.

   ```text
   python -m backend.batterytwin.cli get-run --db data/twincore/twincore.sqlite --run-id RUN_ID
   ```

7. List project runs.

   ```text
   python -m backend.batterytwin.cli list-runs --db data/twincore/twincore.sqlite --project-id PROJECT_ID
   ```

Useful UI-ready projections:

```text
python -m backend.batterytwin.cli project-summary --db data/twincore/twincore.sqlite --project-id PROJECT_ID
python -m backend.batterytwin.cli asset-tree --db data/twincore/twincore.sqlite --project-id PROJECT_ID
python -m backend.batterytwin.cli inspect-asset --db data/twincore/twincore.sqlite --asset-id PACK_ASSET_ID
python -m backend.batterytwin.cli run-detail --db data/twincore/twincore.sqlite --run-id RUN_ID
```
