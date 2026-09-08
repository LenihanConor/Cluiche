**Spec:** @docs/specs/applications/dia/systems/dialighting3d/light-path-behaviour.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DiaGeometry3D` ProjectReference to `DiaLighting3D.vcxproj`; move `dia.geometry3d` from `forbidden` to `required` in `dia.lighting3d.architecture.module.md` | Build passes, no circular dep | Done | haiku | Committed aea625a7 |
| 2 | Implement `LightPathBehaviour3D.h+cpp` in `Dia/DiaLighting3D/Behaviours/`; add `GetPathBehaviour()` to `LightRegistry3D`; add all files to vcxproj | Build passes | Done | sonnet | Committed e95d4bce |
| 3 | Write `TestLightPathBehaviour3D.cpp` in `Cluiche/Tests/GoogleTests/DiaLighting3D/`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaLighting3D_PathBehaviour*"` all pass | Done | sonnet | 7/7 pass. Committed f7f473c2 |
