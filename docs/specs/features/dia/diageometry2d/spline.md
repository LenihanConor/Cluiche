# Feature Spec: Spline

**Parent:** @docs/specs/systems/dia/diageometry2d.md  
**Status:** Done  
**Plan:** @docs/specs/features/dia/diageometry2d/spline.plan.md

---

## Problem Statement

DiaGeometry2D has no smooth parametric curve type. A `Spline` provides a curve through or near a set of control points, needed for movement paths, camera trajectories, and level geometry outlines.

---

## Solution Overview

Add a `Spline` class to `DiaGeometry2D` that stores a fixed array of up to 16 control points and evaluates points and tangents at a normalised parameter `t ∈ [0,1]`. The curve type is stored as a `CurveType` enum; `Evaluate()` and `EvaluateTangent()` switch on it internally. A `SplineFactory` builds a `Spline` for a specific algorithm (BSpline, CatmullRom).

Two factory functions ship in the first version:
- `SplineFactory::MakeBSpline(points, count)` — uniform cubic B-Spline (smooth approximation, does not necessarily pass through control points)
- `SplineFactory::MakeCatmullRom(points, count)` — Catmull-Rom (interpolating, passes through all control points)

Both produce the same `Spline` type; the stored `CurveType` determines which algorithm runs at evaluation time.

Arc-length parameterisation (constant-speed path traversal) is **deferred** — it requires a pre-built lookup table and is out of scope for this feature.

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|--------------|
| AC1 | `Spline` class exists in `Dia/DiaGeometry2D/Shapes/` under `Dia::Geometry2D::` namespace | File + grep |
| AC2 | `Spline` stores up to `kMaxControlPoints = 16` control points in a fixed array (no heap) | Code review |
| AC3 | `Evaluate(float t)` returns a `Vector2D` point on the curve for `t ∈ [0,1]` | Unit test |
| AC4 | `EvaluateTangent(float t)` returns the normalised tangent direction at `t` | Unit test |
| AC5 | `SplineFactory::MakeBSpline` produces a valid `Spline` from ≥ 4 control points | Unit test |
| AC6 | `SplineFactory::MakeCatmullRom` produces a valid `Spline` that passes through its control points at uniform `t` knots | Unit test |
| AC7 | Constructing a `Spline` with fewer than 4 control points fires `DIA_ASSERT` | Unit test |
| AC8 | `SplineFactory` is in `DiaGeometry2D` — not a separate module | File check |
| AC9 | `DiaGeometry2D.vcxproj` and `.vcxproj.filters` list all new files | Project file review |
| AC10 | `ShapeDrawer` gains `SubmitSpline(const Spline&, int segments, RGBA)` in `DiaGeometry2DVisualDebugger` — tessellates to line segments | Code review |
| AC11 | Unit tests in `Cluiche/Tests/GoogleTests/Geometry2D/TestSpline.cpp` pass under `dia run googletest` | `dia run googletest --filter="Geometry2D_Spline*"` |
| AC12 | Full solution builds clean in Debug\|x64 | `dia run googletest` green |

---

## Public API

```cpp
namespace Dia::Geometry2D {

class Spline
{
public:
    static constexpr int kMaxControlPoints = 16;

    enum class CurveType { BSpline, CatmullRom };

    int                          GetControlPointCount() const;
    const Dia::Maths::Vector2D&  GetControlPoint(int index) const;
    CurveType                    GetCurveType() const;

    Dia::Maths::Vector2D  Evaluate(float t)        const;  // t in [0,1]
    Dia::Maths::Vector2D  EvaluateTangent(float t) const;  // normalised

private:
    friend class SplineFactory;

    Spline();  // construct via SplineFactory only

    Dia::Maths::Vector2D mControlPoints[kMaxControlPoints];
    int                  mControlPointCount = 0;
    CurveType            mCurveType         = CurveType::BSpline;
};

class SplineFactory
{
public:
    // Uniform cubic B-Spline. Smooth approximation; does not pass through points.
    // Requires count >= 4. DIA_ASSERT fires if count < 4 or count > Spline::kMaxControlPoints.
    static Spline MakeBSpline(const Dia::Maths::Vector2D* points, int count);

    // Catmull-Rom spline. Interpolating; passes through every control point.
    // Requires count >= 4. DIA_ASSERT fires if count < 4 or count > Spline::kMaxControlPoints.
    static Spline MakeCatmullRom(const Dia::Maths::Vector2D* points, int count);
};

} // namespace Dia::Geometry2D
```

Visual debugger addition (`DiaGeometry2DVisualDebugger`):

```cpp
// ShapeDrawer — new Submit method
void SubmitSpline(const Dia::Geometry2D::Spline& spline, int segments, Dia::Graphics::RGBA colour);
// Tessellates spline into 'segments' line segments and submits them.
// 'segments' is clamped to avoid exceeding kMaxShapes budget.
```

---

## Design Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| DD-1 | `CurveType` enum + switch in `Evaluate()` | Simpler than function pointers; readable at a glance; sufficient for two curve types |
| DD-2 | `kMaxControlPoints = 16` | Consistent with `ConvexPolygon::kMaxVertices`; sufficient for smooth game paths; no heap |
| DD-3 | Arc-length parameterisation deferred | Requires a pre-built LUT; out of scope — add as a separate `SplineSampler` if constant-speed traversal is needed |
| DD-4 | `Spline` constructor is private; `SplineFactory` is the only construction path | Prevents construction of a `Spline` with no curve type set |

---

## Binding Decisions Compliance

| Decision | Source | Compliance |
|----------|--------|------------|
| SD-001 — Namespace `Dia::Geometry2D::` | System | ✅ `Spline` and `SplineFactory` in `Dia::Geometry2D::` |
| SD-002 — Drop `2D` suffix | System | ✅ Named `Spline`, not `Spline2D` |
| SD-005 — No STL in public API | System | ✅ Fixed array; no `std::vector` |
| PD-004 — No STL containers in public APIs | Platform | ✅ |
| PD-006 — VS project files are source of truth | Platform | ✅ All files listed in `.vcxproj` |
| PD-007 — C++20 | Platform | ✅ |

---

## Files Affected

### New
- `Dia/DiaGeometry2D/Shapes/Spline.h`
- `Dia/DiaGeometry2D/Shapes/Spline.cpp`
- `Dia/DiaGeometry2D/Shapes/SplineFactory.h`
- `Dia/DiaGeometry2D/Shapes/SplineFactory.cpp`
- `Cluiche/Tests/GoogleTests/Geometry2D/TestSpline.cpp`

### Modified
- `Dia/DiaGeometry2D/DiaGeometry2D.vcxproj` + `.vcxproj.filters` — add Spline + SplineFactory entries
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` + `.vcxproj.filters` — add `TestSpline.cpp`
- `Dia/DiaGeometry2DVisualDebugger/ShapeDrawer.h` + `.cpp` — add `SubmitSpline`
- `Dia/DiaGeometry2DVisualDebugger/DiaGeometry2DVisualDebugger.vcxproj` + `.vcxproj.filters` — if a `SplineDrawHelper` is added
- `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DShapesDrawer.cpp` — add spline to gallery (Row 3)
- `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DLabelsDrawer.cpp` — add `"spline"` label
