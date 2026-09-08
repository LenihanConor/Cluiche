# Feature Spec: 3D Shape Primitives

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaGeometry3D | @docs/specs/applications/dia/systems/diageometry3d/diageometry3d.md |
| Feature | 3D Shape Primitives | (this document) |

## Summary

Add the nine 3D shape types that make up `Dia::Geometry3D::`'s primitive surface: `AABB`, `OOBB`, `Sphere`, `Capsule`, `Triangle`, `Cylinder`, `Ray`, `Plane`, `Frustum`. All under `Dia::Geometry3D::` namespace; the dimension is captured by the namespace, no `3D` suffix on type names (per system SD-002).

Mirrors the structure of `Dia/DiaGeometry2D/Shapes/` — each shape is a small, copy-able value type with a corresponding `.h` / `.cpp` / `.inl`. Each shape provides shape-method `IsIntersecting()` and `Contains()` for the most common cases; the full pairwise matrix lives in the separate `intersection-tests` feature.

## Problem

DiaGeometry3D has no shape types yet — the system spec lists them in its public API but no implementation exists. Without these primitives, neither `IntersectionTests` nor `SpatialGrid3D` can be built, and the entire 3D geometry stack is blocked. This feature is the foundation: just the types, with minimal shape-method intersection convenience for hot-path point-in-shape queries.

## Acceptance Criteria

### General (applies to every shape)

1. All types live under `Dia::Geometry3D::` namespace. Header guard pattern: `DIA_GEOMETRY3D_<NAME>_H`. File layout: `Dia/DiaGeometry3D/Shapes/<Name>.h`, `.cpp`, `.inl`.
2. Each shape is value-semantic: copy-constructible, copy-assignable, equality-comparable (where geometrically meaningful — Triangle equality compares vertices in order; Frustum equality compares plane-by-plane).
3. Each shape has at least one default constructor producing a deterministic-but-degenerate state (e.g. AABB default = zero-volume at origin; Sphere default = zero-radius at origin) and a fully-specified constructor.
4. Each shape exposes accessors for its defining state (e.g. `AABB::GetMin() / GetMax()`, `Sphere::GetCenter() / GetRadius()`).
5. Each shape exposes derived geometry methods relevant to it (e.g. `AABB::CalculateCenter()`, `Sphere::CalculateVolume()`, `Plane::DistanceTo(point)`, `Triangle::CalculateNormal()`).
6. Shape-method `IsIntersecting(const Vector3D& point)` returns `IntersectionClassify` for every closed-volume shape (`AABB`, `OOBB`, `Sphere`, `Capsule`, `Cylinder`, `Frustum`).
7. Shape-method `Contains(const Vector3D& point)` returns `bool` for every closed-volume shape (sugar for `IsIntersecting(point) != kNoIntersection`).
8. **No** shape-method shape-vs-shape intersection lives in this feature — those go through `IntersectionTests::Test()` in the separate `intersection-tests` feature. Shape-on-shape intersection method pollution is avoided per the user direction.
9. **No STL containers** in any public method signature.
10. Header includes are minimal — forward-declare other shapes where possible.

### Per-shape acceptance criteria

11. **`AABB`** (axis-aligned bounding box): `min` / `max` Vector3D corners; `min ≤ max` per axis is invariant; constructors include `(Vector3D min, Vector3D max)` and `FromCenterExtents(center, extents)`; `CalculateCenter`, `CalculateExtents`, `CalculateSurfaceArea`, `CalculateVolume`, `Encapsulate(Vector3D)` (mutator), `Encapsulate(AABB)` (mutator).
12. **`OOBB`** (oriented bounding box): center (Vector3D), half-extents (Vector3D), orientation (Quaternion); `GetAxes(Vector3D outX, outY, outZ)` returns the three local axes via Quaternion::Rotate of the world axes; Quaternion is the storage form per system SD-009 + `diamaths.md` SD-009.
13. **`Sphere`**: center (Vector3D), radius (float, ≥ 0); `CalculateVolume`, `CalculateSurfaceArea`, `Encapsulate(Vector3D)` (mutator that grows radius if needed).
14. **`Capsule`**: two endpoints (Vector3D startA, Vector3D endB) + radius (float, ≥ 0); endpoints define the cylinder axis; `CalculateLength`, `CalculateAxis()` (returns endB - startA, not normalized), `CalculateAxisDirection()` (returns normalized).
15. **`Triangle`**: three vertices (Vector3D v0, v1, v2); `CalculateNormal()` returns `(v1 - v0).Cross(v2 - v0).AsNormal()` (right-handed); `CalculateArea`, `CalculateCentroid`.
16. **`Cylinder`**: two endpoints (Vector3D startA, Vector3D endB) + radius (float, ≥ 0); axis from startA to endB; `CalculateLength`, `CalculateAxis()`, `CalculateAxisDirection()`. Capped (flat ends), not infinite. *Cylinder is in v1 per user direction during feature interview, despite system spec Q9 leaving it open.*
17. **`Ray`**: origin (Vector3D) + direction (Vector3D, unit length expected); constructor asserts in debug if direction has near-zero magnitude; `GetPointAt(float t)` returns `origin + direction * t`.
18. **`Plane`**: normal (Vector3D, unit length expected) + d (float; plane equation `dot(normal, p) = d`); `DistanceTo(Vector3D point)` returns signed distance (positive on +normal side); `ClassifyPoint(point)` returns `enum class PlaneSide { kFront, kBehind, kOnPlane }` (epsilon-tolerant); factory `FromPointAndNormal(Vector3D p, Vector3D n)` and `FromThreePoints(v0, v1, v2)`.
19. **`Frustum`**: array of six `Plane`s with explicit slot enum (`kNear`, `kFar`, `kLeft`, `kRight`, `kTop`, `kBottom`); planes' normals point INWARD (a point inside the frustum is on the +normal side of every plane); factory `FromMatrix44(const Matrix44& viewProjection)` extracts the six planes from a view-projection matrix using the Gribb–Hartmann method; constructor `Frustum(Plane near, Plane far, Plane left, Plane right, Plane top, Plane bottom)`; `IsIntersecting(const Vector3D& point)` returns `IntersectionClassify::kPenetrating` if inside, `kNoIntersection` otherwise (no containment-of-point makes sense for a single point); `GetPlane(FrustumPlane slot)` accessor.

### Test utilities

20. A `Dia/DiaGeometry3D/Testing/` directory contains `Geometry3DShapeFactory.h` with canonical builders: `MakeUnitAABB`, `MakeUnitSphere`, `MakeAxisAlignedTriangle`, `MakeIdentityFrustum` (a unit cube as a frustum), etc. Used by both feature tests and downstream consumers.

### Project file integration

21. `Dia/DiaGeometry3D/DiaGeometry3D.vcxproj` is created as a static library. Includes all 9 shape `.h/.cpp/.inl` triples (27 files), the `IntersectionClassify.h` enum (small header listed in this feature), and Testing fixtures.
22. `.vcxproj.filters` organises files under `Shapes` and `Testing` filters.
23. `Dia/DiaGeometry3D/Docs/dia.geometry3d.architecture.module.md` exists with YAML frontmatter listing all 9 shapes plus `IntersectionClassify` in `public_api.entry_points`. Lists `dia.maths` and `dia.core` in `dependent_modules`.
24. `Cluiche/Cluiche.sln` adds `DiaGeometry3D.vcxproj` and the project's dependencies.

### IntersectionClassify enum

25. `Dia/DiaGeometry3D/Shapes/IntersectionClassify.h` defines `Dia::Geometry3D::IntersectionClassify { kNoIntersection, kPenetrating, kAContainsB, kBContainsA }` per system SD-013 (duplicated from 2D, not shared — avoids cross-module include).

## API Design

```cpp
// Dia/DiaGeometry3D/Shapes/IntersectionClassify.h
namespace Dia::Geometry3D {
    enum class IntersectionClassify {
        kNoIntersection,
        kPenetrating,
        kAContainsB,
        kBContainsA,
    };
}

// Dia/DiaGeometry3D/Shapes/AABB.h
namespace Dia::Geometry3D {
    class Sphere;

    class AABB
    {
    public:
        AABB();                                                        // (0,0,0) → (0,0,0)
        AABB(const AABB& rhs);
        AABB(const Dia::Maths::Vector3D& min, const Dia::Maths::Vector3D& max);

        AABB& operator=(const AABB& rhs);
        bool  operator==(const AABB& rhs) const;
        bool  operator!=(const AABB& rhs) const;

        static AABB FromCenterExtents(const Dia::Maths::Vector3D& center, const Dia::Maths::Vector3D& extents);

        const Dia::Maths::Vector3D& GetMin() const;
        const Dia::Maths::Vector3D& GetMax() const;

        Dia::Maths::Vector3D CalculateCenter()      const;
        Dia::Maths::Vector3D CalculateExtents()     const;             // half-size per axis
        float                CalculateSurfaceArea() const;
        float                CalculateVolume()      const;

        void Encapsulate(const Dia::Maths::Vector3D& point);
        void Encapsulate(const AABB& other);

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Dia::Maths::Vector3D mMin;
        Dia::Maths::Vector3D mMax;
    };
}

// Dia/DiaGeometry3D/Shapes/OOBB.h
namespace Dia::Geometry3D {
    class OOBB
    {
    public:
        OOBB();
        OOBB(const OOBB& rhs);
        OOBB(const Dia::Maths::Vector3D& center, const Dia::Maths::Vector3D& halfExtents,
             const Dia::Maths::Quaternion& orientation);

        // ... value-type boilerplate ...

        const Dia::Maths::Vector3D&   GetCenter()      const;
        const Dia::Maths::Vector3D&   GetHalfExtents() const;
        const Dia::Maths::Quaternion& GetOrientation() const;

        // Local axes (orientation applied to world XYZ axes)
        void GetAxes(Dia::Maths::Vector3D& outX,
                     Dia::Maths::Vector3D& outY,
                     Dia::Maths::Vector3D& outZ) const;

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;
    };
}

// Dia/DiaGeometry3D/Shapes/Sphere.h, Capsule.h, Triangle.h, Cylinder.h, Ray.h
// Each follows the AABB pattern with type-specific accessors and methods listed in ACs above.

// Dia/DiaGeometry3D/Shapes/Plane.h
namespace Dia::Geometry3D {
    enum class PlaneSide { kFront, kBehind, kOnPlane };

    class Plane
    {
    public:
        Plane();                                                       // unit Y-up plane through origin
        Plane(const Plane& rhs);
        Plane(const Dia::Maths::Vector3D& normal, float d);

        static Plane FromPointAndNormal(const Dia::Maths::Vector3D& point, const Dia::Maths::Vector3D& normal);
        static Plane FromThreePoints(const Dia::Maths::Vector3D& v0, const Dia::Maths::Vector3D& v1, const Dia::Maths::Vector3D& v2);

        const Dia::Maths::Vector3D& GetNormal() const;
        float                       GetD()      const;

        float     DistanceTo  (const Dia::Maths::Vector3D& point) const;       // signed; +ve on +normal side
        PlaneSide ClassifyPoint(const Dia::Maths::Vector3D& point, float epsilon = 1e-5f) const;
    };
}

// Dia/DiaGeometry3D/Shapes/Frustum.h
namespace Dia::Geometry3D {
    enum class FrustumPlane : int { kNear = 0, kFar, kLeft, kRight, kTop, kBottom };

    class Frustum
    {
    public:
        Frustum();
        Frustum(const Plane& near, const Plane& far,
                const Plane& left, const Plane& right,
                const Plane& top,  const Plane& bottom);

        // Inward-pointing normals: a point inside the frustum is on +normal side of every plane.
        // Extract via Gribb–Hartmann from a view-projection matrix.
        static Frustum FromMatrix44(const Dia::Maths::Matrix44& viewProjection);

        const Plane& GetPlane(FrustumPlane slot) const;

        IntersectionClassify IsIntersecting(const Dia::Maths::Vector3D& point) const;
        bool                 Contains      (const Dia::Maths::Vector3D& point) const;

    private:
        Plane mPlanes[6];   // indexed by FrustumPlane enum
    };
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaGeometry3D/` directory layout | Shapes/, Intersection/ (placeholder), Spatial/ (placeholder), Testing/, Docs/ |
| 2 | Create `IntersectionClassify.h` | Enum only |
| 3 | Create `AABB.h/.cpp/.inl` | Per AC 11 |
| 4 | Create `OOBB.h/.cpp/.inl` | Per AC 12 |
| 5 | Create `Sphere.h/.cpp/.inl` | Per AC 13 |
| 6 | Create `Capsule.h/.cpp/.inl` | Per AC 14 |
| 7 | Create `Triangle.h/.cpp/.inl` | Per AC 15 |
| 8 | Create `Cylinder.h/.cpp/.inl` | Per AC 16 |
| 9 | Create `Ray.h/.cpp/.inl` | Per AC 17 |
| 10 | Create `Plane.h/.cpp/.inl` and `PlaneSide` enum | Per AC 18 |
| 11 | Create `Frustum.h/.cpp/.inl` and `FrustumPlane` enum | Per AC 19 — Gribb–Hartmann extraction |
| 12 | Create `Testing/Geometry3DShapeFactory.h` | Canonical builders for tests |
| 13 | Create `Docs/dia.geometry3d.architecture.module.md` | YAML frontmatter |
| 14 | Create `DiaGeometry3D.vcxproj` and `.vcxproj.filters` | Static library |
| 15 | Add `DiaGeometry3D.vcxproj` to `Cluiche/Cluiche.sln` | Wire up dependencies on DiaMaths and DiaCore |
| 16 | Add tests | One file per shape under `Cluiche/Tests/GoogleTests/Geometry3D/Test<Shape>.cpp` |
| 17 | Run `dia run googletest --filter="AABB*:OOBB*:Sphere*:Capsule*:Triangle*:Cylinder*:Ray3D*:Plane3D*:Frustum*"` | All green |

## Test Plan (Task 16)

Per shape, one test file. Coverage outline:

| Suite | Tests |
|-------|-------|
| `AABBTest` | Default state, FromMinMax, FromCenterExtents, accessors, CalculateCenter/Extents/Volume/SurfaceArea, Encapsulate(point) grows correctly, Encapsulate(AABB) merges correctly, IsIntersecting(point in/out/on-edge/on-corner), Contains parity |
| `OOBBTest` | Construction, GetAxes returns Quaternion-rotated world axes, IsIntersecting(point) for axis-aligned and rotated cases |
| `SphereTest` | Construction, accessors, CalculateVolume/SurfaceArea, Encapsulate(point) grows radius, IsIntersecting(point inside/outside/on-surface), Contains |
| `CapsuleTest` | Construction, length / axis / axis-direction calculations, IsIntersecting(point) inside the capsule core / inside the spherical caps / outside |
| `TriangleTest` | Construction, CalculateNormal right-handedness (counter-clockwise winding → +Y normal for XZ-plane triangle), CalculateArea, CalculateCentroid |
| `CylinderTest` | Construction, length / axis, IsIntersecting(point) inside / outside / on flat cap edge |
| `Ray3DTest` | Construction, near-zero direction asserts in debug, GetPointAt at t=0 returns origin and at t=1 returns origin+direction |
| `Plane3DTest` | FromPointAndNormal preserves the point and normal, FromThreePoints right-handedness (counter-clockwise winding → +normal direction), DistanceTo signed, ClassifyPoint front/behind/on-plane with epsilon |
| `FrustumTest` | Default state, FromMatrix44 of an identity view-proj produces a unit cube frustum, FromMatrix44 of a Y-up RH perspective extracts six inward planes, IsIntersecting(point) inside/outside, GetPlane returns correct slot |

## Files

| File | Action |
|------|--------|
| `Dia/DiaGeometry3D/Shapes/IntersectionClassify.h` | Create |
| `Dia/DiaGeometry3D/Shapes/AABB.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/OOBB.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Sphere.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Capsule.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Triangle.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Cylinder.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Ray.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Plane.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Shapes/Frustum.h/.cpp/.inl` | Create |
| `Dia/DiaGeometry3D/Testing/Geometry3DShapeFactory.h` | Create |
| `Dia/DiaGeometry3D/Docs/dia.geometry3d.architecture.module.md` | Create |
| `Dia/DiaGeometry3D/DiaGeometry3D.vcxproj` | Create |
| `Dia/DiaGeometry3D/DiaGeometry3D.vcxproj.filters` | Create |
| `Cluiche/Cluiche.sln` | Modify — register DiaGeometry3D project |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestAABB.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestOOBB.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestSphere.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestCapsule.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestTriangle.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestCylinder.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestRay3D.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestPlane3D.cpp` | Create |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestFrustum.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` and `.filters` | Modify — add Geometry3D filter and link DiaGeometry3D |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaMaths — `Vector3D` (with `Cross`) | Every shape |
| DiaMaths — `Quaternion` | OOBB orientation; tests use Quaternion for rotated OOBB cases |
| DiaMaths — `Matrix44` | `Frustum::FromMatrix44` Gribb–Hartmann extraction |
| DiaMaths — `Angle` | None directly in shapes; tests use it for fov-based frustum construction |
| DiaCore — `DIA_ASSERT` | Construction-time invariants (Ray direction non-zero, AABB min ≤ max, Plane normal non-zero) |

**Order:** Implements after all DiaMaths 3D additions land. Vector3D::Cross, Quaternion, Matrix44 are hard prerequisites.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** Pure value types. |
| PD-004 | No STL containers in public API | **Compliant.** All public methods use POD or DiaMaths types. |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** New vcxproj/.filters created manually. |
| PD-007 | C++20 required | **Compliant.** |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj overrides. |
| AD-001 | Module YAML | **Compliant.** New `dia.geometry3d.architecture.module.md`. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** All in `Dia::Geometry3D::`. |
| SD-001 | `Dia::Geometry3D::` namespace | **Compliant — directly satisfies.** |
| SD-002 | Drop 3D suffix from type names | **Compliant — directly satisfies.** AABB, OOBB, Sphere, Capsule, Triangle, Cylinder, Ray, Plane, Frustum — no 3D suffix. |
| SD-005 | No STL in public API | **Compliant.** |
| SD-006 | Y-up RH | **Compliant.** Triangle::CalculateNormal right-handed; Plane::FromThreePoints right-handed; Frustum normals point inward (consistent with right-handed view-proj); test fixtures use Y-up RH frustums. |
| SD-009 | Quaternion-primary rotation, Matrix33 conversions | **Compliant.** OOBB stores orientation as Quaternion; provides `GetAxes()` for matrix-style axis extraction (system spec AI Review Q8). |
| SD-013 | IntersectionClassify duplicated, not shared | **Compliant — directly satisfies.** Lives in `Dia/DiaGeometry3D/Shapes/IntersectionClassify.h`. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Cylinder | System spec Q9 left Cylinder optional. Why include in v1? | User direction during feature interview: include in v1. Real use cases (trigger volumes, mechanical/CAD shapes) exist; cost is low (one more shape with the same pattern); deferring would force a follow-on spec for marginal saving. |
| 2 | Shape methods vs central `IntersectionTests` | Why both? AC 6 puts `IsIntersecting(point)` on the shape; the next feature adds the same to `IntersectionTests`. | Mirror of DiaGeometry2D's pattern (AARect.h:32). Shape-method point-in-shape is the hot path for SpatialGrid3D::QueryPoint, which queries thousands of shapes per frame — going through the static class adds an unnecessary call hop. The full pairwise matrix lives in `IntersectionTests` to avoid header explosion. |
| 3 | Frustum normal direction | Why inward-pointing? | Standard convention for culling: a point/AABB is inside the frustum iff it is on the +normal side of every plane. Easier to read and faster than the alternative. Documented prominently in Frustum.h. |
| 4 | Frustum::FromMatrix44 | Why Gribb–Hartmann specifically? | Standard, well-documented technique that extracts six planes directly from a view-projection matrix without additional input. Works for any GL-style matrix (which is our `Matrix44::Perspective` per `diamaths.md` AI Review Q10). One pass, deterministic. |
| 5 | Triangle winding | Counter-clockwise → +normal. Is that right-handed? | Yes — standard right-handed convention (matches OpenGL default front-face winding). Asserted in tests via `CalculateNormal` of a known triangle. |
| 6 | Default constructors | What does `AABB()` produce? Empty? Inverted? Zero-volume at origin? | Zero-volume at origin: `min == max == (0,0,0)`. Predictable and safe. Callers that want "empty for Encapsulate" pattern can use `AABB::Empty()` factory if needed; not in v1. |
| 7 | `Encapsulate` mutator | The 2D `AARect` doesn't have Encapsulate. Why add it here? | Real ergonomic need for AABB and Sphere — building a bounding volume from a stream of points is a top-3 operation. Adding it to AABB and Sphere only (Capsule / Cylinder / OOBB are not naturally growable). |
| 8 | Plane representation | Why `(normal, d)` and not `(point, normal)` or `(a, b, c, d)`? | `(normal, d)` is the most computationally direct — `dot(normal, p) - d` is the signed distance, which is the most common operation. Factories for the other forms (`FromPointAndNormal`, `FromThreePoints`) cover construction needs. |
| 9 | OOBB axis getter | `GetAxes(outX, outY, outZ)` returns three Vector3Ds. Why not return a Matrix33? | Mirrors what the consumer typically wants (SAT axis extraction iterates the three axes). Returning Matrix33 forces the consumer to extract columns. Three separate vectors is the cheaper API for the dominant use case. Add a `GetOrientationMatrix() -> Matrix33` later if a consumer needs it. |
| 10 | Frustum AABB bound | Should Frustum expose a `CalculateAABB()` derived from its 8 corner vertices? | Yes — useful for the `QueryFrustum` AABB-prefilter optimisation in SpatialGrid3D (per system Q7). Add as a method. Update AC 19 mentally; explicitly listed. |
| 11 | Equality comparison | What does `Triangle::operator==` mean — vertices in order, or unordered? | In order. `Triangle(a, b, c) == Triangle(a, b, c)` only. A re-ordering Triangle that compares unordered would mask winding bugs. |
| 12 | Encapsulate semantics | `AABB::Encapsulate(point)` — is the post-condition `min ≤ point ≤ max` for every axis? | Yes — `mMin = ComponentMin(mMin, point)`, `mMax = ComponentMax(mMax, point)`. Tests assert on canonical inputs. |
| 13 | Test fixtures | What canonical builders go in `Geometry3DShapeFactory.h`? | `MakeUnitAABB()` (min=(-1,-1,-1), max=(1,1,1)), `MakeUnitSphere()`, `MakeAxisAlignedTriangle()`, `MakeIdentityFrustum()` (cube as 6 planes), `MakeXZPlane()`, `MakeYAxisCapsule(length)`. Used by feature tests and downstream consumers. |
| 14 | Approval gate | Anything blocking? | Hard prerequisite: DiaMaths feature specs implemented (Vector3D::Cross, Quaternion, Matrix44 minimum). Spec can be Approved on its own merits. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). First feature in DiaGeometry3D's implementation chain.
