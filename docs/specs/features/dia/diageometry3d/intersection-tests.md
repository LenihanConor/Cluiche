# Feature Spec: 3D Intersection Tests

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaGeometry3D | @docs/specs/systems/dia/diageometry3d.md |
| Feature | 3D Intersection Tests | (this document) |

## Summary

Add `Dia::Geometry3D::IntersectionTests` — a static class with `Test(...)` overloads for pairwise shape intersection in 3D. Returns `IntersectionClassify { kNoIntersection, kPenetrating, kAContainsB, kBContainsA }`.

V1 prioritises the **culling driver pairs** (per system SD-014): AABB/Sphere/Triangle vs Frustum, ray casts, AABB-vs-AABB, Sphere-vs-Sphere, AABB-vs-Sphere. OOBB SAT and Triangle-vs-Sphere ship as fast paths. Capsule, Cylinder, and full GJK+EPA are deferred to follow-on feature specs.

Mirrors the static-class pattern from `Dia::Geometry2D::IntersectionTests` (per `diageometry2d.md` SD-003).

## Problem

Without pairwise intersection tests, `SpatialGrid3D` cannot implement `QueryRegion`, `QuerySphere`, `QueryRay`, or `QueryFrustum` — the entire spatial-query surface depends on these primitives. Renderer culling, the priority driver, specifically requires AABB-vs-Frustum at scale. Without it, no consumer can build on top of DiaGeometry3D.

## Acceptance Criteria

### General

1. All tests live in `Dia::Geometry3D::IntersectionTests` static class. Files: `Dia/DiaGeometry3D/Intersection/IntersectionTests.h` and `.cpp`.
2. Each `Test()` overload returns `IntersectionClassify`. Symmetric pairs (e.g. `Test(AABB, Sphere)` and `Test(Sphere, AABB)`) are provided as separate overloads delegating to a shared implementation; result handling swaps `kAContainsB` ↔ `kBContainsA` (per `diageometry2d.md` AI Review Q2).
3. `Contains(shape, Vector3D point)` and `ClosestPoint(shape, Vector3D point)` overloads exist for each closed-volume shape (AABB, OOBB, Sphere, Capsule, Cylinder, Frustum, Triangle's plane).
4. **No STL** in any signature.
5. **Allocation-free** — no heap allocation in any `Test()` body. SAT axis tests use stack-allocated arrays.

### Priority pairs (V1 — must ship)

6. **`Test(AABB, Frustum)`** — per-plane test against frustum's six planes; conservative (returns `kPenetrating` for partial overlap; `kNoIntersection` only when the AABB is fully outside one plane). Used by SpatialGrid3D::QueryFrustum.
7. **`Test(Sphere, Frustum)`** — sphere center vs each plane with radius tolerance; same containment classification semantics.
8. **`Test(Triangle, Frustum)`** — three-vertex test against six planes. Returns `kPenetrating` if any vertex is inside or any plane crosses the triangle.
9. **`Test(AABB, AABB)`** — six-axis SAT. Containment classification (`kAContainsB`, `kBContainsA`) when one fully contains the other.
10. **`Test(AABB, Sphere)`** + symmetric — sphere center's closest point to AABB; distance ≤ radius is intersection. Containment classification when sphere fully contains AABB or vice versa.
11. **`Test(Sphere, Sphere)`** — center-distance vs sum-of-radii. Containment classification when one's distance + radius ≤ the other's radius.
12. **`Test(Ray, AABB)`** — slab method (Kay & Kajiya). Returns `kPenetrating` if the ray hits the AABB; never `kAContainsB` / `kBContainsA` (a ray can't contain a volume and vice versa).
13. **`Test(Ray, Sphere)`** — analytic quadratic. Same containment caveat as ray-AABB.
14. **`Test(Ray, Triangle)`** — Möller–Trumbore. Same containment caveat.
15. **`Test(Ray, Plane)`** — line-plane intersection. Returns `kPenetrating` if intersection is on the ray (t ≥ 0); `kNoIntersection` if parallel or behind origin.

### Fast-path pairs (V1 — ship if implementation cost is low)

16. **`Test(AABB, OOBB)`** + symmetric — full SAT (15 axes: 3 from AABB, 3 from OOBB, 9 cross products). Containment classification.
17. **`Test(OOBB, OOBB)`** — full SAT (15 axes). Containment classification.
18. **`Test(Triangle, AABB)`** + symmetric — Akenine-Möller's triangle-box test. Standard for mesh culling.
19. **`Test(Triangle, Sphere)`** + symmetric — closest point on triangle to sphere center; distance ≤ radius.

### Deferred to follow-on feature specs

20. Capsule pairs, Cylinder pairs, full pairwise OOBB-vs-others (SAT), full GJK+EPA. Documented as out-of-scope.

### Containment / closest-point queries

21. `Contains(AABB, Vector3D)` — per-axis test.
22. `Contains(Sphere, Vector3D)` — squared distance.
23. `Contains(OOBB, Vector3D)` — transform point into OOBB's local space (via inverse Quaternion), then per-axis half-extents test.
24. `Contains(Frustum, Vector3D)` — point on +normal side of every plane.
25. `Contains(Capsule, Vector3D)`, `Contains(Cylinder, Vector3D)` — distance from point to axis line segment, vs radius (capsule includes spherical caps).
26. `ClosestPoint(AABB, Vector3D)` — per-axis clamp.
27. `ClosestPoint(Sphere, Vector3D)` — center + (point - center).AsNormal() * radius.
28. `ClosestPoint(OOBB, Vector3D)` — local-space clamp + Quaternion::Rotate back.
29. `ClosestPoint(Triangle, Vector3D)` — barycentric closest point on triangle (handles all 7 regions).
30. `ClosestPoint(Capsule, Vector3D)`, `ClosestPoint(Cylinder, Vector3D)` — closest point on axis segment + cap/radius offset.

### Project file integration

31. New files registered in `DiaGeometry3D.vcxproj` and `.vcxproj.filters` under `Intersection` filter.
32. `dia.geometry3d.architecture.module.md` updated to list `IntersectionTests` in `public_api.entry_points`.

## API Design

```cpp
// Dia/DiaGeometry3D/Intersection/IntersectionTests.h
namespace Dia::Geometry3D {

class IntersectionTests
{
public:
    // Bounding-volume vs Frustum (priority — culling driver)
    static IntersectionClassify Test(const AABB&     a, const Frustum& b);
    static IntersectionClassify Test(const Sphere&   a, const Frustum& b);
    static IntersectionClassify Test(const Triangle& a, const Frustum& b);
    static IntersectionClassify Test(const Frustum& a, const AABB&     b);
    static IntersectionClassify Test(const Frustum& a, const Sphere&   b);
    static IntersectionClassify Test(const Frustum& a, const Triangle& b);

    // Bounding-volume pairs
    static IntersectionClassify Test(const AABB&   a, const AABB&   b);
    static IntersectionClassify Test(const AABB&   a, const Sphere& b);
    static IntersectionClassify Test(const Sphere& a, const AABB&   b);
    static IntersectionClassify Test(const Sphere& a, const Sphere& b);
    static IntersectionClassify Test(const AABB&   a, const OOBB&   b);
    static IntersectionClassify Test(const OOBB&   a, const AABB&   b);
    static IntersectionClassify Test(const OOBB&   a, const OOBB&   b);

    // Triangle-vs-volume (mesh culling)
    static IntersectionClassify Test(const Triangle& a, const AABB&   b);
    static IntersectionClassify Test(const AABB&     a, const Triangle& b);
    static IntersectionClassify Test(const Triangle& a, const Sphere& b);
    static IntersectionClassify Test(const Sphere&   a, const Triangle& b);

    // Ray casts
    static IntersectionClassify Test(const Ray& a, const AABB&     b);
    static IntersectionClassify Test(const Ray& a, const Sphere&   b);
    static IntersectionClassify Test(const Ray& a, const Triangle& b);
    static IntersectionClassify Test(const Ray& a, const Plane&    b);

    // Point containment
    static bool Contains(const AABB&     shape, const Dia::Maths::Vector3D& point);
    static bool Contains(const Sphere&   shape, const Dia::Maths::Vector3D& point);
    static bool Contains(const OOBB&     shape, const Dia::Maths::Vector3D& point);
    static bool Contains(const Capsule&  shape, const Dia::Maths::Vector3D& point);
    static bool Contains(const Cylinder& shape, const Dia::Maths::Vector3D& point);
    static bool Contains(const Frustum&  shape, const Dia::Maths::Vector3D& point);

    // Closest point
    static Dia::Maths::Vector3D ClosestPoint(const AABB&     shape, const Dia::Maths::Vector3D& point);
    static Dia::Maths::Vector3D ClosestPoint(const Sphere&   shape, const Dia::Maths::Vector3D& point);
    static Dia::Maths::Vector3D ClosestPoint(const OOBB&     shape, const Dia::Maths::Vector3D& point);
    static Dia::Maths::Vector3D ClosestPoint(const Triangle& shape, const Dia::Maths::Vector3D& point);
    static Dia::Maths::Vector3D ClosestPoint(const Capsule&  shape, const Dia::Maths::Vector3D& point);
    static Dia::Maths::Vector3D ClosestPoint(const Cylinder& shape, const Dia::Maths::Vector3D& point);
};

}  // namespace Dia::Geometry3D
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaGeometry3D/Intersection/IntersectionTests.h` | Class declaration |
| 2 | Implement priority pairs (ACs 6–15) | Test(AABB, Frustum), Test(Sphere, Frustum), Test(Triangle, Frustum), Test(AABB,AABB), Test(AABB,Sphere)+sym, Test(Sphere,Sphere), Ray casts |
| 3 | Implement fast-path pairs (ACs 16–19) | OOBB SAT pairs, Triangle-vs-AABB Akenine-Möller, Triangle-vs-Sphere |
| 4 | Implement Contains + ClosestPoint (ACs 21–30) | Per-shape point queries |
| 5 | Update `dia.geometry3d.architecture.module.md` | Add `IntersectionTests` |
| 6 | Update `DiaGeometry3D.vcxproj` and `.vcxproj.filters` | Add Intersection filter |
| 7 | Add tests | `Cluiche/Tests/GoogleTests/Geometry3D/TestIntersectionTests.cpp` |
| 8 | Run `dia run googletest --filter="IntersectionTests*"` | All green |

## Test Plan (Task 7)

| Suite | Tests |
|-------|-------|
| `IntersectionAABBFrustumTest` | AABB fully inside frustum → kBContainsA; AABB outside one plane → kNoIntersection; AABB straddling a plane → kPenetrating; frustum fully inside AABB → kAContainsB |
| `IntersectionSphereFrustumTest` | Sphere fully inside; sphere fully outside one plane; sphere straddling; tangent to a plane (radius == distance) |
| `IntersectionTriangleFrustumTest` | Triangle fully inside; fully outside; one vertex inside; plane crosses edge |
| `IntersectionAABBAABBTest` | Disjoint, touching, overlapping, A contains B, B contains A, identical |
| `IntersectionAABBSphereTest` | Sphere center inside AABB → kPenetrating; sphere fully outside; sphere tangent; sphere fully contains AABB; AABB fully contains sphere |
| `IntersectionSphereSphereTest` | Disjoint, tangent (sum of radii == distance), overlapping, A contains B, identical |
| `IntersectionAABBOOBBTest` | Axis-aligned OOBB equals AABB-AABB result; rotated OOBB intersection cases (15-axis SAT) |
| `IntersectionOOBBOOBBTest` | Same orientation = AABB-AABB result after rotation; differing orientations stress all 15 axes |
| `IntersectionTriangleAABBTest` | Triangle fully inside AABB; outside; edge-on-face; vertex-on-face |
| `IntersectionTriangleSphereTest` | Sphere center inside triangle plane and inside triangle; sphere center outside triangle but within radius of an edge; sphere far away |
| `IntersectionRayAABBTest` | Ray entering AABB; missing AABB; starting inside AABB (always hit); parallel to a face |
| `IntersectionRaySphereTest` | Ray hitting through center; tangent; missing; starting inside |
| `IntersectionRayTriangleTest` | Standard hit; missing; ray parallel to triangle plane (no hit); ray coplanar (degenerate, kNoIntersection by convention) |
| `IntersectionRayPlaneTest` | Standard hit; ray parallel to plane (no hit); ray pointing away (no hit); ray on plane (kNoIntersection by convention) |
| `ContainsTest` | Point-in-AABB / Sphere / OOBB / Capsule / Cylinder / Frustum: inside, outside, on-boundary cases for each |
| `ClosestPointTest` | AABB clamp; Sphere center+normal*r; OOBB local-space clamp; Triangle barycentric all 7 regions; Capsule/Cylinder axis projection |

## Files

| File | Action |
|------|--------|
| `Dia/DiaGeometry3D/Intersection/IntersectionTests.h` | Create |
| `Dia/DiaGeometry3D/Intersection/IntersectionTests.cpp` | Create |
| `Dia/DiaGeometry3D/Docs/dia.geometry3d.architecture.module.md` | Modify — add IntersectionTests |
| `Dia/DiaGeometry3D/DiaGeometry3D.vcxproj` and `.vcxproj.filters` | Modify — add Intersection filter |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestIntersectionTests.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` and `.filters` | Modify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaGeometry3D — Shape Primitives (this system, separate feature) | All 9 shape types as inputs |
| DiaMaths — Vector3D (with Cross), Quaternion, Matrix44 | Vector ops, OOBB local-space transforms via Quaternion::Inverse, etc. |
| DiaCore — DIA_ASSERT | Preconditions (e.g. ray direction non-zero) |

**Order:** Implements after `shape-primitives` (same system spec). Cannot land before shapes exist.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** |
| PD-004 | No STL in public API | **Compliant.** All Test/Contains/ClosestPoint signatures use POD or DiaGeometry3D/DiaMaths types. |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** |
| PD-007 | C++20 required | **Compliant.** |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** |
| AD-001 | Module YAML | **Compliant.** Architecture doc updated. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** `Dia::Geometry3D::IntersectionTests`. |
| SD-001 | `Dia::Geometry3D::` namespace | **Compliant.** |
| SD-003 | Unified static `Test()` overloads | **Compliant — directly satisfies.** Mirrors `diageometry2d.md` SD-003. |
| SD-005 | No STL in public API | **Compliant.** |
| SD-006 | Y-up RH | **Compliant.** Frustum normals inward (consistent with RH); Triangle winding right-handed. |
| SD-009 | Quaternion-primary rotation | **Compliant.** OOBB tests use Quaternion::Inverse() for local-space transforms. |
| SD-013 | IntersectionClassify duplicated, not shared | **Compliant.** Uses `Dia::Geometry3D::IntersectionClassify`. |
| SD-014 | First-round priority pairs are culling-driven | **Compliant — directly satisfies.** ACs 6–15 are the priority set; ACs 16–19 are fast-path additions; Capsule / Cylinder pairs and GJK+EPA are deferred. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Symmetry | Why provide both `Test(AABB, Sphere)` and `Test(Sphere, AABB)` instead of relying on argument-order conventions? | `diageometry2d.md` AI Review Q2 settled this — provide both as overloads delegating to a shared implementation, swapping containment classification. Saves callers from having to remember which order is canonical. |
| 2 | Containment classification for ray casts | A `Ray` is half-infinite — can't be contained nor contain anything. Ray Test() returns only kPenetrating / kNoIntersection? | Yes. Documented in API. The `IntersectionClassify` enum values `kAContainsB` / `kBContainsA` are unused for ray pairs. |
| 3 | Frustum-vs-AABB containment | When does Test(AABB, Frustum) return `kBContainsA` vs `kAContainsB`? | `kBContainsA` when AABB is fully inside the frustum (every AABB corner is on +normal side of every plane). `kAContainsB` when the frustum is fully inside the AABB (every frustum corner is inside the AABB — requires computing the frustum's 8 corners). Tests verify both. |
| 4 | OOBB SAT cost | 15-axis SAT for OOBB-OOBB is the textbook approach but expensive. Is there a cheaper option for the common case? | No reliable shortcut without losing accuracy. Identical-orientation OOBBs reduce to AABB-AABB after rotation, but checking that condition is its own cost. Stick with full SAT; profile if it becomes a hotspot. |
| 5 | Triangle-vs-AABB | Akenine-Möller's test or simple SAT? | Akenine-Möller. Faster and standard for mesh culling. The reference paper is freely available. |
| 6 | Coplanar ray-triangle | Möller–Trumbore returns "no intersection" for rays parallel to the triangle plane (denominator zero). Is that desired? | Yes. A ray coplanar with a triangle has infinitely many intersection points; no useful single result. `kNoIntersection` is the safe answer. |
| 7 | Fast paths | Should Test(AABB, AABB) early-exit when one is empty (zero volume)? | Yes for `kNoIntersection`, but careful about containment classification — a zero-volume AABB at a point inside another AABB is technically `kBContainsA`. Match the `min ≤ max` invariant established in shape-primitives AC 11; tests cover. |
| 8 | Capsule / Cylinder | Why deferred? | Same answer as system spec SD-014: the priority is rendering culling. Capsule and Cylinder pair tests are needed only when a consumer (physics, gameplay) actually arrives. Adding them speculatively risks dead code and longer test surface. |
| 9 | Exception safety | What happens on degenerate inputs (zero-radius sphere, AABB with min > max)? | Constructor-time invariants (shape-primitives AC) ensure inputs are valid. `IntersectionTests::Test` does NOT re-validate; trusts the type system. Asserts at the source of any degenerate construction (in shape ctors). |
| 10 | Approval gate | Anything blocking? | Hard prerequisite: shape-primitives feature implementation. Spec can be Approved on its own merits and committed in the same window. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review).
