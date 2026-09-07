---
schema: dia.module.v1
module_id: dia.entityspatialvisualdebugger
name: DiaEntitySpatialVisualDebugger
layer: domain/gameplay/tools
path: Dia/DiaEntitySpatialVisualDebugger
status: active
maturity: dev
parent_module_id: dia.entityspatial

summary: >
  IDebugDomain implementation for DiaEntitySpatial — grid, indexed-entity, and
  query-shape world-space overlays, built on the header-only Adaptors in
  DiaEntitySpatial/Adaptors.

dependencies:
  required:
    - dia.core
    - dia.entityspatial
    - dia.entity
  forbidden: []

public_api:
  headers:
    - DiaEntitySpatialVisualDebugger/EntitySpatialVisualDebugger.h
  namespaces:
    - Dia::EntitySpatial
  entry_points:
    - EntitySpatialVisualDebugger

non_responsibilities:
  - Query logic or spatial structure implementation (DiaEntitySpatial)
  - Overlay draw primitives (DiaEntitySpatial/Adaptors)
  - Any runtime behaviour in Release builds
---
