# Feature Spec: Spatial Queries

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **spatial-queries** |

**Status:** `Approved`

---

## Problem Statement

Game code needs to ask spatial questions about the physics world without triggering full collision detection: "What bodies are in this region?", "What is the first body a ray hits?", "What bodies are within this explosion radius?". These are read-only queries that delegate to the broad-phase spatial structure and optionally confirm via narrow-phase.

---

## Solution Overview

`PhysicsWorld` exposes three query methods: `Raycast` (first hit, narrow-phase confirmed), `QueryRegion` (AARect broad-phase), and `QueryCircle` (circle broad-phase). All queries use the injected `ISpatialStructure` for the initial candidate set, then optionally confirm with narrow-phase `IntersectionTests`. Results are written into caller-provided output buffers.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Raycast` returns the closest body the ray intersects; `outHit` populated | Unit test |
| AC2 | `Raycast` returns false when no body intersected | Unit test |
| AC3 | `Raycast` skips bodies whose broad AABB the ray misses (narrow-phase not called) | Unit test with mock shape |
| AC4 | `QueryRegion` returns all bodies whose narrow shapes overlap the region | Unit test |
| AC5 | `QueryRegion` does not return bodies whose broad-phase passes but narrow-phase fails | Unit test |
| AC6 | `QueryCircle` returns all bodies within the circle | Unit test |
| AC7 | All query methods return nothing when world has no bodies | Unit test |
| AC8 | Queries work correctly immediately after `AddBody`/`RemoveBody` | Unit test |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

struct RaycastHit {
    Body2DBase*                    body     = nullptr;  // Cast to PointBody2D* or RigidBody2D* if needed
    Dia::Geometry2D::ContactResult contact;             // normal, depth, hit point
    float                          distance = 0.0f;    // Distance from ray origin to hit point
};

// On PhysicsWorld:
bool Raycast(const Dia::Geometry2D::Ray&    ray,    RaycastHit& outHit)                                    const;
void QueryRegion(const Dia::Geometry2D::AARect& region, Dia::Core::DynamicArrayC<Body2DBase*>& out)        const;
void QueryCircle(const Dia::Geometry2D::Circle& circle, Dia::Core::DynamicArrayC<Body2DBase*>& out)        const;

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Raycast

```cpp
bool PhysicsWorld::Raycast(ray, outHit) {
    broadPhase->QueryRay(ray, candidates)
    closest = FLT_MAX
    for each candidate handle:
        body = broadPhase->Resolve(handle)
        ContactResult r = IntersectionTests::Test(ray, *body->GetNarrowShape())
        if r.hasContact and r.depth < closest:
            closest = r.depth
            outHit = { body, r, r.depth }
    return outHit.body != nullptr
}
```

### QueryRegion / QueryCircle

1. Query broad-phase for candidate handles
2. For each candidate, run narrow-phase `IntersectionTests::Test` against actual shape
3. If contact confirmed, add body pointer to output array
4. Deduplicate (body can appear in multiple broad-phase cells)

---

## Dependencies

### Required Features
- **physics-world** — hosts the query methods and broad-phase pointer
- **physics-body** — `Body2DBase*`; shape pointers for narrow-phase confirmation
- **DiaGeometry2D / intersection-tests** — `Test(Ray, shape)`, `Test(AARect, shape)`, `Test(Circle, shape)`
- **DiaGeometry2D / spatial-grid** — `ISpatialStructure::QueryRay`, `QueryRegion`, `QueryCircle`

### Required Modules
- **DiaCore** — `DynamicArrayC`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestSpatialQueries.cpp`)

1. Raycast hits single body — correct hit, distance, normal
2. Raycast hits nearest of two overlapping bodies — correct body returned
3. Raycast misses all bodies — returns false
4. QueryRegion — only bodies overlapping region returned
5. QueryCircle — only bodies within circle radius returned
6. Empty world — all queries return empty/false
7. Body removed — queries no longer return it

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ `DynamicArrayC` output |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-002 | System | Broad-phase via injected ISpatialStructure | ✅ Queries delegate to injected structure |
