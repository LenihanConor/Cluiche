# Feature Spec: Intersection Tests

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diageometry2d.md | **intersection-tests** |

**Status:** `Approved`

---

## Problem Statement

`IntersectionClassify` and `IntersectionTests` currently live in `Dia/DiaMaths/Shape/Common/` under `Dia::Maths::`. The API is incomplete (many pairwise tests stubbed/commented out), covers only 9 shape types, and returns only a classify enum — giving callers no depth or normal data. DiaRigidBody2D needs contact normals and penetration depth for collision response. The shape set is expanding to 13 types, and adding each new shape under an N×N model scales poorly.

---

## Solution Overview

Migrate to `Dia/DiaGeometry2D/Intersection/`. Adopt a **hybrid GJK + EPA + fast-path** approach:

- **GJK (Gilbert-Johnson-Keerthi)** — general narrow-phase for all convex shape pairs; returns a `ContactResult` (normal, depth, contact point)
- **EPA (Expanding Polytope Algorithm)** — used after GJK detects penetration to extract depth and normal
- **Hand-optimised fast paths** — Circle-Circle, AARect-AARect, Circle-AARect (most common pairs; faster than GJK)
- **Dedicated implementations** — Plane, Ray, Line, Arc, Sector, AnnularSector (non-convex or degenerate shapes; GJK does not apply)

`IntersectionClassify` is kept as a lightweight boolean-classify return for callers that only need overlap detection. `ContactResult` (defined in shape-primitives) is returned when depth/normal is needed.

This feature depends on **shape-primitives** being complete first.

### GJK Convex Set
Circle, AARect, OORect, Triangle, Capsule, ConvexPolygon, Point

### Dedicated (non-convex / degenerate)
Plane, Ray, Line, Arc, Sector, AnnularSector

### Key Design Points

1. **Unified static API** — `IntersectionTests::Test(a, b)` returns `ContactResult`; `IntersectionTests::Classify(a, b)` returns `IntersectionClassify` for callers that don't need depth/normal
2. **Symmetric overloads** — both orderings provided; containment direction flips (`kAContainsB` ↔ `kBContainsA`), normal negates
3. **GJK support function** — each convex shape implements `Support(direction)` returning the furthest point in that direction; GJK calls this generically
4. **Fast paths** — Circle-Circle, AARect-AARect, Circle-AARect bypass GJK; same `ContactResult` return type
5. **Complete coverage** — no commented stubs; Ellipse removed from scope

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Files exist in `Dia/DiaGeometry2D/Intersection/` | File system check |
| AC2 | All types in `Dia::Geometry2D::` namespace | Grep |
| AC3 | No commented-out stubs in any Intersection source file | Code review |
| AC4 | `Test()` overloads exist for all convex-vs-convex pairs (GJK path) | Code review |
| AC5 | Fast paths exist for Circle-Circle, AARect-AARect, Circle-AARect | Code review |
| AC6 | Dedicated implementations exist for Plane, Ray, Line, Arc, Sector, AnnularSector vs all applicable shapes | Code review |
| AC7 | `ContactResult` returned by `Test()` includes correct normal, depth, contact point for penetrating pairs | Unit tests |
| AC8 | `Classify()` returns correct `IntersectionClassify` enum consistent with `Test().hasContact` | Unit tests |
| AC9 | Symmetric overloads return flipped containment and negated normal | Unit tests |
| AC10 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| AC11 | All unit tests pass | `UnitTests.exe` |

---

## Public API

```cpp
namespace Dia::Geometry2D {

// Lightweight classify-only result (for callers that don't need depth/normal)
enum class IntersectionClassify {
    kNoIntersection,
    kPenetrating,
    kAContainsB,
    kBContainsA
};

// Full contact result (ContactResult defined in ContactResult.h / shape-primitives feature)
// struct ContactResult { bool hasContact; Vector2D normal; float depth; Vector2D point; };

class IntersectionTests {
public:
    // --- Full contact results (GJK + EPA for convex pairs; fast paths for hot pairs) ---

    // Convex vs convex (GJK path unless fast-path exists)
    static ContactResult Test(const Circle& a,        const Circle& b);        // fast path
    static ContactResult Test(const Circle& a,        const AARect& b);        // fast path
    static ContactResult Test(const AARect& a,        const AARect& b);        // fast path
    static ContactResult Test(const Circle& a,        const OORect& b);
    static ContactResult Test(const Circle& a,        const Triangle& b);
    static ContactResult Test(const Circle& a,        const Capsule& b);
    static ContactResult Test(const Circle& a,        const ConvexPolygon& b);
    static ContactResult Test(const AARect& a,        const OORect& b);
    static ContactResult Test(const AARect& a,        const Triangle& b);
    static ContactResult Test(const AARect& a,        const Capsule& b);
    static ContactResult Test(const AARect& a,        const ConvexPolygon& b);
    static ContactResult Test(const OORect& a,        const OORect& b);
    static ContactResult Test(const OORect& a,        const Triangle& b);
    static ContactResult Test(const OORect& a,        const Capsule& b);
    static ContactResult Test(const OORect& a,        const ConvexPolygon& b);
    static ContactResult Test(const Triangle& a,      const Triangle& b);
    static ContactResult Test(const Triangle& a,      const Capsule& b);
    static ContactResult Test(const Triangle& a,      const ConvexPolygon& b);
    static ContactResult Test(const Capsule& a,       const Capsule& b);
    static ContactResult Test(const Capsule& a,       const ConvexPolygon& b);
    static ContactResult Test(const ConvexPolygon& a, const ConvexPolygon& b);

    // All symmetric overloads (swap a/b, negate normal, flip kAContainsB/kBContainsA)
    static ContactResult Test(const AARect& a,        const Circle& b);
    // ... (all reverse orderings)

    // Dedicated: Plane (half-space tests)
    static ContactResult Test(const Plane& a,  const Circle& b);
    static ContactResult Test(const Plane& a,  const AARect& b);
    static ContactResult Test(const Plane& a,  const OORect& b);
    static ContactResult Test(const Plane& a,  const Triangle& b);
    static ContactResult Test(const Plane& a,  const Capsule& b);
    static ContactResult Test(const Plane& a,  const ConvexPolygon& b);

    // Dedicated: Ray casts (returns first hit; ContactResult.point = hit position)
    static ContactResult Test(const Ray& a,    const Circle& b);
    static ContactResult Test(const Ray& a,    const AARect& b);
    static ContactResult Test(const Ray& a,    const OORect& b);
    static ContactResult Test(const Ray& a,    const Triangle& b);
    static ContactResult Test(const Ray& a,    const Capsule& b);
    static ContactResult Test(const Ray& a,    const ConvexPolygon& b);
    static ContactResult Test(const Ray& a,    const Plane& b);
    static ContactResult Test(const Ray& a,    const Line& b);

    // Dedicated: Line segment tests
    static ContactResult Test(const Line& a,   const Circle& b);
    static ContactResult Test(const Line& a,   const AARect& b);
    static ContactResult Test(const Line& a,   const Line& b);

    // Dedicated: Angular shapes
    static ContactResult Test(const Arc& a,           const Circle& b);
    static ContactResult Test(const Arc& a,           const AARect& b);
    static ContactResult Test(const Sector& a,        const Circle& b);
    static ContactResult Test(const Sector& a,        const AARect& b);
    static ContactResult Test(const Sector& a,        const ConvexPolygon& b);
    static ContactResult Test(const AnnularSector& a, const Circle& b);
    static ContactResult Test(const AnnularSector& a, const AARect& b);
    static ContactResult Test(const AnnularSector& a, const ConvexPolygon& b);

    // --- Classify-only (no depth/normal; cheaper for broad overlap queries) ---
    template<typename A, typename B>
    static IntersectionClassify Classify(const A& a, const B& b);

    // --- Point containment ---
    static bool Contains(const Circle& shape,        const Dia::Maths::Vector2D& point);
    static bool Contains(const AARect& shape,        const Dia::Maths::Vector2D& point);
    static bool Contains(const OORect& shape,        const Dia::Maths::Vector2D& point);
    static bool Contains(const Triangle& shape,      const Dia::Maths::Vector2D& point);
    static bool Contains(const Capsule& shape,       const Dia::Maths::Vector2D& point);
    static bool Contains(const ConvexPolygon& shape, const Dia::Maths::Vector2D& point);
    static bool Contains(const Sector& shape,        const Dia::Maths::Vector2D& point);
    static bool Contains(const AnnularSector& shape, const Dia::Maths::Vector2D& point);
    // Plane: use SignedDistance(). Line/Ray: no area, not applicable.

    // --- Closest point queries ---
    static Dia::Maths::Vector2D ClosestPoint(const Circle& shape,        const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const AARect& shape,        const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const OORect& shape,        const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const Line& shape,          const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const Ray& shape,           const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const Triangle& shape,      const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const Capsule& shape,       const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const ConvexPolygon& shape, const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const Sector& shape,        const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const AnnularSector& shape, const Dia::Maths::Vector2D& p);
    static Dia::Maths::Vector2D ClosestPoint(const Plane& shape,         const Dia::Maths::Vector2D& p);
};

} // namespace Dia::Geometry2D
```

---

## Implementation Notes

### GJK Overview

GJK works on any convex shape by calling a `Support(direction)` function — the furthest point in a given direction. Each convex shape implements this:

```cpp
// Support functions (internal, not public API)
Vector2D Support(const Circle& shape, const Vector2D& dir);
Vector2D Support(const AARect& shape, const Vector2D& dir);
Vector2D Support(const OORect& shape, const Vector2D& dir);
// ... etc.

// Generic GJK entry point (internal)
ContactResult GJK_EPA(
    SupportFn supportA, const void* shapeA,
    SupportFn supportB, const void* shapeB);
```

### Fast Path Pattern

```cpp
ContactResult IntersectionTests::Test(const Circle& a, const Circle& b) {
    // Fast path — skip GJK entirely
    Vector2D delta = b.GetCenter() - a.GetCenter();
    float dist = delta.Length();
    float radii = a.GetRadius() + b.GetRadius();
    if (dist >= radii) return ContactResult{};  // no contact
    ContactResult r;
    r.hasContact = true;
    r.normal     = (dist > 0.0f) ? delta / dist : Vector2D(0, 1);
    r.depth      = radii - dist;
    r.point      = a.GetCenter() + r.normal * a.GetRadius();
    return r;
}
```

### Symmetric Overload Pattern

```cpp
ContactResult IntersectionTests::Test(const AARect& a, const Circle& b) {
    ContactResult r = Test(b, a);       // Call primary
    r.normal = -r.normal;               // Negate normal
    // kAContainsB/kBContainsA flip handled in Classify() wrapper
    return r;
}
```

### Algorithm Summary

| Pair category | Algorithm |
|---|---|
| Convex vs convex | GJK + EPA |
| Circle-Circle | Fast path: center distance vs radius sum |
| AARect-AARect | Fast path: overlap on 2 axes + depth from minimum overlap axis |
| Circle-AARect | Fast path: closest point on AABB to circle center |
| Plane vs any | Signed distance from plane; depth = -signed distance if negative |
| Ray vs convex | Slab method (AABB); parametric intersection for others |
| Capsule | Reduce to closest point on Line segment, then Circle test |
| Arc / Sector / AnnularSector | Angular band + radial band intersection |

### File Layout

```
Dia/DiaGeometry2D/Intersection/
├── IntersectionClassify.h
├── ContactResult.h          (or included from Shapes/ContactResult.h)
├── IntersectionTests.h
├── IntersectionTests.cpp    (fast paths + GJK dispatch)
├── GJK.h / GJK.cpp          (GJK + EPA implementation, internal)
└── SupportFunctions.h       (per-shape support functions, internal)
```

---

## Dependencies

### Required Features (must be complete first)
- **shape-primitives** — all 13 shape types and `ContactResult` must exist

### Required Modules
- **DiaMaths** — `Vector2D`, `Angle`, `Matrix33`
- **DiaCore** — `DIA_ASSERT`

### Dependent Features
- **spatial-grid**, **quadtree**, **bvh** — use `AARect` bounds queries (indirect)
- **DiaRigidBody2D / collision-detection** — uses `Test()` for narrow-phase; uses `ContactResult` for response

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/Geometry2D/TestIntersectionTests.cpp`)

For each shape pair:
1. **No contact** — separated shapes → `hasContact == false`
2. **Penetrating** — overlapping → `hasContact == true`, `depth > 0`, `normal` unit length
3. **Touching** — exactly touching → `hasContact == true`, `depth == 0`
4. **Contact point** — verify `point` lies on boundary of shape A
5. **Symmetric** — `Test(b, a).normal == -Test(a, b).normal`; `depth` equal

Priority order: Circle-Circle, Circle-AARect, AARect-AARect (physics uses these first).

### GJK Regression
- Verified against known analytic results for Circle-OORect, Triangle-Triangle, ConvexPolygon-ConvexPolygon
- Degenerate cases: zero-area polygon (1 or 2 vertices) — assert, not silent failure

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | ✅ All parameters/returns are value types |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ `Dia::Geometry2D::` |
| SD-003 | System | Unified static `Test()` overloads | ✅ Implemented here |
| SD-006 | System | Complete all stubs | ✅ No commented stubs; non-applicable pairs documented as N/A |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | GJK | GJK requires convex shapes only. What happens if a non-convex shape is accidentally passed to a GJK overload? | Only convex shapes have GJK overloads. Non-convex shapes (Arc, Sector, AnnularSector) only have dedicated overloads — no GJK dispatch path exists for them. The type system enforces this. |
| 2 | EPA convergence | EPA can fail to converge on degenerate inputs. How is this handled? | Cap iterations at 32; if not converged, return last best estimate with a `DIA_ASSERT` in debug builds. Physics engine should not rely on exact depth for micro-penetrations. |
| 3 | ContactResult | Should `ContactResult` carry multiple contact points (contact manifold)? | Single point for v1 — sufficient for most game collision response. Multi-point manifold is a DiaRigidBody2D concern; the physics feature spec can extend this. |
| 4 | Classify template | The `Classify<A,B>()` template calls `Test()` and discards depth/normal. Is this acceptable overhead? | Yes — `Classify()` is for non-physics callers (visibility, triggers). If performance matters, callers can call `Test().hasContact` directly. |
| 5 | AnnularSector tests | AnnularSector vs ConvexPolygon is complex. Should it be deferred? | Implement AnnularSector-Circle and AnnularSector-AARect for v1 (covers vision cone use cases). AnnularSector-ConvexPolygon can be added when a concrete use case requires it; stub with `DIA_ASSERT(false, "Not implemented")` until then. |
