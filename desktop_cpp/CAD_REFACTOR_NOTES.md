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
