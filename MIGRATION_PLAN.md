# Migration Plan

1. Add schemas

   Add TwinCore and BatteryTwin dataclasses for identity, asset graphs, components, scenarios, manifests, results, validation, provenance, and reports.

2. Wrap legacy solver

   Add a BatteryTwin Pack ECM solver plugin that adapts BatteryTwin scenarios into existing `backend.sim_core` configurations and wraps native results in TwinCore result packages.

3. Add persistence

   Introduce storage for asset graphs, scenarios, manifests, result packages, validation records, provenance, and reports without changing the legacy CLI contract.

4. Convert presets to asset graph

   Represent existing battery presets as BatteryTwin components inside TwinCore asset graphs while keeping legacy preset aliases available.

5. Add component inspector

   Add UI and API surfaces for inspecting pack, module, cell group, cell, busbar, cooling, and enclosure records.

6. Add validation/provenance reports

   Generate report records from validation scorecards, truth datasets, solver metadata, assumptions, limitations, and provenance records.

7. Rename product layer from BMSx to BatteryTwin while keeping BMSx module

   Move product-facing naming toward BatteryTwin, but preserve BMSx compatibility modules and aliases until downstream users and the desktop bridge have migrated.
