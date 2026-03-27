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

## Remaining future work

- TODO: add undo/redo command stack
- TODO: add JSON save/load for CAD documents
- TODO: add move/rotate/edit operations on entities instead of regenerate-only layout updates
- TODO: add snapping, grid constraints, and object snap
- TODO: add richer picking such as ray/volume picking
- TODO: add OpenGL render backend implementing `IRenderBackend`
- TODO: add simulation overlay channels beyond temperature
- TODO: preserve entity identity across layout regeneration when topology changes are incremental
