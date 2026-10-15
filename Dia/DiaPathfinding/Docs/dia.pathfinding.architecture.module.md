---
schema: dia.module.v1
module_id: dia.pathfinding
name: DiaPathfinding
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaPathfinding
language: cpp
parent_module_id: dia.root

summary: >
  Grid A* pathfinding — CPathGraph concept (zero-overhead static polymorphism),
  SquarePathGrid + HexPathGrid (axial hexagon or rectangular flat-top odd-q offset) + HexTopology, injectable IPathCostProvider, sync FindPath<TGraph>(),
  async PathfindingSystem with time-sliced Update(budgetMs) and IPathResultObserver.

intent: >
  Provides a self-contained, graph-agnostic A* implementation for grid-based games.
  Graph topology is concept-constrained (zero vtable overhead on the hot path); cost
  injection is virtual (one call per edge — negligible). Ships both a synchronous
  single-query API and an asynchronous time-sliced queue for multi-unit use.

responsibilities:
  - CPathGraph concept — GetNeighbours + IsPassable requirement
  - SquarePathGrid — rectangular grid, 4/8-connected, per-cell passability (heap-allocated, no cap)
  - HexPathGrid — 6-connected hex grid with per-cell passability (heap-allocated for rectangular layout); two layouts (HexLayout): axial hexagon (radius) and rectangular flat-top odd-q offset (width, height)
  - HexTopology — pure value type for rectangular flat-top odd-q hex grids (odd columns shifted down; y-down world space; direction order NE,N,NW,SW,S,SE at 30deg+60deg*d CCW); NeighbourAt(cell, dir), kNoCell past the edge, kHexDirections (6), Distance, DirectionFromAngle, DirectionVector, CellCenter(coord,size), CellFromPoint(point,size)
  - Layout-aware hex heuristic in FindPath (axial vs offset)
  - IPathCostProvider virtual interface + FlatCostProvider (1.0f uniform default)
  - FindPath<TGraph>() synchronous A* returning PathResult
  - PathResult — success, totalCost, DynamicArrayC<CellCoord>, ToWorldPositions()
  - PathfindingSystem<TGraph> — async request queue, Update(budgetMs) time-slicing, IPathResultObserver result delivery
  - CancelRequest(PathRequestId) — drops pending work
  - PathfindingSystemBudgetAdapter<TGraph> — wraps PathfindingSystem::Update(ms) as ISimTimeBudgetedSystem (DiaSimTime, kBackground priority)
  - DIA_LOG_INFO on request submit, path found, path failed
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Terrain cost data storage (IPathCostProvider is injected)
  - Flow fields (Tier 3, future DiaFlowField)
  - Hierarchical pathfinding / HPA* (Tier 4)
  - Steering / local movement
  - NavMesh pathfinding
  - Path smoothing / string pulling
  - Thread safety within PathfindingSystem
  - Visual debugger overlay

dependent_modules:
  - dia.simtime

public_api:
  headers:
    - Dia/DiaPathfinding/CellCoord.h
    - Dia/DiaPathfinding/CPathGraph.h
    - Dia/DiaPathfinding/IPathCostProvider.h
    - Dia/DiaPathfinding/PathResult.h
    - Dia/DiaPathfinding/SquarePathGrid.h
    - Dia/DiaPathfinding/HexPathGrid.h
    - Dia/DiaPathfinding/HexTopology.h
    - Dia/DiaPathfinding/FindPath.h
    - Dia/DiaPathfinding/IPathResultObserver.h
    - Dia/DiaPathfinding/PathfindingSystem.h
    - Dia/DiaPathfinding/PathfindingSystemBudgetAdapter.h
  namespaces:
    - Dia::Pathfinding
  entry_points:
    - CellCoord
    - CPathGraph
    - IPathCostProvider
    - FlatCostProvider
    - PathResult
    - SquarePathGrid
    - HexPathGrid
    - HexLayout
    - HexTopology
    - kNoCell
    - kHexDirections
    - FindPath
    - PathfindingSystem
    - IPathResultObserver
    - PathfindingSystemBudgetAdapter

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.observation
    - dia.simtime
  forbidden:
    - dia.geometry2d
    - dia.blackboard
    - dia.command
    - dia.streams
    - dia.application
    - dia.graphics
---
