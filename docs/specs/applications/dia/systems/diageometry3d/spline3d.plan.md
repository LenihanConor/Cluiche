**Spec:** @docs/specs/applications/dia/systems/diageometry3d/spline3d.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `Spline3D` + `SplineFactory3D` in `Shapes/Spline3D.h+cpp`; add to `DiaGeometry3D.vcxproj` + `.filters` | Build passes | Done | sonnet | Mirrors DiaGeometry2D::Spline exactly with Vector3D |
| 2 | Write initial `TestSpline3D.cpp` (endpoints, tangents, factory preconditions); add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaGeometry3D_Spline*"` all pass | Done | sonnet | 28 tests pass |
| 3 | Audit test exhaustiveness; fill gaps (boundary t, normalization, curve-type divergence, max/min point counts) | All extended tests pass | Done | sonnet | Merged into task 2 — all gap tests included in initial file |
| 4 | Observation scan — check `Spline3D` + `SplineFactory3D` against 5 DiaObservation pillars; note gaps | No code change expected | Done | haiku | No gaps on value type; suggest trace zone in LightPathBehaviour3D call-site |
