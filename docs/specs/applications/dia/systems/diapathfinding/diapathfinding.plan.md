# DiaPathfinding Implementation Plan

**Spec:** @docs/specs/applications/dia/systems/diapathfinding/diapathfinding.md
**Status:** Done

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `DiaPathfinding` module skeleton — directory, vcxproj, vcxproj.filters, module.md, Cluiche.sln entry | Build passes | Done | haiku | Static library targeting Debug/Release/Debug-Asan/Debug-Ubsan x64 |
| 2 | Implement `CellCoord`, `CPathGraph` concept, `IPathCostProvider`, `FlatCostProvider`, `PathResult` | Build passes | Done | sonnet | Core types; `ToWorldPositions` needs DiaMaths Vector2D |
| 3 | Implement `SquarePathGrid` (4/8-connected, per-cell passability) | Build + unit tests | Done | sonnet | Satisfies `CPathGraph`; neighbours must respect bounds + passability |
| 4 | Implement `HexPathGrid` (axial coordinates, 6-connected) | Build + unit tests | Done | sonnet | Satisfies `CPathGraph`; axial neighbour offsets + radius bounds check |
| 5 | Implement synchronous `FindPath<TGraph>()` — A* with fixed heuristics per graph type | Build + unit tests | Done | sonnet | Manhattan/Chebyshev for square, cube-distance for hex; internal STL priority_queue allowed |
| 6 | Implement async `PathfindingSystem<TGraph>` — `RequestPath`, `CancelRequest`, `Update(budgetMs)`, `IPathResultObserver` | Build + unit tests | Done | sonnet | Time-sliced; budget in ms; `DIA_LOG_INFO` on submit/found/failed |
| 7 | Implement `Testing/` utilities — `AssertPathFound`, `AssertPathCells`, `MockCostProvider` | Build passes | Done | sonnet | Ships with library; header-only; opt-in via include |
| 8 | Wire GoogleTests — create `DiaPathfinding/TestPathfinding.cpp`, register in GoogleTests.vcxproj | Tests pass | Done | sonnet | Cover all public API surface, grid types, heuristics, async system |
| 9 | Register `DiaPathfinding` in GoogleTests.vcxproj project references + AdditionalDependencies, and in Cluiche.sln | Build + tests pass | Done | haiku | Final wiring to make `dia run googletest` pick up DiaPathfinding tests |
