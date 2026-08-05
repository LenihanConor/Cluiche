---
module_id: dia.entityspatial
display_name: DiaEntitySpatial
parent_module: dia.entity
layer: domain-adapter
version: "1.0"
status: In Progress
dependencies:
  - dia.core
  - dia.maths
  - dia.geometry2d
  - dia.entity
  - dia.observation
public_api:
  headers:
    - DiaEntitySpatial/SpatialComponent.h
    - DiaEntitySpatial/EntitySpatialIndex.h
    - DiaEntitySpatial/EntitySpatialModule.h
  namespaces:
    - Dia::EntitySpatial
  entry_points:
    - EntitySpatialModule
    - SpatialComponent
    - EntitySpatialIndex
responsibilities:
  - Bridges ISpatialStructure<Entity> with the DiaEntity domain
  - Provides SpatialComponent for entity opt-in to spatial indexing
  - Provides EntitySpatialModule with dirty-flag frame sweep and five query shapes
  - Supports SquareGrid and HexGrid topologies
not_responsibilities:
  - Does not own the geometry math (ISpatialStructure implementations live in DiaGeometry2D)
  - Does not replicate component data inside the index
  - Does not support concurrent queries on the same module instance
---
# DiaEntitySpatial

Thin adapter bridging `DiaGeometry2D::ISpatialStructure<T>` with the `DiaEntity` domain.

Any entity with a `SpatialComponent` is spatially indexed and queryable by position, region, ray, or sector. The index is rebuilt incrementally via a dirty-flag sweep each frame.
