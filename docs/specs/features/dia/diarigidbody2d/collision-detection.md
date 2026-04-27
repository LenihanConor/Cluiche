# Feature Spec: Collision Detection

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **collision-detection** |

**Status:** `Approved`

---

## Problem Statement

Each simulation step, the physics engine must determine which bodies are overlapping and compute the contact normal and penetration depth needed for impulse response. Testing every pair of bodies is O(n²); a broad-phase pass must first filter candidate pairs cheaply, then a precise narrow-phase test is run only on those candidates.

---

## Solution Overview

Two-phase detection each step:

1. **Broad-phase** — query the injected `ISpatialStructure<void*>` with each body's AABB to get candidate overlap pairs. Pairs involving two static bodies are skipped. Both `PointBody2D` and `RigidBody2D` pools are queried.
2. **Layer filter** — apply `ShouldCollide()` (bilateral layer/mask check) before narrow-phase; skip filtered pairs entirely.
3. **Narrow-phase** — call `Dia::Geometry2D::IntersectionTests::Test(shapeA, shapeB)` on each candidate pair. The returned `ContactResult` (normal, depth, contact point) is stored in a per-step contact list.

The contact list is consumed by collision response and collision events.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Two overlapping bodies produce a contact in the contact list | Unit test |
| AC2 | Two separated bodies produce no contact | Unit test |
| AC3 | Static-static pair is skipped (not in contact list) | Unit test |
| AC4 | Contact normal points from B toward A | Unit test: verify direction |
| AC5 | Penetration depth > 0 for overlapping pair | Unit test |
| AC6 | Broad-phase candidate list correct (no missed overlaps for tested scenes) | Unit test: grid of bodies |
| AC7 | Kinematic-static pair produces a contact | Unit test |
| AC8 | Contact list cleared each step before detection | Unit test: separate bodies after overlap; verify contact gone next step |
| AC9 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

// Contact stores typed pointers — the hot ResolveCollisions loop reads these
// directly without any indirection. Exactly one of pointBodyA / rigidBodyA is
// non-null per slot (same for B side).
struct Contact {
    PointBody2D* pointBodyA = nullptr;  // Non-null if bodyA is a PointBody2D
    RigidBody2D* rigidBodyA = nullptr;  // Non-null if bodyA is a RigidBody2D
    PointBody2D* pointBodyB = nullptr;
    RigidBody2D* rigidBodyB = nullptr;
    Dia::Geometry2D::ContactResult result;  // normal, depth, contact point

    // Convenience: get the Body2DBase* regardless of type (for event emission, pair keys)
    Body2DBase* GetBodyA() const;
    Body2DBase* GetBodyB() const;
};

// Internal to PhysicsWorld::StepOnce — not called directly by game code
void DetectCollisions(
    const Dia::Core::DynamicArrayC<PointBody2D*>&  pointBodies,
    const Dia::Core::DynamicArrayC<RigidBody2D*>&  rigidBodies,
    Dia::Geometry2D::ISpatialStructure<void*>*     broadPhase,
    Dia::Core::DynamicArrayC<Contact>&             outContacts);

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Broad-Phase

Both pools are iterated. Each body's pointer is cast to `void*` as the spatial structure key:

```cpp
// Both pools contribute to broad-phase queries
for each dynamic/kinematic body A in (pointBodies ∪ rigidBodies):
    broadPhase->QueryRegion(A->GetBroadShape()->GetWorldAABB(), rawCandidates)
    for each void* rawB in rawCandidates (where rawB != A):
        B = resolve rawB to Body2DBase*
        if already processed pair (A,B) this step: skip
        if A->GetBodyType() == kStatic && B->GetBodyType() == kStatic: skip
        if !ShouldCollide(*A, *B): skip   // layer/mask filter
        candidatePairs.Add({A, B})
```

Use a `HashTable<BodyPairKey, bool>` as a visited set to avoid duplicate pairs (A,B) and (B,A). `BodyPairKey` uses `Body2DBase*` (canonical lower-address ordering).

### Narrow-Phase

```cpp
for each candidate pair (A, B):
    ContactResult r = IntersectionTests::Test(*GetNarrowShape(A), *GetNarrowShape(B))
    if r.hasContact:
        outContacts.Add(MakeContact(A, B, r))  // fills typed pointer slots
```

Shape dispatch: each body has at most one narrow-phase shape (Circle or ConvexPolygon). `GetNarrowShape()` is a free function that accepts `Body2DBase*` and returns the appropriate shape pointer via if/else on the known type — no virtual call needed since `Contact` already knows the concrete type.

### Shape Selection Priority

1. If body has `circleShape` → use Circle
2. If body has `polyShape` → use ConvexPolygon
3. Fallback → use `broadShape` (AARect) — less accurate but always present

---

## Dependencies

### Required Features
- **physics-body** — `Body2DBase`, `PointBody2D`, `RigidBody2D`, shape pointers, `ShouldCollide()`
- **physics-world** — provides contact list storage and broad-phase pointer
- **DiaGeometry2D / intersection-tests** — `IntersectionTests::Test()`, `ContactResult`
- **DiaGeometry2D / spatial-grid** — `ISpatialStructure<T>`

### Required Modules
- **DiaCore** — `DynamicArrayC`, `HashTable`

### Dependent Features
- **collision-response** — consumes `Contact` list
- **collision-events** — consumes `Contact` list

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestCollisionDetection.cpp`)

1. Two circles overlapping — contact produced, normal correct, depth > 0
2. Two circles separated — no contact
3. Two static bodies overlapping — no contact in list
4. Circle vs ConvexPolygon overlap — contact produced
5. 10 bodies in a grid — broad-phase returns correct candidates; narrow-phase correct contacts
6. Moving body separates — contact gone next step

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ `DynamicArrayC<Contact>` |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-002 | System | Broad-phase via injected ISpatialStructure | ✅ |
