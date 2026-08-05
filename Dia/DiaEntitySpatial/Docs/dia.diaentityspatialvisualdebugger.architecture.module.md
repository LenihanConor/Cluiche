---
schema: dia.module.v1
module_id: dia.diaentityspatialvisualdebugger
name: DiaEntitySpatialVisualDebugger
owner_team: TBD
layer: debug-adaptor
status: active
maturity: dev

path: Dia/DiaEntitySpatial/Adaptors
language: cpp
parent_module_id: dia.entityspatial

summary: >
  Debug draw adaptors for DiaEntitySpatial — grid cells, indexed entities, and query shapes.

intent: >
  Provides three independently-togglable IVisualDebugger implementations that expose
  spatial indexing state visually in the game world: grid cell outlines (square or hex),
  spatially-indexed entities as coloured circles, and the most recent query shape
  (circle, region, ray, sector, or k-nearest).

responsibilities:
  - EntitySpatialGridOverlay: draws grid cell outlines (square rects or hex 6-edge polygons) with runtime topology dispatch
  - EntitySpatialEntityOverlay: draws alive entities with SpatialComponent as circles, colour-coded by layer mask, with query hit highlighting
  - EntitySpatialQueryOverlay: draws the last-N pushed query shape (circle/rect/ray/arc/k-nearest marker)
  - Adds three debug layer constants to DebugLayerNames (kEntitySpatialGrid, kEntitySpatialEntities, kEntitySpatialQuery)
  - 15 Google Tests validating overlay state transitions and draw calls

non_responsibilities:
  - Query logic or spatial structure implementation (lives in EntitySpatialModule)
  - Entity selection or picking (no interactive features)
  - DebugLayerManager registration or console integration
  - Release build exclusion (consumer's responsibility via preprocessor guards)
  - Persistent configuration storage

dependent_modules:
  - dia.core
  - dia.maths
  - dia.geometry2d
  - dia.entity
  - dia.entityspatial
  - dia.diavisualdebugger

public_api:
  headers:
    - DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h
  namespaces:
    - Dia::EntitySpatial::Adaptors
  entry_points:
    - EntitySpatialGridOverlay
    - EntitySpatialEntityOverlay
    - EntitySpatialQueryOverlay
    - EntityOverlayConfig
    - QueryDescriptor

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.geometry2d
    - dia.entity
    - dia.entityspatial
    - dia.diavisualdebugger
  forbidden: []

vcxproj: none
notes: >
  Header-only adaptor — no .vcxproj of its own. Consumers add DiaVisualDebugger to their
  project dependencies before including EntitySpatialOverlay.h. Intentionally optional
  to keep DiaEntitySpatial free of debug draw coupling. DebugLayerNames additions live in
  DiaCore (not in this module's headers) per convention.
---

# DiaEntitySpatialVisualDebugger

Header-only debug draw adaptors for `DiaEntitySpatial`. Three independently-togglable overlays expose spatial indexing state: grid topology, indexed entities, and query shapes.

## Key Classes

- **EntitySpatialGridOverlay** — Renders grid cell outlines. Stores topology at construction (square or hex) and dispatches draw calls accordingly.
- **EntitySpatialEntityOverlay** — Renders entities with `SpatialComponent` as circles, colour-coded by layer mask (modulo 8). Accepts `SetQueryHits()` to highlight query results in yellow.
- **EntitySpatialQueryOverlay** — Renders the most recent query shape: circle, axis-aligned region, ray, sector (arc + radii), or k-nearest marker.

## Usage

```cpp
// Square grid
Dia::EntitySpatial::Adaptors::EntitySpatialGridOverlay gridOverlay(squareDef, kEntitySpatialGrid);
debugDraw.Register(gridOverlay);

// Entities
Dia::EntitySpatial::Adaptors::EntitySpatialEntityOverlay entityOverlay(module, domain, kEntitySpatialEntities);
debugDraw.Register(entityOverlay);

// Queries
Dia::EntitySpatial::Adaptors::EntitySpatialQueryOverlay queryOverlay(kEntitySpatialQuery);
debugDraw.Register(queryOverlay);

// Push a query result
QueryDescriptor desc{ QueryDescriptor::Shape::Circle, origin, radius };
queryOverlay.PushQuery(desc);
entityOverlay.SetQueryHits(hitEntities);
```

## Testing

15 Google Tests in `Cluiche/Tests/GoogleTests/DiaEntitySpatial/TestEntitySpatialVisualDebugger.cpp` validate:
- Grid overlay draw calls for square and hex topologies
- Entity overlay colour mapping and hit highlighting
- Query overlay descriptor rendering for all 5 shapes
- Layer enable/disable state transitions
