# Plan: PU Parent-Child Tree

**Spec:** @docs/specs/features/dia/diaapplication/pu-parent-child-tree.md  
**Status:** Done  
**Started:** 2026-05-06  
**Last Updated:** 2026-05-06

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add tree API declarations to ProcessingUnit.h + parentInstanceId to ApplicationManifest | — | Done | opus | Added virtual destructor, tree API, mTreeStopRequested |
| 2 | Create TestPUTree.cpp with unit tests (AC1-5, AC8-10, AC13, AC15) | TestPUTree | Done | opus | 11 tests |
| 3 | Implement tree methods in ProcessingUnit.cpp | TestPUTree | Done | opus | AddChild, Remove, Find, IsRoot, tree traversal, max depth |
| 4 | Add LoadApplicationTree to ApplicationLoader.h/.cpp | TestPUTreeIntegration | Done | opus | Root validation, tree construction from manifest |
| 5 | Create TestPUTreeIntegration.cpp (AC6, AC7, AC11, AC14) | TestPUTreeIntegration | Done | opus | 4 integration tests |
| 6 | Implement auto thread lifecycle in DoStart/DoStop | TestPUTreeIntegration | Done | opus | Skips manually-started children |
| 7 | Update vcxproj + vcxproj.filters for test files | — | Done | opus | TestPUTree.cpp + TestPUTreeIntegration.cpp |
| 8 | Build + run all tests | GoogleTest suite | Done | opus | 4307/4308 pass (1 pre-existing failure) |
| 9 | Migrate CluicheTest MainProcessingUnit to use tree (AC12) | CluicheTest builds | Done | opus | Tree ownership, manual start+thread retained |
| 10 | Final verification + update spec/backlog status | — | Done | opus | Spec→Done, backlog updated |

## Session Notes

### 2026-05-06
- Phases A, C, D confirmed done. Only Phase B remained.
- Used std::vector for owned children (DynamicArrayC::RemoveAt requires copy which UniquePtr can't do)
- Added virtual destructor to ProcessingUnit (was missing despite being polymorphic)
- Added mTreeStopRequested atomic for parent-signaled shutdown
- Auto-lifecycle skips already-started children (supports hybrid manual/tree pattern)
- CluicheTest migrated to tree ownership with manual start/thread (option A)
- Error propagation: child ReportError now bubbles up to parent

