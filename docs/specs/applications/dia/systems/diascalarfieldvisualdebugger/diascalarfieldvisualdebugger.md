# System Spec: DiaScalarFieldVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai, pathfinding, spatial, debug

## Purpose

DiaScalarFieldVisualDebugger makes scalar field state visible at runtime via the `DiaVisualDebuggerConsole`. It provides two `IVisualDebugger` implementations that can be independently toggled:

- **`ScalarFieldHeatmapOverlay`** — fills each cell with a colour interpolated from a configurable low/high colour map, showing field value intensity as a gradient
- **`ScalarFieldGradientOverlay`** — draws a direction arrow per cell showing the gradient vector, with an arrowhead triangle at the tip

Both overlay classes are header-only template adaptors in `DiaScalarField/Adaptors/ScalarFieldOverlay.h`. They support both `SquareFieldTopology` and `HexFieldTopology` via `if constexpr` dispatch. `DiaScalarField` has zero compile-time dependency on `DiaVisualDebugger`.

## Responsibilities

- Provide `ScalarFieldHeatmapOverlay<Topology, Policy>` — fills each cell as a filled rect with colour lerped from `lowColour` (at `minValue`) to `highColour` (at `maxValue`)
- Provide `ScalarFieldGradientOverlay<Topology, Policy>` — draws a `RequestDrawRay` shaft from cell centre in the gradient direction, plus a filled triangle arrowhead at the tip via `RequestDraw(p1, p2, p3, ...)`
- Both classes derive from `Dia::Debug::IVisualDebugger` and respect `SetEnabled/IsEnabled`
- Both enumerate cells via `if constexpr` topology dispatch — square grid iterates `(x, y)` loops; hex grid iterates axial `(q, r)` ranges
- Hex world-space cell centres computed via correct axial-to-world formula (pointy-top): `world_x = size * (sqrt(3)*q + sqrt(3)/2*r)`, `world_y = size * (3/2*r)`
- `ScalarFieldGradientOverlay` skips cells where gradient magnitude is below configurable `minMagnitude` threshold
- Register canonical layer name constants `scalarfield.heatmap` and `scalarfield.gradient` in `DebugLayerNames.h` under a `ScalarField` section (priority tier 0 — background/spatial data)
- Provide `GoogleTests` coverage: construct, all-cells-visited, high-value colour, disabled draws nothing, zero gradient skips, non-zero gradient draws ray
- Provide `dia.diascalarfieldvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Scalar field propagation logic — DiaScalarField
- Automatic registration into `DebugLayerManager` — caller registers
- Editor inspector panel (pause-and-scrub, numeric hover, history ring buffer) — DiaScalarFieldInspector
- Release build exclusion — callers choose whether to compile the adaptor header in; no `#ifdef DIA_DEBUG` guard required (overlay is lightweight and explicitly opt-in)

## Public Interfaces

```cpp
// DiaScalarField/Adaptors/ScalarFieldOverlay.h
// Header-only — no DiaScalarFieldVisualDebugger.vcxproj.
// Consumers add DiaVisualDebugger to their own project dependencies.

namespace Dia::ScalarField::Adaptors {

    struct OverlayColourMap {
        Dia::Core::RGBA lowColour  = RGBA(0,   0,   255, 200); // blue
        Dia::Core::RGBA highColour = RGBA(255, 0,   0,   200); // red
        float           minValue   = 0.0f;
        float           maxValue   = 1.0f;
    };

    template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
    class ScalarFieldHeatmapOverlay : public Dia::Debug::IVisualDebugger {
    public:
        ScalarFieldHeatmapOverlay(const DiaScalarField<Topology, Policy>& field,
                                   Dia::Core::StringCRC layerName,
                                   float cellWorldSize,
                                   Dia::Maths::Vector2D worldOrigin,
                                   OverlayColourMap colourMap = {});

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
        void DrawCell(Dia::Core::IDebugDraw& draw, CellIndex cell) const; // public helper
    };

    template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
    class ScalarFieldGradientOverlay : public Dia::Debug::IVisualDebugger {
    public:
        ScalarFieldGradientOverlay(const DiaScalarField<Topology, Policy>& field,
                                    Dia::Core::StringCRC layerName,
                                    float cellWorldSize,
                                    Dia::Maths::Vector2D worldOrigin,
                                    float arrowScale   = 0.35f,
                                    float minMagnitude = 0.01f,
                                    Dia::Core::RGBA arrowColour = RGBA(255, 255, 255, 180));

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
        void DrawCell(Dia::Core::IDebugDraw& draw, CellIndex cell) const; // public helper
    };

} // namespace Dia::ScalarField::Adaptors
```

### Layer name additions to `DebugLayerNames.h`

```cpp
// ScalarField (priority tier 0 — background/spatial data)
inline const Dia::Core::StringCRC kScalarFieldHeatmap  { "scalarfield.heatmap"  };
inline const Dia::Core::StringCRC kScalarFieldGradient { "scalarfield.gradient" };
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| ScalarFieldHeatmapOverlay | Colour-gradient rect fill per cell, lerped low→high by value. Square + hex. | inline | Done |
| ScalarFieldGradientOverlay | Direction arrow (ray + arrowhead triangle) per cell from gradient vector. Square + hex. | inline | Draft |
| DebugLayerNames entries | `scalarfield.heatmap` + `scalarfield.gradient` constants in DebugLayerNames.h. | inline | Done |
| GoogleTests | 7 tests covering construct, all-cells, colour, disabled, zero/non-zero gradient. | inline | Done |

## Dependencies on Other Systems

**Required (consumer must provide):**
- **DiaScalarField** — `DiaScalarField<T,P>`, `CellIndex`, topology types
- **DiaCore** — `IVisualDebugger`, `IDebugDraw`, `StringCRC`, `RGBA`
- **DiaMaths** — `Vector2D`

**Explicitly excluded from DiaScalarField.vcxproj:**
- **DiaVisualDebugger** — no hard dependency; adaptor is header-only opt-in

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SFVD-001 | Header-only adaptor, no separate vcxproj | DiaScalarField must not carry a compile-time dependency on DiaVisualDebugger. Header-only opt-in is the established pattern (SFD-009 for RulesPropagationPolicy). | Accepted | Yes |
| SFVD-002 | Two separate IVisualDebugger subclasses, not one | Each can be enabled/disabled independently. Heatmap alone is useful without arrows; arrows alone is useful for gradient-only inspection. Matches DiaHTNVisualDebugger pattern (SD-001). | Accepted | Yes |
| SFVD-003 | if constexpr topology dispatch, not virtual | Cell enumeration is called every frame per cell. Virtual dispatch here would add measurable overhead on large fields. Mirrors the SFD-001 rationale for DiaScalarField::Tick(). | Accepted | Yes |
| SFVD-004 | Arrowhead via RequestDraw triangle, not a new primitive | IDebugDraw already has a triangle primitive. Composing shaft + triangle gives a proper arrow without adding API surface. | Accepted | Yes |
| SFVD-005 | Hex world-space uses axial-to-world formula, not square-grid offset | Axial (q,r) coordinates map to world space via `world_x = size*(sqrt(3)*q + sqrt(3)/2*r)`, `world_y = size*(3/2*r)`. Using square-grid offset for hex produces incorrect cell positions. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Layer names are `StringCRC` constants in `DebugLayerNames.h` |
| PD-004 | Platform | No STL containers in public APIs | All public methods use DiaCore containers or primitives |
| PD-005 | Platform | x64 only | Header-only; consumers' vcxproj targets x64 |
| PD-007 | Platform | C++20 | `if constexpr` and concepts require C++20 |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::ScalarField::Adaptors::` namespace |

## Open Design Questions

1. **Hex heatmap cell shape** — The heatmap draws a square rect centred at the hex world position. This leaves visible gaps between hex cells in a tightly-packed grid. Should the heatmap draw 6 triangles to fill the hex shape, or is the square approximation acceptable for a debug tool? The gradient overlay is unaffected (it draws a ray, not a fill). Resolve at implementation — the axial-to-world centre position is correct either way.

2. **Arrowhead size parameters** — `arrowScale` controls the shaft length; arrowhead proportions (headLen, headWidth relative to shaft) are currently implementation constants. Should these be exposed on the constructor, or kept fixed? Fixed proportions (e.g. headLen = shaft * 0.3, headWidth = shaft * 0.2) are simpler and cover the expected debugging use case.

## DiaDebugDomain Migration

This spec describes the `IVisualDebugger` adaptor layer. The DiaDebugDomain migration wraps these two overlay classes in an `IDebugDomain` implementation and integrates them with `DiaDebugPanel`.

Migration scope (per `@docs/specs/applications/dia/systems/diadebugdomain/domain-migration.md`):
- **Group:** Spatial/Geometry — accent `DebugGroupAccents::kSpatial` (`#06b6d4`)
- **Drawers:** Heatmap, Gradient (maps to the two existing adaptor classes)
- **Module extraction:** move from `DiaScalarField/Adaptors/` into a standalone `DiaScalarFieldVisualDebugger/` module directory (resolves the `Adaptors/` isolation violation)
- **Contract:** all 16 ACs in `@docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md` must be satisfied by the `IDebugDomain` wrapper

## Status

**Status:** `Done`
