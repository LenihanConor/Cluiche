# Feature Spec: Collision Layers & Masks

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarigidbody2d.md | **collision-layers** |

**Status:** `Approved`

---

## Problem Statement

Game code needs selective collision filtering — projectiles that pass through allies, player characters that ignore trigger volumes, enemies that don't collide with each other but still hit walls and the player. Without a filtering mechanism every body collides with every other body and game code must post-filter collision events, which is expensive and error-prone.

---

## Solution Overview

Both `PointBody2D` and `RigidBody2D` carry two `uint32_t` bitmasks via `Body2DBase`: `layer` (what group this body belongs to, exactly one bit set) and `mask` (which layers this body should collide with, one bit per layer). Filtering is bilateral: a pair (A, B) is tested only if `(A.mask & B.layer) != 0 AND (B.mask & A.layer) != 0`. This enables asymmetric setups (e.g. a ghost body that ignores everything but is still detected by sensors).

Filtering is applied in narrow-phase, after the broad-phase candidate list is built, before `IntersectionTests::Test` is called. Collision events are only emitted for pairs that pass the filter.

Up to 32 distinct layers are supported (one bit each in a `uint32_t`).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Two bodies with compatible masks collide normally | Unit test |
| AC2 | Two bodies where A's mask does not include B's layer — no contact produced, no event emitted | Unit test |
| AC3 | Bilateral check: A accepts B but B does not accept A — no contact | Unit test |
| AC4 | Default `layer = 1, mask = 0xFFFFFFFF` — body collides with everything | Unit test |
| AC5 | Body with `mask = 0` collides with nothing | Unit test |
| AC6 | Filtering applied before narrow-phase — `IntersectionTests::Test` not called for filtered pairs | Unit test with spy/counter |
| AC7 | Collision event not emitted for filtered pair | Unit test: subscribe to events; verify no event for filtered pair |
| AC8 | Up to 32 distinct layer bits usable without overflow | Compile + unit test: set all 32 bits |
| AC9 | Layer / mask changeable at runtime via `SetLayer` / `SetMask` | Unit test: change mask mid-simulation, verify filtering changes |
| AC10 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::RigidBody2D {

// Convenience constants — game code defines its own layer values
namespace Layers {
    constexpr uint32_t kNone       = 0;           // No layer — body never collides with anything
    constexpr uint32_t kDefault    = 1 << 0;
    constexpr uint32_t kPlayer     = 1 << 1;
    constexpr uint32_t kEnemy      = 1 << 2;
    constexpr uint32_t kProjectile = 1 << 3;
    constexpr uint32_t kTrigger    = 1 << 4;
    constexpr uint32_t kAll        = 0xFFFFFFFF;
}

// layer and mask added to both PointBodyDef and RigidBodyDef (see physics-body spec)
// struct PointBodyDef { ...; uint32_t layer = Layers::kDefault; uint32_t mask = Layers::kAll; };
// struct RigidBodyDef { ...; uint32_t layer = Layers::kDefault; uint32_t mask = Layers::kAll; };

// GetLayer/GetMask/SetLayer/SetMask live on Body2DBase (shared by both types)
// See physics-body spec for full Body2DBase API

// Free function used by DetectCollisions — also useful for game code pre-filtering
// Accepts Body2DBase& so it works for both PointBody2D and RigidBody2D without cast
inline bool ShouldCollide(const Body2DBase& a, const Body2DBase& b) {
    return (a.GetMask() & b.GetLayer()) != 0
        && (b.GetMask() & a.GetLayer()) != 0;
}

} // namespace Dia::RigidBody2D
```

---

## Implementation Notes

### Narrow-Phase Filter Insertion

In `DetectCollisions()`, after building the candidate pair list from the broad-phase and before calling `IntersectionTests::Test`:

```cpp
for each candidate pair (A, B):
    if !ShouldCollide(*A, *B): continue   // skip — filtered out
    ContactResult r = IntersectionTests::Test(...)
    ...
```

### Why Bilateral

One-way filtering (only A's mask checked) would make layer setup asymmetric and error-prone — "I collide with you" but "you don't collide with me" produces inconsistent contact normals and event pairs. Bilateral is unambiguous: both parties must agree.

### Layer Convention

`layer` should have exactly one bit set (identifies which group a body is in), or `0` (`Layers::kNone`) for a body that never participates in collisions. `mask` is a bitmask of which groups to collide with. Enforced by `DIA_ASSERT(layer == 0 || (layer & (layer - 1)) == 0)` — zero or power-of-two check.

### Runtime Changes

`SetLayer` / `SetMask` take effect from the next `DetectCollisions` pass. No rebuild of the broad-phase required — filtering happens after broad-phase candidate retrieval.

---

## Dependencies

### Required Features
- **physics-body** — `Body2DBase` provides `layer`, `mask`, `SetLayer()`, `SetMask()`, `GetLayer()`, `GetMask()`; both `PointBodyDef` and `RigidBodyDef` have `layer`/`mask` fields
- **collision-detection** — `ShouldCollide()` filter inserted before narrow-phase
- **collision-events** — events only emitted for pairs that pass `ShouldCollide()`

### Required Modules
- **DiaCore** — `DIA_ASSERT`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/RigidBody2D/TestCollisionLayers.cpp`)

1. Default bodies — collide with each other (both kDefault layer, kAll mask)
2. Player (layer=kPlayer, mask=kAll) vs Enemy (layer=kEnemy, mask=kAll) — collide
3. Projectile (mask excludes kProjectile) vs Projectile — no contact
4. Ghost (mask=0) vs anything — no contact
5. Asymmetric: A accepts B's layer, B does not accept A's layer — no contact
6. Filter does not call IntersectionTests — verify via step-through or body position unchanged
7. No event emitted for filtered pair — subscribe, step, verify empty
8. SetMask at runtime — pair that was filtered now collides after mask change
9. 32-layer setup — bodies on layer 32 (bit 31) collide correctly

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | A body has `layer = 0` (no layer bit set). `ShouldCollide` would always return false regardless of masks. Is this a valid use case, or should `layer = 0` be treated as an error? | Valid and intentional — add `Layers::kNone = 0` as an explicit constant so the intent is self-documenting. The `DIA_ASSERT` power-of-two check fires only for values with multiple bits set; `kNone` (zero bits) is exempt as a deliberate "ghost/no-collide" designation. |
| 2 | The `DIA_ASSERT((layer & (layer - 1)) == 0)` power-of-two check is described for `layer`. Should the same assertion also fire in `SetLayer()` at runtime, or only in the constructor? | Both — assert fires in the constructor (via `Body2DBase` initialisation from the Def) and in `SetLayer()`. Runtime changes are just as likely to be buggy as construction-time values. |
| 3 | With 32 layers and many bodies, `kAll = 0xFFFFFFFF` as the default mask means every new body collides with everything by default. Is this the right ergonomic default, or should it default to `kDefault` only (mask = 1)? | `kAll` is correct — every new body collides with everything unless explicitly filtered. Game code only needs to set layer/mask when it wants exceptions. Matches Box2D ergonomics; opt-in filtering is less error-prone than opt-out. |
| 4 | `ShouldCollide` is `inline` in the header and takes `const Body2DBase&`. Can game code call this same function to pre-filter before adding bodies to a scene, or is it intended only for use inside `DetectCollisions`? | Intentionally public. It is a pure function with no side effects; game code can use it to pre-filter query results, debug layer configuration, or check pairs before spawning. Exposing it avoids game code having to re-implement the bilateral check. |
| 5 | When a sleeping body wakes via broad-phase overlap, the pair still needs to pass `ShouldCollide` before narrow-phase runs. Does the broad-phase wake happen before or after the `ShouldCollide` check? | `ShouldCollide` runs first. If the pair is layer-filtered, the sleeping body stays asleep — waking it for a collision that will never resolve breaks the performance contract of sleeping. Order in `DetectCollisions`: broad-phase candidate pair → `ShouldCollide` check → if filtered, skip (no wake) → if not filtered and one body is sleeping, call `Wake()` → narrow-phase. |
| 6 | Is there a way to check at runtime whether a layer value has been "claimed" (to help game code avoid accidentally reusing layer bits)? | No engine support — entirely the game code's responsibility. The `Layers::` namespace constants are provided as a starting point; projects define their own constants in their own namespace. |

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL in public APIs | ✅ Plain `uint32_t`; no STL introduced |
| PD-007 | Platform | C++20 required | ✅ `constexpr` Layers:: constants and `inline bool ShouldCollide` — all C++20-compliant |
| AD-002 | Dia App | No STL in public APIs | ✅ Reinforces PD-004 |
| AD-003 | Dia App | Namespace | ✅ `Dia::RigidBody2D::` and `Dia::RigidBody2D::Layers::` |
| SD-001 | System | Fixed timestep + accumulator | ✅ `ShouldCollide()` called inside `DetectCollisions()`, which runs inside `StepOnce()` — fully integrated with fixed step |
| SD-002 | System | Broad-phase injected via ISpatialStructure | ✅ Filtering applied after broad-phase candidate retrieval, before narrow-phase — consistent with injected broad-phase contract |
| SD-004 | System | Collision events via ObserverSubject | ✅ Collision events are only emitted for pairs that pass `ShouldCollide()` — no events for filtered pairs |
| SD-006 | System | No STL in public API | ✅ Reinforces PD-004 |
| SD-009 | System | Static + kinematic body types | ✅ Layer/mask live on `Body2DBase`; `ShouldCollide(Body2DBase&, Body2DBase&)` works for static, kinematic, and dynamic bodies without special-casing |
| SD-011 | System | Bilateral layer+mask bitmask | ✅ `ShouldCollide()` checks both directions — `(A.mask & B.layer) != 0 && (B.mask & A.layer) != 0` |
