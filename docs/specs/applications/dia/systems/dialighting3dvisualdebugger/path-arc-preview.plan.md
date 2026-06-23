**Spec:** @docs/specs/applications/dia/systems/dialighting3dvisualdebugger/path-arc-preview.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `GetSpline() const` to `LightPathBehaviour3D.h/.cpp` | Build passes | Done | haiku | Also added GetXxxIdByIndex to LightRegistry3D + const on GetPathBehaviour |
| 2 | Implement `LightPathArcDrawer.h/.cpp`; add to `DiaLighting3DVisualDebugger.vcxproj` | Build passes | Done | sonnet | |
| 3 | Write `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightPathArcDrawer.cpp`; add to `GoogleTests.vcxproj` | All 10 tests pass | Done | sonnet | 10/10 pass |
