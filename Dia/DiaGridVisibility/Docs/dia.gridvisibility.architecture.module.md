---
schema: dia.module.v1
module_id: dia.gridvisibility
name: DiaGridVisibility
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaGridVisibility
language: cpp
parent_module_id: dia.root

summary: >
  Per-cell visibility system — maintains Unexplored/Revealed/Visible state per VisibilityGroupId on a coarser grid derived from the pathfinding graph; recursive-octant shadowcasting against impassable cells; dirty-flag updates; CanSee/GetCellState/GetVisibleEntities queries; IVisibilityChangeObserver callbacks.

intent: >
  Answers three per-tick questions game and AI code routinely ask: CanSee(entity, group), GetCellState(cell, group), GetVisibleEntities(group). Reuses the DiaPathfinding grid — no new grid abstraction. Shadowcasting O(visible cells) per dirty source; standing-still sources cost zero. Observer pattern for cell and entity visibility changes.

responsibilities:
  - VisibilityGroupId (StringCRC alias) and VisibilityState enum (Unexplored/Revealed/Visible)
  - GridVisibilitySystem<TGraph> — observer registration, per-group state grid, chunk-grid consolidation at construction
  - Update pass: dirty-flag shadowcasting sweep (recursive octant), Visible→Revealed decay, visible-entity list rebuild via DiaEntitySpatial
  - CanSee, GetCellState, GetVisibleEntities queries (O(1))
  - IVisibilityChangeObserver — OnCellStateChanged + OnEntityVisibilityChanged callbacks during Update
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Fog-of-war render data / per-cell opacity
  - Per-entity cone perception (DiaSensor)
  - DiaStreams-based events (uses Observer pattern instead)
  - Thread-safe concurrent mutation (single-threaded, SimPU)
  - Visual debugger overlay (DiaGridVisibilityVisualDebugger)
  - Minimap data layer
  - AI decision logic

dependent_modules:
  - dia.pathfinding
  - dia.entityspatial

public_api:
  headers:
    - Dia/DiaGridVisibility/VisibilityGroupId.h
    - Dia/DiaGridVisibility/VisibilityState.h
    - Dia/DiaGridVisibility/VisibilityLogChannel.h
    - Dia/DiaGridVisibility/IVisibilityChangeObserver.h
    - Dia/DiaGridVisibility/GridVisibilitySystem.h
    - Dia/DiaGridVisibility/Testing/VisibilityTestHelpers.h
  namespaces:
    - Dia::GridVisibility
  entry_points:
    - GridVisibilitySystem
    - VisibilityGroupId
    - VisibilityState
    - IVisibilityChangeObserver
    - kLogChannel

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.pathfinding
    - dia.entityspatial
    - dia.entity
    - dia.observation
  forbidden:
    - dia.streams
    - dia.geometry2d
    - dia.applicationflow
    - dia.sensor
    - dia.blackboard
    - dia.graphics
---
