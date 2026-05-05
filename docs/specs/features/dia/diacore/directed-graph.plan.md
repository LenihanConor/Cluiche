# Plan: DirectedGraph Container

**Spec:** @docs/specs/features/dia/diacore/directed-graph.md  
**Status:** In Progress  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-05

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 1 | Implement `DirectedGraphPolicy.h` | Done | sonnet | Policy tags + policydata base-class template |
| 2 | Implement `DirectedGraphNode.h` + `.inl` | Done | sonnet | kMaxEdges param added for out-edge list capacity |
| 3 | Implement `DirectedGraphEdge.h` + `.inl` | Done | sonnet | GetFrom/GetTo instead of GetHead/GetTail |
| 4 | Implement `DirectedGraph.h` + `.inl` — core container | Done | sonnet | EBO via private inheritance of PolicyData |
| 5 | Implement traversal — BFS, DFS, TopoSort, HasCycle | Done | sonnet | Kahn's for TopoSort; caller-provided buffers |
| 6 | ReverseEdgeCache policy specialisation | Done | sonnet | Lazy rebuild; accessed via base-class members |
| 7 | AcyclicEnforced policy specialisation | Done | sonnet | CanReachNode DFS in AddEdge |
| 8 | Update `DiaCore.vcxproj` + `.vcxproj.filters` | Done | haiku | Added all 7 new files to Graphs filter |
| 9 | Update `dia.core.containers.graphs.architecture.module.md` | Done | haiku | Extended public_api entries |
| 10 | GoogleTest coverage | Done | sonnet | 11 suites, TestDirectedGraph.cpp |
| 11 | Phase 2 — `RelationshipIndex` wraps `DirectedGraph` | Not Started | sonnet | Deferred to separate session |

## Session Notes

### 2026-05-05
- Design decisions: EBO via private inheritance of `DirectedGraphPolicyData` base ensures zero overhead for `None`/`AcyclicEnforced` policies on MSVC (preferred over `[[no_unique_address]]` which MSVC may ignore).
- `DirectedGraphNode` adds `kMaxEdges` template param for the out-edge `DynamicArrayC` capacity.
- `AddEdge` registers the stored edge pointer (not a temporary) as out-edge of fromNode — avoids the dangling pointer issue present in the original `GraphEdge` constructor pattern.
- All traversals use caller-provided buffers; no heap allocation anywhere.
- TopoSort uses Kahn's (iterative, natural cycle detection, queue-as-array with front index).
