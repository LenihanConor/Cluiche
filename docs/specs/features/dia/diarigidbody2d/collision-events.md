# Feature Spec: Collision Events

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **collision-events** |

**Status:** `Approved`

---

## Problem Statement

Game code needs to react to collisions — deal damage, play sounds, trigger pickups, start animations. The physics engine must notify listeners when two bodies begin overlapping (Enter), continue overlapping (Stay), or stop overlapping (Exit), without coupling to DiaApplication's MessageBus.

---

## Solution Overview

After each step, compare the current frame's contact list against the previous frame's active pair set (stored in `PhysicsWorld`) to classify each pair as Enter, Stay, or Exit. Emit `CollisionEvent` structs via `Dia::Core::ObserverSubject<CollisionEvent>` on `PhysicsWorld`. Listeners subscribe once and receive all events.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | First frame two bodies overlap — `kEnter` event emitted | Unit test |
| AC2 | Second consecutive frame overlap — `kStay` event emitted (not `kEnter` again) | Unit test |
| AC3 | Bodies separate — `kExit` event emitted on first non-overlapping frame | Unit test |
| AC4 | Bodies separate then re-collide — new `kEnter` event on re-contact | Unit test |
| AC5 | Listener receives correct `bodyA`, `bodyB`, `contact` in event | Unit test |
| AC6 | Kinematic-dynamic collision emits events on both bodies | Unit test |
| AC7 | Static-dynamic collision emits events | Unit test |
| AC8 | Unsubscribing listener no longer receives events | Unit test |
| AC9 | No events emitted when no contacts exist | Unit test |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

enum class CollisionEventType { kEnter, kStay, kExit };

struct CollisionEvent {
    CollisionEventType           type;
    Body2DBase*                  bodyA;  // PointBody2D* or RigidBody2D*; no vtable call needed
    Body2DBase*                  bodyB;
    Dia::Geometry2D::ContactResult contact;  // Valid for Enter/Stay; zeroed for Exit
};

// Usage — subscribe via PhysicsWorld:
//   world.GetCollisionEvents().AddObserver(&myListener);
//
// Listener interface (DiaCore Observer pattern):
//   class MyListener : public Dia::Core::Observer<CollisionEvent> {
//       void OnNotify(const CollisionEvent& e) override { ... }
//   };

// Internal to PhysicsWorld::StepOnce
void EmitCollisionEvents(
    const Dia::Core::DynamicArrayC<Contact>&                    currentContacts,
    Dia::Core::HashTable<BodyPairKey, CollisionPairState>&       activePairs,
    Dia::Core::ObserverSubject<CollisionEvent>&                  subject);

struct BodyPairKey {
    // Canonical ordering: lower pointer address first — ensures (A,B) == (B,A)
    // Body2DBase* covers both PointBody2D* and RigidBody2D* without a vtable call
    const Body2DBase* first;
    const Body2DBase* second;
    bool operator==(const BodyPairKey&) const;
};

struct CollisionPairState {
    bool             wasActive;
    Dia::Geometry2D::ContactResult lastContact;
};

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Enter / Stay / Exit Classification

```cpp
void EmitCollisionEvents(currentContacts, activePairs, subject) {
    // Mark all existing pairs as "not seen this step"
    for each pair in activePairs: pair.wasActive = false

    // Process current contacts
    for each contact in currentContacts:
        key = MakePairKey(contact.bodyA, contact.bodyB)
        if activePairs.Contains(key):
            activePairs[key].wasActive = true
            subject.Notify({ kStay, bodyA, bodyB, contact.result })
        else:
            activePairs.Insert(key, { true, contact.result })
            subject.Notify({ kEnter, bodyA, bodyB, contact.result })

    // Emit Exit for pairs no longer active
    for each pair in activePairs where !wasActive:
        subject.Notify({ kExit, pair.bodyA, pair.bodyB, {} })
        activePairs.Remove(pair)
}
```

### BodyPairKey Canonicalisation

Always store the lower pointer address as `first` to ensure `(A,B)` and `(B,A)` produce the same key:
```cpp
BodyPairKey MakePairKey(Body2DBase* a, Body2DBase* b) {
    return { min(a, b), max(a, b) };
}
```

---

## Dependencies

### Required Features
- **collision-detection** — `Contact` list (current frame contacts)
- **physics-world** — owns `activePairs` and `ObserverSubject`
- **physics-body** — `Body2DBase*` in events; `BodyPairKey` canonicalisation

### Required Modules
- **DiaCore** — `ObserverSubject`, `Observer`, `HashTable`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestCollisionEvents.cpp`)

1. Enter on first overlap — listener receives `kEnter`
2. Stay on continued overlap — listener receives `kStay`, not second `kEnter`
3. Exit on separation — listener receives `kExit`
4. Re-collision after exit — new `kEnter`
5. Multiple listeners — all receive the same events
6. Unsubscribe — removed listener does not receive events
7. No contacts — no events emitted
8. Kinematic hits dynamic — both bodies present in event

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ `HashTable`, `DynamicArrayC` |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` |
| SD-004 | System | Observer-based collision events | ✅ Implemented here |
