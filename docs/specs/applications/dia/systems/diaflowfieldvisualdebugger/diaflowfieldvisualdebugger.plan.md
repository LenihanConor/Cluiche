**Spec:** @docs/specs/applications/dia/systems/diaflowfieldvisualdebugger/diaflowfieldvisualdebugger.md
**Status:** Todo

## API Decisions

- `FlowField::GetWidth()`/`GetHeight()` added as non-debug accessors per SD-001 — trivial getters from stored width/height
- Arrow length = `cellSize × 0.4 × scale` per SD-002
- Unreachable cells draw no arrow per SD-003; absence is clearer than a zero-length line
- Debugger takes `const FlowField&` + `float cellSize` at construction
- Drawer enables use `std::atomic<bool>`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `FlowField::GetWidth()` and `FlowField::GetHeight()` non-debug accessors to `DiaFlowField`; constructor already stores width/height internally — accessors are trivial getters | Unit test: `FlowField(8, 4)` → `GetWidth()==8`, `GetHeight()==4`, `GetCellCount()==32` | Todo | haiku | Not DIA_DEBUG-gated; useful at runtime |
| 2 | Scaffold `DiaFlowFieldVisualDebugger` module: `FlowFieldVisualDebugger.h/.cpp`, `DirectionArrowsDrawer.h/.cpp`, `ReachabilityOverlayDrawer.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Todo | haiku | |
| 3 | Implement `FlowFieldVisualDebugger` identity, `HasWorldDrawers()=true`, `GetDrawerCount()=2`, `Register/Unregister`, `GetJSONState()` emitting drawers + stats (cellCount, reachableCount, isComplete), `OnCommand("toggle",...)`, `OnCommand("setScale","arrowLength")` | AC-4,10,11,12 pass | Todo | sonnet | `reachableCount` computed by iterating all cells in `GetJSONState` |
| 4 | Implement `DirectionArrowsDrawer::Draw()`: iterate via `GetWidth()`/`GetHeight()`; compute cell centre `{col*cellSize + cellSize*0.5, row*cellSize + cellSize*0.5}`; for each reachable cell draw arrow line in `FlowCell.direction`, length = `cellSize × 0.4 × GetDebugScale()`, colour `kDebugFlowDirection`; skip unreachable cells | R reachable cells → R line primitives; arrow endpoint direction matches `FlowCell.direction` | Todo | sonnet | |
| 5 | Implement `ReachabilityOverlayDrawer::Draw()`: iterate all cells; draw translucent quad per cell sized `cellSize × GetDebugScale()`; `kDebugGridPassable` for reachable, `kDebugGridImpassable` for unreachable | N-cell field → N quads; reachable/unreachable colours differ | Todo | sonnet | |
| 6 | Write mandatory test shapes in `Tests/GoogleTests/DiaFlowFieldVisualDebugger/TestFlowFieldVisualDebugger.cpp`: DrawerGate, PrimitiveType, ScaleSensitivity, JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Todo | sonnet | |
| 7 | Write domain-specific test shapes: OneArrowPerReachableCell, NoArrowForUnreachableCell, ArrowDirectionMatchesFlowCell, ReachabilityOverlay_OneCellPerAllCells, ReachabilityOverlay_ColoursDiffer, DrawerGate_ReachabilityOverlay, Toggle_ReachabilityOverlay, Stats_CellCount_Accurate, Stats_ReachableCount_Accurate, Stats_IsComplete_Mirrors_Field, EmptyField_ZeroCells_NoAssert, ArrowLength_Command | All 12 shapes pass | Todo | sonnet | |
| 8 | `dia check debugger-contract` full pass | Exit code 0 | Todo | haiku | |
