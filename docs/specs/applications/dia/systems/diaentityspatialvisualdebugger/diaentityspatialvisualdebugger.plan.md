**Spec:** @docs/specs/applications/dia/systems/diaentityspatialvisualdebugger/diaentityspatialvisualdebugger.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `kEntitySpatialGrid`, `kEntitySpatialEntities`, `kEntitySpatialQuery` constants to `DebugLayerNames.h` | Build passes | Done | haiku | |
| 2 | Write `Dia/DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h` — all three overlay classes (GridOverlay, EntityOverlay, QueryOverlay) | Build passes | Done | sonnet | Header-only; no vcxproj entry needed |
| 3 | Write `Cluiche/Tests/GoogleTests/DiaEntitySpatial/TestEntitySpatialVisualDebugger.cpp` + add to `GoogleTests.vcxproj` | `dia run googletest --filter="EntitySpatialVisualDebugger*"` all pass | Done | sonnet | 15/15 pass |
| 4 | Write `Dia/DiaEntitySpatial/Docs/dia.diaentityspatialvisualdebugger.architecture.module.md` | n/a (docs) | Done | haiku | |
| 5 | Run `dia docs registry` to regenerate module-registry.md | Registry updated | Done | haiku | |
