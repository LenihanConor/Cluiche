# System Spec: DiaEntitySpatialVisualDebugger

**Parent:** @docs/specs/applications/dia/dia.md  
**Status:** Done

---

## Summary

DiaEntitySpatialVisualDebugger makes the spatial index state of `DiaEntitySpatial` visible at runtime via the `DiaVisualDebuggerConsole`. It provides three independently-togglable `IVisualDebugger` implementations:

- **`EntitySpatialGridOverlay`** — draws the underlying grid cell structure (square or hex) as an outline overlay, making cell boundaries and spatial partitioning visible
- **`EntitySpatialEntityOverlay`** — draws each spatially-indexed entity as a circle (position + radius) colour-coded by layer mask bits, plus a text label showing the entity handle
- **`EntitySpatialQueryOverlay`** — draws the most recent query shape (circle, rect, ray, sector arc) and highlights which entities matched

All three classes are header-only template adaptors in `DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h`. `DiaEntitySpatial` has zero compile-time dependency on `DiaVisualDebugger`.

---

## Architecture

```
DiaEntitySpatial   EntitySpatialModule          owns ISpatialStructure<Entity>
                   SpatialComponent             position, radius, layerMask
                         │
                         │  adaptor header (no vcxproj dep)
                         ▼
DiaEntitySpatialVisualDebugger
                   EntitySpatialGridOverlay     IVisualDebugger — grid cell outlines
                   EntitySpatialEntityOverlay   IVisualDebugger — entity circles + labels
                   EntitySpatialQueryOverlay    IVisualDebugger — last query shape + hits
                         │
                         ▼
DiaVisualDebugger  IVisualDebugger  IDebugDraw
```

### Grid overlay

Topology is passed as a template parameter (`SquareGrid` or `HexGrid`). Cell enumeration uses `if constexpr` dispatch identical to the pattern in `DiaScalarFieldVisualDebugger`:

- **Square**: iterates `(x, y)` grid cells, draws each as a `RequestDrawRect` outline
- **Hex** (pointy-top axial): iterates `(q, r)` axial ranges, computes world-space centre via `world_x = size*(sqrt(3)*q + sqrt(3)/2*r)`, `world_y = size*(3/2*r)`, draws 6-edge polygon outline via 6 `RequestDraw(line)` calls

### Entity overlay

Iterates `EntitySpatialModule` by reading the `SpatialComponent` fields directly (position, radius, layerMask). For each entity with a valid spatial handle:
- Draws a `RequestDraw(circle)` outline; fill colour is transparent by default, outline colour selected from a configurable palette indexed by `layerMask & kMaxLayerColours` (using only bits 0–30, masking out bit 31)
- Optionally draws a `RequestDrawText` label at the entity position showing the entity's slot index

### Query overlay

The calling game/test code pushes a query descriptor to the overlay before issuing the query. The overlay retains the last N descriptors (configurable, default 1) and draws them on the next `Draw()` call:

- `Circle`: `RequestDraw(circle)` in query colour
- `Region` (AARect): `RequestDrawRect` outline in query colour
- `Ray`: `RequestDrawRay` in query colour
- `Sector`: `RequestDrawArc` for the arc edge + two `RequestDraw(line)` for the radius edges in query colour
- `KNearest`: `RequestDraw(circle)` at origin in query colour; no bound radius drawn (unbounded query)

Matched entities from the last query are highlighted: the entity overlay draws them with a distinct `hitColour` instead of their layer-palette colour when the query overlay is also enabled.

---

## Responsibilities

- Provide `EntitySpatialGridOverlay<Topology>` — grid cell outline rendering, square + hex via `if constexpr`
- Provide `EntitySpatialEntityOverlay` — per-entity circle + optional label, layer-palette colouring
- Provide `EntitySpatialQueryOverlay` — last-N query shape rendering + hit highlight integration
- All three derive from `Dia::Debug::IVisualDebugger` and respect `SetEnabled/IsEnabled`
- Register canonical layer name constants `entityspatial.grid`, `entityspatial.entities`, `entityspatial.query` in `DebugLayerNames.h` under an `EntitySpatial` section (priority tier 1 — entity/game data)
- Provide Google Tests coverage: construct, grid draws correct cell count, entity draws match SpatialComponent data, disabled draws nothing, query shapes draw correctly, hit highlights applied
- Provide `dia.diaentityspatialvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Spatial query logic — `DiaEntitySpatial`
- Entity picking or selection state — `DiaEntityInspector`
- Automatic registration into `DebugLayerManager` — caller registers
- Release build exclusion — header-only opt-in, no `#ifdef DIA_DEBUG` guard required

---

## Public Interface

```cpp
// DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h
// Header-only — no DiaEntitySpatialVisualDebugger.vcxproj.
// Consumers add DiaVisualDebugger to their own project dependencies.

namespace Dia::EntitySpatial::Adaptors {

    // ── Grid overlay ──────────────────────────────────────────────────────────

    template<CGridTopology Topology>
    class EntitySpatialGridOverlay : public Dia::Debug::IVisualDebugger {
    public:
        EntitySpatialGridOverlay(const EntitySpatialIndex& index,
                                  Dia::Core::StringCRC layerName,
                                  float cellWorldSize,
                                  Dia::Maths::Vector2D worldOrigin,
                                  Dia::Core::RGBA cellColour = RGBA(80, 80, 80, 120));

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
    };

    // ── Entity overlay ────────────────────────────────────────────────────────

    static constexpr int kMaxLayerColours = 8;

    struct EntityOverlayConfig {
        Dia::Core::RGBA layerColours[kMaxLayerColours] = { /* 8 distinct colours */ };
        Dia::Core::RGBA hitColour   = RGBA(255, 255,   0, 220); // yellow — query match
        bool            showLabels  = false;
        float           labelSize   = 12.0f;
    };

    class EntitySpatialEntityOverlay : public Dia::Debug::IVisualDebugger {
    public:
        EntitySpatialEntityOverlay(const EntitySpatialModule& module,
                                    Dia::Core::StringCRC layerName,
                                    EntityOverlayConfig config = {});

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;

        // Called by game/test code to mark a set of entities as query hits
        // for the next Draw() call. Cleared after Draw() completes.
        void SetQueryHits(const Dia::Core::DynamicArrayC<Dia::Entity::Entity, 64>& hits);
    };

    // ── Query overlay ─────────────────────────────────────────────────────────

    struct QueryDescriptor {
        enum class Shape { Circle, Region, Ray, Sector, KNearest };
        Shape shape;
        Dia::Maths::Vector2D origin;
        union {
            struct { float radius; }                            circle;
            struct { Dia::Maths::AARect rect; }                 region;
            struct { Dia::Maths::Vector2D dir; float maxDist; } ray;
            struct { Dia::Maths::Vector2D dir; float radius; float halfAngle; } sector;
            struct { int k; }                                   kNearest;
        };
    };

    class EntitySpatialQueryOverlay : public Dia::Debug::IVisualDebugger {
    public:
        explicit EntitySpatialQueryOverlay(Dia::Core::StringCRC layerName,
                                            Dia::Core::RGBA queryColour = RGBA(0, 255, 200, 180));

        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;

        // Push before issuing a query; retains up to kMaxRetained descriptors.
        static constexpr int kMaxRetained = 1;
        void PushQuery(const QueryDescriptor& desc);
    };

} // namespace Dia::EntitySpatial::Adaptors
```

### Layer name additions to `DebugLayerNames.h`

```cpp
// EntitySpatial (priority tier 1 — entity/game data)
inline const Dia::Core::StringCRC kEntitySpatialGrid     { "entityspatial.grid"     };
inline const Dia::Core::StringCRC kEntitySpatialEntities { "entityspatial.entities" };
inline const Dia::Core::StringCRC kEntitySpatialQuery    { "entityspatial.query"    };
```

---

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| EntitySpatialGridOverlay | Grid cell outline rendering. Square grid draws rect outlines; hex draws 6-edge polygon via line calls. `if constexpr` topology dispatch. | Draft |
| EntitySpatialEntityOverlay | Per-entity circle at `SpatialComponent.position` + `radius`. Outline colour from layer-palette (bits 0–30 of layerMask mod 8). Optional text label. Query hits highlighted with `hitColour`. | Draft |
| EntitySpatialQueryOverlay | Retains last `kMaxRetained` query descriptors. Draws circle / rect / ray / arc+lines / circle for the 5 query shapes. | Draft |
| DebugLayerNames entries | `entityspatial.grid`, `entityspatial.entities`, `entityspatial.query` constants in `DebugLayerNames.h`. | Draft |
| GoogleTests | Construct, grid cell count, entity circle matches SpatialComponent, disabled draws nothing, all 5 query shapes draw, hit highlight applied. | Draft |

---

## Binding Decisions

| Decision | How this feature complies |
|----------|--------------------------|
| PD-001 — StringCRC for IDs | Layer names are `StringCRC` constants in `DebugLayerNames.h` |
| PD-004 / AD-002 — No STL in public APIs | `SetQueryHits` takes `DynamicArrayC`; all public methods use DiaCore primitives |
| PD-007 — C++20 | `if constexpr` topology dispatch requires C++20 |
| AD-003 — Namespace `Dia::<Module>::` | All types in `Dia::EntitySpatial::Adaptors::` |

---

## Design Decisions

| # | Question | Decision |
|---|----------|----------|
| 1 | Header-only adaptor vs separate vcxproj | Header-only — `DiaEntitySpatial` must not carry a compile-time dep on `DiaVisualDebugger`. Mirrors `DiaScalarFieldVisualDebugger` SFVD-001 pattern. |
| 2 | Three separate classes vs one configurable class | Three independently-togglable classes — grid overlay is useful without entity data; entity overlay is useful without query replay. Matches DiaScalarFieldVisualDebugger SFVD-002 pattern. |
| 3 | Query descriptor push model vs polling EntitySpatialModule | Push model — the overlay doesn't need access to the module's internal resolve buffer. Game/test code calls `PushQuery` + `SetQueryHits` around its own query call, making hit highlight accurate without the overlay re-running the query. |
| 4 | Layer palette indexed by `layerMask & kMaxLayerColours` | Bits 0–30 encode game-defined layer categories; mapping to a small colour palette (8 slots) gives visually distinct colours across common use cases without requiring per-layer configuration. Bit 31 (dirty flag) is masked out. |
| 5 | `kMaxRetained = 1` default for query overlay | One retained query covers the overwhelming majority of debugging use cases (last call is what you're looking at). Increasing to N is a constructor parameter when needed. |
| 6 | `if constexpr` for hex cell polygon drawing | Hex cell outlines require a 6-edge polygon; square requires a rect. Runtime virtual dispatch for per-cell calls on large grids would be measurable. Mirrors SFVD-003 rationale. |

---

## Open Design Questions

1. **Hex cell outline rendering** — Drawing 6 `RequestDraw(line)` calls per hex cell is correct but verbose. `IDebugDraw` has no native hexagon primitive. Should the adaptor compute and cache the 6 vertex offsets once at construction, or recompute per cell? Recomputing is simpler and likely fast enough for debug use; cache if profiling shows otherwise. Resolve at implementation.

2. **Layer palette default colours** — The 8 default `layerColours` in `EntityOverlayConfig` are placeholder values. Finalise at implementation time to be visually distinct in the context of the existing debug colour vocabulary (avoid conflicting with `kEntityHighlight` yellow, `kPhysicsShapes` colours, etc.).
