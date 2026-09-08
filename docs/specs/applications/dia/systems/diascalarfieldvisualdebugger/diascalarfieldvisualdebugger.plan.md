**Spec:** @docs/specs/applications/dia/systems/diascalarfieldvisualdebugger/diascalarfieldvisualdebugger.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add arrowhead triangle to `ScalarFieldGradientOverlay::DrawCell` in `ScalarFieldOverlay.h` — compute tip, perp, headLen/headWidth, call `RequestDraw(p1,p2,p3,...)` | Build passes | Pending | sonnet | Resolves SFVD-004; fixed proportions: headLen = shaft * 0.3, headWidth = shaft * 0.2 |
| 2 | Expand `TestScalarFieldVisualDebugger.cpp` with exhaustive tests (see list below); add `triangleCount` to `MockDebugDraw`; add file to `GoogleTests.vcxproj` via `dia docs vcxproj-add` | `dia run googletest --filter="ScalarFieldVisualDebugger*"` all pass | Pending | sonnet | Replaces + extends existing 7 tests; test file untracked — vcxproj entry needed |
| 3 | Write `Dia/DiaScalarField/Docs/dia.diascalarfieldvisualdebugger.architecture.module.md` | n/a (docs) | Pending | haiku | Header-only adaptor; optional dep on dia.visualdebugger; no vcxproj of its own |
| 4 | Run `dia docs registry` to regenerate module-registry.md | Registry updated | Pending | haiku | |
| 5 | Run `dia docs spec-done` to mark spec Done | Spec status updated | Pending | haiku | |

### Task 2 — Exhaustive test list

**MockDebugDraw additions**
- Add `triangleCount` counter to the triangle `RequestDraw(p1,p2,p3,...)` overload

**Heatmap — colour mapping**
- `HeatmapOverlay_Draw_LowValue_UsesLowColour` — 1×1 field at value 0 → fill == lowColour
- `HeatmapOverlay_Draw_MidValue_UsesLerpedColour` — 1×1 field at value 0.5 → fill channel is midpoint between low/high
- `HeatmapOverlay_Draw_ClampBelowMin_UsesLowColour` — value < minValue (e.g. -1) → clamps to lowColour
- `HeatmapOverlay_Draw_ClampAboveMax_UsesHighColour` — value > maxValue (e.g. 2) → clamps to highColour

**Heatmap — control**
- `HeatmapOverlay_Draw_AllCellsVisited` *(existing — keep)*
- `HeatmapOverlay_Draw_HighValue_UsesHighColour` *(existing — keep)*
- `HeatmapOverlay_Draw_WhenDisabled_DrawsNothing` *(existing — keep)*
- `HeatmapOverlay_GetLayerName_ReturnsConstructorValue` — `GetLayerName()` returns the CRC passed at construction
- `HeatmapOverlay_Draw_ReenableAfterDisable_DrawsAgain` — disable → draw (0 rects) → enable → draw → rects == cellCount
- `HeatmapOverlay_Draw_WorldOriginOffset_CellPositionShifted` — non-zero worldOrigin: `DrawCell` on (0,0) produces rect min at origin, not (0,0)

**Heatmap — hex topology**
- `HeatmapOverlay_Hex_Draw_AllCellsVisited` — `HexScalarField` radius 2 (19 cells) → rectCount == 19

**Gradient — draw counts**
- `GradientOverlay_Draw_ZeroGradient_SkipsCell` *(existing — keep)*
- `GradientOverlay_Draw_NonZeroGradient_DrawsRay` *(existing — keep, extend to also assert `triangleCount > 0`)*
- `GradientOverlay_Draw_NonZeroGradient_RayAndTriangleCountMatch` — peak at centre of 5×5 field; after Tick, `rayCount == triangleCount` (each arrow shaft has exactly one arrowhead)
- `GradientOverlay_Draw_WhenDisabled_DrawsNothing` — `SetEnabled(false)` → rayCount == 0, triangleCount == 0
- `GradientOverlay_Draw_ReenableAfterDisable_DrawsAgain` — disable → enable → non-zero gradient field → rayCount > 0
- `GradientOverlay_GetLayerName_ReturnsConstructorValue`

**Gradient — minMagnitude threshold**
- `GradientOverlay_Draw_BelowMinMagnitude_SkipsCell` — construct with `minMagnitude = 1.0f`; write gradient that produces magnitude < 1.0 → rayCount == 0
- `GradientOverlay_Draw_AtMinMagnitude_DrawsCell` — construct with `minMagnitude = 0.01f`; write value producing gradient magnitude just above 0.01 → rayCount > 0

**Gradient — hex topology**
- `GradientOverlay_Hex_Draw_NonZeroGradient_DrawsRays` — `HexScalarField` radius 2, write peak at (0,0), Tick → rayCount > 0
