---
schema: dia.module.v1
module_id: dia.diascalarfieldvisualdebugger
name: DiaScalarFieldVisualDebugger
owner_team: TBD
layer: visual-debug
status: active
maturity: dev

path: Dia/DiaScalarField/Adaptors
language: cpp
parent_module_id: dia.scalarfield

summary: >
  Debug draw adaptors for DiaScalarField — heatmap colour fills and gradient arrows per cell.

intent: >
  Provides two independently-togglable IVisualDebugger implementations that expose scalar
  field state visually in the game world: a heatmap overlay that colour-lerps each cell
  from a low-value colour to a high-value colour, and a gradient overlay that renders a
  direction ray and arrowhead triangle at each cell's gradient vector.

responsibilities:
  - ScalarFieldHeatmapOverlay: fills each cell as a colour-lerped rect from lowColour (minValue) to highColour (maxValue)
  - ScalarFieldGradientOverlay: draws a ray and arrowhead triangle per cell showing the gradient direction; skips cells below minMagnitude threshold
  - Both overlays support Square and Hex topologies via if constexpr dispatch
  - Exposes OverlayColourMap config struct for heatmap colour and value range customisation
  - Adds two debug layer constants to DebugLayerNames: kScalarFieldHeatmap and kScalarFieldGradient (in DiaCore/DebugDraw/DebugLayerNames.h)

non_responsibilities:
  - Scalar field simulation or decay logic (lives in DiaScalarField)
  - DebugLayerManager registration or console integration
  - Release build exclusion (consumer's responsibility via preprocessor guards)
  - Persistent configuration storage
  - Interactive picking or selection

dependent_modules:
  - dia.core
  - dia.maths
  - dia.scalarfield
  - dia.visualdebugger

public_api:
  headers:
    - DiaScalarField/Adaptors/ScalarFieldOverlay.h
  namespaces:
    - Dia::ScalarField::Adaptors
  entry_points:
    - ScalarFieldHeatmapOverlay
    - ScalarFieldGradientOverlay
    - OverlayColourMap

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.scalarfield
  optional:
    - dia.visualdebugger
  forbidden: []

vcxproj: none
notes: >
  Header-only adaptor — no .vcxproj of its own. Consumers add DiaVisualDebugger to their
  project dependencies before including ScalarFieldOverlay.h. Intentionally optional to
  keep DiaScalarField free of debug draw coupling. DebugLayerNames additions live in
  DiaCore (not in this module's headers) per convention.
---

# DiaScalarFieldVisualDebugger

Header-only debug draw adaptors for `DiaScalarField`. Two independently-togglable overlays expose scalar field state: a heatmap colour fill per cell, and a gradient direction arrow per cell.

## Key Classes

- **ScalarFieldHeatmapOverlay\<Topology, Policy\>** — Fills each cell with a colour interpolated between `lowColour` (at `minValue`) and `highColour` (at `maxValue`). Configurable via `OverlayColourMap`.
- **ScalarFieldGradientOverlay\<Topology, Policy\>** — Draws a direction ray and arrowhead triangle per cell using the cell's gradient vector. Cells with gradient magnitude below `minMagnitude` are skipped.

Both classes dispatch topology via `if constexpr` for `SquareFieldTopology` and `HexFieldTopology`.

## Usage

```cpp
// Heatmap
Dia::ScalarField::Adaptors::OverlayColourMap colours;
colours.lowColour  = Dia::Core::RGBA(0,   0,   255, 200); // blue for 0.0
colours.highColour = Dia::Core::RGBA(255, 0,   0,   200); // red  for 1.0

Dia::ScalarField::Adaptors::ScalarFieldHeatmapOverlay<SquareFieldTopology, UniformDecayPolicy>
    heatmap(field, Dia::Debug::LayerNames::kScalarFieldHeatmap, 32.0f, worldOrigin, colours);

// Gradient arrows
Dia::ScalarField::Adaptors::ScalarFieldGradientOverlay<SquareFieldTopology, UniformDecayPolicy>
    arrows(field, Dia::Debug::LayerNames::kScalarFieldGradient, 32.0f, worldOrigin);

layerManager.Register(&heatmap);
layerManager.Register(&arrows);
```

## Layer Names

- `Dia::Debug::LayerNames::kScalarFieldHeatmap` — registered in `DiaCore/DebugDraw/DebugLayerNames.h`
- `Dia::Debug::LayerNames::kScalarFieldGradient` — registered in `DiaCore/DebugDraw/DebugLayerNames.h`
