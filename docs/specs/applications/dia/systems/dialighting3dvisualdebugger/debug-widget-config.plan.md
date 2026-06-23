**Spec:** @docs/specs/applications/dia/systems/dialighting3dvisualdebugger/debug-widget-config.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `kLightWidgets` and `kLightPathArc` to `DebugLayerNames.h` | Build passes | Pending | haiku | |
| 2 | Create `Dia/DiaLighting3DVisualDebugger/` directory; create `DiaLighting3DVisualDebugger.vcxproj` and `.vcxproj.filters`; add project references; register in `Cluiche.sln` | Build passes | Pending | sonnet | |
| 3 | Create `dia.lighting3dvisualdebugger.architecture.module.md` YAML module doc | `dia docs registry` passes | Pending | haiku | |
| 4 | Implement `LightWidgetsDrawer.h/.cpp`; add to vcxproj | Build passes | Pending | sonnet | |
| 5 | Write `Cluiche/Tests/GoogleTests/DiaLighting3D/TestLightWidgetsDrawer.cpp`; add to `GoogleTests.vcxproj` | All 10 tests pass | Pending | sonnet | |
