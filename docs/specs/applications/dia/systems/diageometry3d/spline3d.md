# Feature Spec: Spline3D

## Parent System
@docs/specs/applications/dia/systems/diageometry3d/diageometry3d.md

**Status:** `Done`

---

## Summary

Add a 3D spline primitive to DiaGeometry3D, mirroring `DiaGeometry2D::Spline` exactly but operating on `Vector3D`. Provides Catmull-Rom and B-Spline curve types with `Evaluate(t)` and `EvaluateTangent(t)` over `[0,1]`. Primary consumer is `DiaLighting3D`'s `LightPathBehaviour3D`; reusable by any system needing smooth 3D curves (camera paths, particle emitters, motion rails).

---

## Goals

1. `Spline3D` value type — up to 16 `Vector3D` control points, `CurveType` enum (BSpline, CatmullRom)
2. `SplineFactory3D` — `MakeBSpline()` and `MakeCatmullRom()` factory functions
3. `Evaluate(float t) -> Vector3D` and `EvaluateTangent(float t) -> Vector3D` (normalised) over `t ∈ [0,1]`
4. All code in `Dia::Geometry3D::` namespace at `Dia/DiaGeometry3D/Shapes/Spline3D.h+cpp`
5. GoogleTest suite covering evaluation endpoints, tangent direction, and factory preconditions

## Non-Goals

- Arclength re-parameterisation (constant-speed travel)
- NURBS or higher-order curves
- Serialisation / JSON loading
- Spline editor tooling

---

## Public API

```cpp
namespace Dia::Geometry3D
{
    class Spline3D
    {
    public:
        static constexpr int kMaxControlPoints = 16;

        enum class CurveType { BSpline, CatmullRom };

        Spline3D() = default;    // invalid until constructed by SplineFactory3D

        int                     GetControlPointCount() const;
        Dia::Maths::Vector3D    GetControlPoint(int index) const;
        CurveType               GetCurveType() const;

        // t in [0, 1] — clamped at endpoints
        Dia::Maths::Vector3D    Evaluate(float t) const;
        // Normalised tangent direction at t
        Dia::Maths::Vector3D    EvaluateTangent(float t) const;
    };

    class SplineFactory3D
    {
    public:
        // Uniform cubic B-spline: smooth approximation, does NOT pass through control points.
        static Spline3D MakeBSpline   (const Dia::Maths::Vector3D* points, int count);
        // Catmull-Rom: interpolating — passes through every control point.
        static Spline3D MakeCatmullRom(const Dia::Maths::Vector3D* points, int count);
        // Precondition for both: count >= 4 && count <= Spline3D::kMaxControlPoints
    };
}
```

---

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Implement `Spline3D` and `SplineFactory3D` in `Dia/DiaGeometry3D/Shapes/Spline3D.h+cpp`; add to `DiaGeometry3D.vcxproj` and `.vcxproj.filters` | Build passes |
| 2 | Write `Cluiche/Tests/GoogleTests/DiaGeometry3D/TestSpline3D.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="DiaGeometry3D_Spline*"` all pass |

---

## Inherited Binding Decisions

| ID | Decision | How it applies |
|---|---|---|
| PD-001 | StringCRC for IDs | Not applicable — Spline3D is a value type with no runtime identity |
| PD-004 | No STL in public API | Control points stored in a fixed-capacity internal array, not `std::vector` |
| PD-007 | C++20 required | Compiled under `/std:c++20` |
| AD-003 | Namespace `Dia::<Module>::` | `Dia::Geometry3D::Spline3D`, `Dia::Geometry3D::SplineFactory3D` |
| SD-002 | No `3D` suffix on type names | `Spline3D` is the exception — `Spline` is already taken by `DiaGeometry2D::Spline` in the same engine; the `3D` suffix is necessary to disambiguate across modules |

---

## Status

`Approved`
