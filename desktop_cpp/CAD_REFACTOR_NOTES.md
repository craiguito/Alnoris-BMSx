# CAD Refactor Notes

## What changed

The CAD subsystem was refactored from a single procedural preview module into a document-oriented battery CAD architecture.

The old prototype centered on one class that mixed:

- document state
- camera state
- scene generation
- frame preparation
- selection
- hit testing
- STL loading

The new architecture separates those responsibilities into standalone C++ areas under `desktop_cpp/cad/`.

## New structure

- `cad/core/`
  - `EntityId.h`: stable entity identifiers
  - `SelectionState.h`: selection by `EntityId`
  - `CadDocument.h/.cpp`: persistent battery CAD document owning explicit entities
- `cad/math/`
  - `CadMath.h`: shared vector/matrix math used by camera, render, and viewport projection
- `cad/camera/`
  - `Camera.h/.cpp`: orbit/zoom/view/projection logic
- `cad/battery/`
  - `BatteryConfig.h`: split layout/electrical/thermal config structs
  - `BatteryEntities.h`: explicit `CellEntity`, `BusbarEntity`, `CoolingPlateEntity`, `ModuleBoundaryEntity`, `PackEnclosureEntity`
  - `PackLayoutGenerator.h/.cpp`: generates a first document from battery layout config
  - `BatteryVisualizationBuilder.h/.cpp`: derives visualization overlay data separately from geometry ownership
- `cad/render/`
  - `RenderPacket.h`: disposable draw packet
  - `RenderComposer.h/.cpp`: composes draw-ready geometry from document + camera + overlay
  - `IRenderBackend.h`: scaffold for a future OpenGL backend
- `cad/picking/`
  - `HitTester.h/.cpp`: centralized screen-space hit testing returning `EntityId`
- `cad/io/`
  - `MeshLoader.h/.cpp`: STL loading abstraction
  - `JsonCadDocumentIO.h`: JSON serialization scaffold placeholder
- `cad/`
  - `CadEngine.h/.cpp`: thin coordinator that owns the document, camera, overlay, composer, and hit tester

## Current behavior preserved

- the CAD viewport still shows the battery pack preview
- cell selection still works
- orbit and zoom still work
- STL-based cell rendering still works
- the Qt host still renders the current packet with the existing temporary software path

## Important architectural improvements

- `CadDocument` is now the persistent source of truth
- CAD entities are explicit battery-native objects with stable IDs
- selection is entity-based rather than cell-index-based
- camera logic is isolated
- render output is a disposable packet derived from the document
- picking is centralized and returns `EntityId`
- layout generation is separated from render composition
- simulation-style temperature coloring is derived in an overlay layer instead of being stored as geometry ownership

## Incremental Editing Pass

This pass moves the CAD subsystem closer to a true editable battery document instead of a pure regenerate-only preview flow.

### Direct document editing support

- `CadDocument` now supports direct mutation APIs such as:
  - move entity
  - set entity position
  - remove entity
  - select / clear selection
  - set cell position and geometry
  - set busbar, cooling plate, module boundary, and enclosure geometry
  - rename entity labels
- `CadEngine` now exposes those mutations as engine-facing APIs and refreshes the derived visualization/render state after edits.

### Better entity lookup

- `CadDocument` now maintains a lightweight `EntityId -> EntityLocator` index.
- New APIs now support:
  - `hasEntity`
  - `entityKind`
  - `findCell`
  - `findBusbar`
  - `findCoolingPlate`
  - `findModuleBoundary`
  - `findEnclosure`
  - entity snapshot/restore for future undo and serialization work

### Property-editing foundation

- Explicit property-facing accessors now exist for:
  - cells
  - busbars
  - cooling plates
  - module boundaries
  - enclosures
- The engine can now expose selected-entity summaries and typed property snapshots without requiring the host UI to traverse raw containers directly.

### Identity-preserving regeneration

- Layout regeneration now preserves IDs for logically compatible generated entities where practical.
- Reused logical keys include:
  - cells by `series_index + parallel_index`
  - busbars by role
  - cooling plates by plate index
  - module boundaries by module index
  - enclosures by enclosure index
- Selection now persists across compatible regeneration if the selected entity still exists logically.

### Undo/redo scaffold

- A lightweight command layer now exists under `cad/commands/`:
  - `ICommand`
  - `CommandStack`
  - `MoveEntityCommand`
  - `RemoveEntityCommand`
  - `UpdateCellGeometryCommand`
- Full editor integration is still future work, but the engine now has a clean command-execution seam.

### Broader picking

- Render packets now include broader pickable shapes instead of only cell circles.
- Coarse rectangle-based picking is now prepared for:
  - busbars
  - cooling plates
  - module boundaries
  - enclosures
- This remains a screen-space approximation and is intentionally structured so future ray/AABB or GPU picking can replace it cleanly.

## Override-Aware Generated Entities Pass

This pass makes compatible layout regeneration preserve manual user edits instead of only preserving IDs and labels.

### Override-aware fields

- Battery entities now track explicit property ownership modes:
  - position
  - geometry
  - label
  - visibility
- Each of those fields can now be either:
  - `Generated`
  - `UserOverride`

### Regeneration now preserves manual edits

- Compatible regeneration still rebuilds generated layout entities from battery config, but now merges them against existing document entities.
- If a field is still in `Generated` mode, regeneration recomputes it.
- If a field is in `UserOverride` mode, regeneration preserves the user-edited value.
- This now applies to:
  - cell position and geometry
  - busbar position and size
  - cooling plate position and size
  - module boundary position and size
  - enclosure position and geometry
  - labels and visibility

### Typed property update APIs

- The engine/document layer now exposes explicit typed property updates such as:
  - `CellPropertiesUpdate`
  - `BusbarPropertiesUpdate`
  - `CoolingPlatePropertiesUpdate`
  - `ModuleBoundaryPropertiesUpdate`
  - `PackEnclosurePropertiesUpdate`
- Applying one of these updates marks the edited fields as `UserOverride`.
- Typed full-property application helpers also exist so undo/redo can restore complete entity state cleanly.

### First command-driven property editing path

- User-style edits can now go through commands instead of only direct mutation APIs.
- Added command-backed operations for:
  - move entity
  - rename entity
  - update cell properties
  - update busbar properties
  - remove entity
- The older low-level mutation methods remain available internally for engine/document plumbing.

### Reset-to-generated foundation

- The engine/document now has a first reset seam for generated fields:
  - reset position to generated
  - reset geometry to generated
  - reset label to generated
- These currently trigger a regeneration pass so generated defaults are reapplied while preserving other overrides.

## Remaining future work

- TODO: add JSON save/load for CAD documents
- TODO: add full UI integration for the command stack
- TODO: add richer property editing and document tools on top of the new mutation APIs
- TODO: add UI actions for resetting overridden fields back to generated defaults
- TODO: persist generated-vs-overridden state in future JSON save/load
- TODO: add snapping, grid constraints, and object snap
- TODO: add richer picking such as ray/volume picking
- TODO: add OpenGL render backend implementing `IRenderBackend`
- TODO: bind simulation overlays more directly by entity identity
- TODO: add dirty-flag optimization to avoid rebuilding visualization/render packets more often than necessary
- TODO: add constraints/dimensions if a future pass needs engineering-style editing workflows

## Command-Driven Property Editing Pass

This pass makes command-backed editing the default user-facing path for the main supported CAD edits and adds the first real end-to-end property editing workflow in the Qt host.

### Editing responsibilities moved out of `CadEngine`

- A focused `cad/edit/CadEditService` now owns the document mutation helpers used by the engine for:
  - typed property updates
  - label and visibility changes
  - reset-to-generated operations
- `CadEngine` remains the main facade used by the host, but it now delegates more of the document-edit behavior instead of continuing to absorb every mutation path directly.

### Command-backed editing is now the default user path

- User-facing engine entry points now go through commands for the main supported operations:
  - move entity
  - rename entity
  - set visibility
  - update cell properties
  - update busbar properties
  - update cooling plate properties
  - update module boundary properties
  - update enclosure properties
  - reset position to generated
  - reset geometry to generated
  - reset label to generated
- Low-level direct mutation methods still exist underneath as internal building blocks for command execution and document plumbing.

### Smarter reset-to-generated behavior

- Reset operations no longer rely on a blunt rebuild-only path for the supported entity types.
- Position, geometry, and label resets now use targeted edit-service helpers that restore generated defaults for:
  - cells
  - busbars
  - cooling plates
  - module boundaries
  - pack enclosures
- After those targeted resets, the engine refreshes visualization/render state instead of forcing a broader topology rebuild.
- Future work may still deepen this with finer dirty-flag routing and explicit topology-vs-geometry refresh distinctions.

### First real property-editing UI workflow

- The Qt host now includes a minimal CAD properties panel.
- A selected entity can now be:
  - inspected
  - renamed
  - hidden/shown
  - edited through typed fields
  - reset back to generated defaults
  - undone/redone through the command stack
- The current workflow supports practical editing for:
  - cells
  - busbars
  - cooling plates
  - module boundaries
  - pack enclosures

### Improved selection prioritization

- Pickable render data now carries a selection priority.
- Hit testing now prefers smaller, more directly editable entities over large wrapper entities when candidates overlap.
- Current priority order is roughly:
  - cell
  - busbar
  - cooling plate
  - module boundary
  - pack enclosure

### Remaining gaps after this pass

- TODO: save/load overridden/generated property state in future JSON IO
- TODO: add snapping, alignment tools, and object snap
- TODO: add richer picking such as ray/AABB or GPU-based picking
- TODO: add dirty-flag optimization for targeted document/overlay/render refresh
- TODO: add OpenGL backend integration behind the existing render packet seam
- TODO: add simulation overlay editing/binding by entity identity
- TODO: add more advanced editing tools and gizmos
- TODO: add multi-select and marquee selection

## UI Cleanup And Selection Feedback Pass

This pass focuses on the Qt desktop shell around the CAD engine so the viewport feels like the primary workspace and the inspector behaves more reliably during real use.

### Main UI layout improvements

- The main window now follows a clearer engineering-tool hierarchy:
  - left setup sidebar
  - dominant center CAD workspace
  - right sidebar tabs for inspector and results
  - smaller bottom logs/output area
- A lightweight toolbar was added for the most common actions:
  - load
  - save
  - CAD undo / redo
  - run
  - baseline
  - compare

### Simulation inputs are grouped more cleanly

- The left sidebar is now broken into clearer grouped sections:
  - cell
  - pack layout
  - electrical / thermal
  - simulation run
  - actions
- This reduces the earlier long flat wall of controls and makes scanning the setup workflow easier.

### CAD inspector usability improvements

- The CAD properties area now reads more like a real inspector:
  - selection summary
  - transform
  - geometry
  - actions
- Entity-specific geometry fields are still shown only when relevant.

### Property interaction fixes

- The property panel now suppresses stale mid-apply refresh churn while commands are executing.
- Apply now avoids firing redundant edits when values have not actually changed.
- Apply also now routes a given entity edit through a single typed update command instead of splitting one user action into separate rename / visibility / geometry commands.
- Undo, redo, and reset actions now refresh the inspector more predictably after the command stack updates the document.
- Clicking empty space in the viewport now clears the current selection instead of leaving stale inspector state behind.

### Stronger selected-entity feedback

- The viewport now draws a strong blue selection overlay using projected wireframe geometry for the selected entity.
- The current software-rendered path uses a blue wireframe/outline overlay first, with the screen-space halo only as secondary reinforcement.
- This works across the currently supported selectable entity types through the shared pickable data path.

### Remaining UI polish ideas

- TODO: add dockable panels
- TODO: persist panel/splitter layout state
- TODO: polish dark/light theme variants further
- TODO: add richer CAD toolbars and view controls
- TODO: add direct manipulation gizmos
- TODO: add multi-select inspector support
- TODO: add a more advanced simulation dashboard layout

## CAD-To-Simulation Thermal Coupling Pass

This pass adds the first practical bridge from the battery CAD document into the backend thermal model.

### Thermal zones in the backend

- The Python backend now accepts:
  - `thermal_zones`
  - `group_zone_assignments`
  - `group_labels`
  - `group_entity_ids`
- Each simulation group can now inherit:
  - a zone-specific cooling coefficient
  - a zone-specific ambient temperature override
- The thermal model remains lumped per group, but it is now ready to reflect simple CAD-informed cooling differences.

### Desktop-side mapping

- The desktop host now builds a lightweight simulation mapping from the CAD document before sending a simulation request.
- `SimulationMappingBuilder` derives:
  - `group_count`
  - stable per-group labels
  - stable group entity IDs
  - thermal zone assignments
  - thermal zone definitions

### Current CAD heuristics

- Cells are grouped by series index into simulation groups.
- Cells near visible cooling plates are assigned to cooling-derived thermal zones.
- Cells that are not mapped to a cooling plate but fall inside a visible module boundary inherit a module-derived thermal zone.
- If no more specific mapping exists, groups fall back to thermal zone `0`.

### Current limitations

- This is a deterministic approximation, not CFD or full conductive thermal coupling.
- Cooling-plate and module-zone influence is currently inferred from simple spatial heuristics.
- There is not yet a dedicated UI for editing thermal zones directly.

### Future work

- TODO: expose thermal zone editing in the desktop inspector
- TODO: bind backend per-group thermal results back onto CAD cells and modules
- TODO: persist thermal-zone metadata in CAD document save/load
- TODO: add richer geometric mapping beyond simple proximity/containment rules

## Desktop Simulation Visualization Pass

This pass makes simulation results visible and inspectable directly inside the Qt desktop application instead of only surfacing a few raw charts from JSON.

### Typed desktop result model

- The desktop host now parses backend payloads into a dedicated typed result model before updating the UI.
- The typed model keeps:
  - pack-level time series
  - per-group SOC / temperature / voltage arrays
  - summary metrics
  - group labels and entity IDs when available
- Parsing remains defensive so older or partially populated payloads fail gracefully instead of crashing the UI.

### Results-driven desktop workflow

- A successful run now updates one shared results state inside the main window.
- That state drives:
  - summary text
  - charts
  - group inspection table
  - CAD overlay coloring
  - time scrub playback
- The goal is to keep the charts, table, and viewport synchronized around the same selected timestep.

### Chart coverage

- The desktop results area now includes chart views for:
  - pack voltage
  - current
  - pack power
  - average SOC
  - SOC envelope
  - average / maximum temperature
- Selected-group detail charts were also added for:
  - voltage
  - temperature
  - SOC
- A lightweight vertical scrub marker now shows the active timestep across the charts.

### Group inspection

- The results panel now includes a compact per-group table showing the latest timestep values for:
  - group label
  - SOC
  - voltage
  - temperature
- The weakest and hottest groups are visually marked to help users spot limiting groups quickly.

### CAD overlay mapping

- The CAD viewport can now receive a disposable simulation overlay without mutating the underlying CAD document.
- Supported overlay metrics are:
  - temperature
  - SOC
  - voltage
- Current v1 mapping assumes backend group index corresponds to CAD cell `series_index`, and applies the group metric to every CAD cell in that series group.
- This is intentionally simple and deterministic so it can be replaced later by stronger traceability through stable entity IDs.

### Current limitations

- The overlay uses group-to-cell series-index mapping rather than a richer entity-ID overlay binding.
- The charts and group table are intentionally compact and not yet a full simulation dashboard.
- Comparison mode is still chart-focused; it does not yet provide a full side-by-side group overlay workflow.

### Future work

- TODO: add legends for simulation overlay scales
- TODO: drive overlays directly from backend `group_entity_ids` when available
- TODO: add playback controls beyond manual scrubbing
- TODO: add richer comparison views for per-group behavior
- TODO: add export / copy support for the group inspection table

## Canonical Result Contract And Nonlinear Inspection Pass

This pass tightens the backend-to-desktop result contract and exposes more of the nonlinear backend state inside the desktop UI.

### Canonical result naming

- The backend now serializes one canonical result shape with explicit unit suffixes such as:
  - `pack_voltage_v`
  - `pack_temp_avg_c`
  - `group_voltage_v`
  - `group_core_temp_c`
  - `group_surface_temp_c`
  - `group_effective_resistance_ohm`
- Legacy aliases are still emitted temporarily so older desktop consumers do not break while the UI migrates.

### Desktop model parity

- The desktop result model now parses and exposes:
  - group core and surface temperatures
  - group heat
  - group hysteresis voltage
  - group diffusion stress
  - group effective resistance
  - chemistry name
  - enabled nonlinear feature flags
  - structured warnings
- Parsing stays defensive so missing optional fields fall back cleanly.

### Richer inspection views

- Results charts now include:
  - pack voltage
  - current
  - pack power
  - SOC average and envelope
  - pack temperature summary
  - max core temperature
  - max surface temperature
- Selected-group charts now include:
  - voltage
  - SOC
  - core temperature
  - surface temperature
  - diffusion stress
  - hysteresis voltage
- The group inspection area now shows the selected group’s:
  - SOC
  - voltage
  - core temperature
  - surface temperature
  - diffusion stress
  - hysteresis
  - effective resistance
  - zone and entity traceability

### Overlay upgrades

- CAD overlays now support:
  - core temperature
  - surface temperature
  - SOC
  - group voltage
  - diffusion stress
  - effective resistance
- The selected timestep drives overlay values, charts, and group detail together.

### Virtual test UX improvements

- Virtual test results now show:
  - vetted parameters used
  - vetting warnings
  - summary metrics
  - pass / flagged checks
  - a simple comparison table for multi-scenario tests
- Result export now writes the active canonical JSON payload to disk.

### Remaining follow-up

- TODO: retire legacy result aliases after the desktop fully migrates
- TODO: use backend `group_entity_ids` everywhere overlays bind to CAD groups
- TODO: add richer comparison charts for multi-scenario tests
- TODO: add CSV export for comparison tables

## Battery System Preset Pass

This pass adds full battery-system presets rather than only cell presets or demo startup content.

### Preset architecture

- System presets now live in a typed backend registry instead of being hardcoded inside desktop UI code.
- Each preset can define:
  - chemistry
  - reference cell
  - module count
  - pack topology
  - CAD layout defaults
  - cooling / thermal assumptions
  - balancing defaults
  - operating limits
  - recommended virtual tests

### Generative startup workflow

- The desktop now queries the preset catalog from the backend.
- A selected preset populates simulation defaults and drives battery-aware CAD generation.
- Default startup now prefers a generated preset scene rather than sample mesh content.

### CAD layout expansion

- The battery CAD generator now supports:
  - multiple modules
  - cylindrical cells
  - prismatic cells
  - pouch-style cells
  - preset-driven busbar thickness, cooling channel thickness, enclosure wall thickness, and module gap

### Current limitations

- Preset thermal-zone multipliers still flow most strongly through generated CAD cooling/module heuristics rather than a full explicit zone editor.
- The desktop still exposes only a compact subset of preset parameters for direct manual editing after load.
