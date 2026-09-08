**Spec:** @docs/specs/applications/dia/systems/dialighting3dvisualdebugger/debug-widget-config.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `kLightWidgets` and `kLightPathArc` to `DebugLayerNames.h` | Build passes | Done | haiku | |
| 2 | Create `Dia/DiaLighting3DVisualDebugger/` directory; create `DiaLighting3DVisualDebugger.vcxproj` and `.vcxproj.filters`; add project references; register in `Cluiche.sln` | Build passes | Done | sonnet | LightPathArcDrawer stub created to unblock build |
| 3 | Create `dia.lighting3dvisualdebugger.architecture.module.md` YAML module doc | `dia docs registry` passes | Done | haiku | |
| 4 | Implement `LightWidgetsDrawer.h/.cpp`; add to vcxproj | Build passes | Done | sonnet | Fixed RequestDrawRay3D to 4-arg signature |
| 5 | Write `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightWidgetsDrawer.cpp`; add to `GoogleTests.vcxproj` | All 10 tests pass | Done | sonnet | 10/10 pass |
