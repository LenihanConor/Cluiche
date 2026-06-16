# Implementation Plan: Spline

**Spec:** @docs/specs/applications/dia/systems/diageometry2d/spline.md  
**Status:** Done

---

## Implementation Patterns

### Spline class (`Spline.h` / `Spline.cpp`)
- Plain value type, no vtable, no heap
- `mCurveType` enum drives `switch` in `Evaluate()` and `EvaluateTangent()`
- Private constructor; `SplineFactory` is `friend`
- Fixed array `mControlPoints[16]`, `mControlPointCount` int
- `Evaluate(t)` maps `t ∈ [0,1]` to the appropriate segment using `(mControlPointCount - 3)` segments for BSpline and `(mControlPointCount - 1)` for CatmullRom; computes local `u` per segment

### SplineFactory (`SplineFactory.h` / `SplineFactory.cpp`)
- Two static methods: `MakeBSpline`, `MakeCatmullRom`
- Both call `DIA_ASSERT(count >= 4 && count <= Spline::kMaxControlPoints)`
- Copies control points into private `Spline` fields, sets `mCurveType`

### BSpline evaluation (cubic uniform)
- Basis matrix: standard uniform cubic B-Spline `M_bs` (4×4)
- For parameter `t`: find segment `i = floor(t * (n-3))`, local `u = frac`
- Point: `M_bs * [u³ u² u 1]ᵀ` applied to 4 consecutive control points
- Tangent: derivative matrix applied to same 4 points, then normalise

### CatmullRom evaluation
- Basis matrix: standard Catmull-Rom `M_cr` (tension = 0.5)
- Segment count = `n - 1`; segment `i = floor(t * (n-1))`, local `u = frac`
- Point + tangent computed from 4-point stencil (clamp at ends)

### ShapeDrawer extension
- Add `ShapeType::Spline` enum entry
- Store a `SplineData` struct: copy of the `Spline` object + `int segments`
- `SubmitSpline` fills a `ShapeEntry` of type `Spline`
- `Draw()` tessellates: loop `segments` steps, call `spline.Evaluate(i / segments)`, emit `Line` draw calls

### Tests (`TestSpline.cpp`)
- Test class naming: `TEST(Geometry2D_Spline, ...)`
- Cover: construction via factory, `GetControlPointCount`, `Evaluate` at `t=0/0.5/1`, `EvaluateTangent` non-zero and normalised, CatmullRom passes through endpoints, assert fires on < 4 points

### Shape gallery draw (`Geometry2DShapesDrawer.cpp`)
- Add a `Spline` (CatmullRom) as a new entry in **Row 3** (Y=-50) at X=0 (or append to Row 2 if space allows — check existing spacing of 150 units)
- Call `shapeDrawer.SubmitSpline(spline, 32, colour)` — 32 segments gives smooth visual
- Use the same white colour as other primitives

### Shape gallery label (`Geometry2DLabelsDrawer.cpp`)
- Add label `"spline"` below the new spline entry at `position + kLabelOffsetY`
- No asterisk needed (drawn natively, not converted to ConvexPolygon)

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `Spline.h` / `Spline.cpp` — class skeleton, private ctor, `CurveType` enum, fixed array, public getters | Build clean | Done | sonnet | No eval logic yet |
| 2 | Add `SplineFactory.h` / `SplineFactory.cpp` — `MakeBSpline` + `MakeCatmullRom` with asserts and field population | Build clean | Done | sonnet | Depends on task 1 |
| 3 | Implement `Evaluate()` + `EvaluateTangent()` for BSpline (basis matrix, segment selection) | AC3, AC5 unit tests | Done | sonnet | |
| 4 | Implement `Evaluate()` + `EvaluateTangent()` for CatmullRom | AC4, AC6 unit tests | Done | sonnet | Depends on task 3 |
| 5 | Write `TestSpline.cpp` — full AC3–AC7 unit test coverage | AC11 green | Done | sonnet | |
| 6 | Update `DiaGeometry2D.vcxproj` + `.vcxproj.filters` — add Spline + SplineFactory files; update `GoogleTests.vcxproj` + `.vcxproj.filters` — add `TestSpline.cpp` | AC9, AC12 build clean | Done | haiku | |
| 7 | Extend `ShapeDrawer` — `SubmitSpline`, `SplineData`, `ShapeType::Spline`, `Draw()` tessellation | AC10 | Done | sonnet | Touch `DiaGeometry2DVisualDebugger.vcxproj` if SplineDrawHelper added |
| 8 | Add spline to `Geometry2DShapesDrawer` — CatmullRom spline at Row 3 Y=-50, `SubmitSpline(..., 32, colour)` | Visual in gallery | Done | sonnet | Depends on task 7 |
| 9 | Add label `"spline"` to `Geometry2DLabelsDrawer` at matching position + `kLabelOffsetY` | Label visible in gallery | Done | haiku | Depends on task 8 |
| 10 | Final verification — `dia run googletest --filter="Geometry2D_Spline*"` all green, full solution build clean | AC11, AC12 | Done | haiku | |
