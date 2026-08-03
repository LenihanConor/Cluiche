---
schema: dia.module.v1
module_id: dia.flowfield
name: DiaFlowField
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaFlowField
language: cpp
parent_module_id: dia.root

summary: >
  Vector-field navigation for mass-unit movement — CFlowFieldGraph concept,
  sync ComputeFlowField<TGraph>() (Dijkstra sweep from goal), FlowField per-cell
  direction array, FlowFieldCache with named dirty-flag invalidation; square + hex support.

intent: >
  Provides O(grid_cells) shared-destination navigation for groups. One Dijkstra sweep
  from the goal cell computes a per-cell direction field; every unit reads its cell's
  direction vector each frame. Extends DiaPathfinding's graph types and cost interface
  rather than duplicating them.

responsibilities:
  - CFlowFieldGraph concept — extends CPathGraph with CellToWorldPosition + WorldToCell
  - FlowCell (direction Vector2, bool reachable) + FlowField (per-cell array, Sample, SampleWorld, IsComplete)
  - ComputeFlowField<TGraph>() — synchronous Dijkstra sweep from goal; returns complete FlowField
  - SquareFlowAdapter + HexFlowAdapter — thin wrappers adding world-space helpers to existing path grids
  - FlowFieldCache — named field store keyed by StringCRC; GetOrCompute/Invalidate/InvalidateAll/InvalidateRegion
  - DIA_LOG_INFO on compute start, completion (cells visited, ms elapsed), and invalidation
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Per-unit pathfinding (DiaPathfinding)
  - Local collision avoidance between units (DiaSteering / RVO)
  - Hierarchical flow fields for very large maps (deferred)
  - Terrain cost data storage (IPathCostProvider injected)
  - Path smoothing
  - Visual debugger overlay (future DiaFlowFieldVisualDebugger)
  - Thread safety within FlowFieldCache (single-threaded, called from SimPU)

dependent_modules:
  - dia.pathfinding

public_api:
  headers:
    - Dia/DiaFlowField/CFlowFieldGraph.h
    - Dia/DiaFlowField/FlowCell.h
    - Dia/DiaFlowField/FlowField.h
    - Dia/DiaFlowField/ComputeFlowField.h
    - Dia/DiaFlowField/SquareFlowAdapter.h
    - Dia/DiaFlowField/HexFlowAdapter.h
    - Dia/DiaFlowField/FlowFieldCache.h
  namespaces:
    - Dia::FlowField
  entry_points:
    - CFlowFieldGraph
    - FlowCell
    - FlowField
    - ComputeFlowField
    - SquareFlowAdapter
    - HexFlowAdapter
    - FlowFieldCache
    - FlowFieldKey

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.observation
    - dia.pathfinding
  forbidden:
    - dia.geometry2d
    - dia.blackboard
    - dia.command
    - dia.streams
    - dia.application
    - dia.graphics
---
