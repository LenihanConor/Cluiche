# Feature Spec: update-loop

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Implement `Domain::Update(dt)` — the per-frame tick that walks all DoUpdate-opted components in a deterministic order. Components opt in explicitly via a `DIA_UPDATABLE` macro in their class body, which sets `kFlagOverridesDoUpdate` on their `ComponentTypeDesc`. Only opted-in types are walked; flat entities and non-updating components pay zero cost.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentitytemplate.md](../../systems/dia/diaentitytemplate.md) |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [reflection.md](reflection.md) |

## Goals

- Deterministic, predictable update order — no per-component scheduling
- Only components that declare `DIA_UPDATABLE` are walked — zero cost for non-updating types
- `Domain::Update` and `Domain::EndOfFrame` remain separate caller-driven entry points

## Acceptance Criteria

- `DIA_UPDATABLE` macro in a component class body sets `kFlagOverridesDoUpdate` on its `ComponentTypeDesc`
- `Domain::Update(dt)` walks all registered component types with `kFlagOverridesDoUpdate` set, in component-type-registration order
- Within each type, entities are walked in entity-index order
- `DoUpdate(domain, entity, dt)` called once per live component per frame
- Components queued-but-not-yet-live (pending `EndOfFrame`) are not walked
- Components queued for removal are still walked if not yet removed — removal is `EndOfFrame`
- `Domain::Update(dt)` does not call `EndOfFrame` — caller drives both separately
- A component type with no live instances is skipped with no iteration cost
- `dt` is passed through unchanged to each `DoUpdate` call

## Data Model

### DIA_UPDATABLE macro

```cpp
// In ComponentFoo.h, inside the class body alongside DIA_COMPONENT:
//
// class ComponentFoo : public Dia::Entity::IComponent {
// public:
//     DIA_COMPONENT(ComponentFoo, "foo", 1)
//     DIA_UPDATABLE   // sets kFlagOverridesDoUpdate in ComponentTypeDesc::flags
//
//     void DoUpdate(Domain&, Entity, float dt) override;
// };
//
// Expands to:
//   static constexpr uint16_t kFlagOverridesDoUpdate = true; // contributes to ComponentTypeDesc::flags at register time
```

### Domain::Update

```cpp
namespace Dia::Entity {
    class Domain {
    public:
        // ...existing API...

        // Walk all DIA_UPDATABLE component types in registration order,
        // entity-index order within each type. Does not call EndOfFrame.
        void Update(float dt);
    };
}
```

### Update walk (internal)

```cpp
// Pseudocode inside Domain::Update:
for each ComponentTypeDesc* desc in mComponentRegistry (registration order):
    if not (desc->flags & kFlagOverridesDoUpdate): continue
    ComponentPool* pool = FindPool(desc->typeId);
    if pool == nullptr: continue
    for each (entity, component*) in pool (entity-index order):
        if not IsAlive(entity): continue
        component->DoUpdate(*this, entity, dt);
```

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/ComponentMacros.h` | Modified — add `DIA_UPDATABLE` macro |
| `Dia/diaentitytemplate/Domain.h` | Modified — add `Update(float dt)` declaration |
| `Dia/diaentitytemplate/Domain.cpp` | Modified — implement `Update` walk |
| `Tests/GoogleTests/Entity/UpdateLoopTests.cpp` | New |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-004 | No STL in public APIs | `Domain::Update` takes a plain `float`. No STL. Compliant. |
| SD-ENT-015 | `Domain::Update(dt)` walks DoUpdate-opted components in registration order then entity-index order | Implemented exactly as specified. `DIA_UPDATABLE` is the explicit opt-in. Compliant. |
| SD-ENT-012 | Structural changes queued at EndOfFrame | `Update` does not apply mutations. Mutations queued during `DoUpdate` calls are batched until `EndOfFrame`. Compliant. |
| SD-ENT-018 | Single-threaded per domain | No locking in `Update`. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Update order stability | Registration order is static-init order of `DIA_COMPONENT_REGISTER` calls across TUs. Is this stable across builds? | Within a single build it is stable (linker order is deterministic for static libs). Across builds it may vary if TU link order changes. For v1 this is acceptable — the spec explicitly does not guarantee determinism beyond "registration order." Callers that need strict ordering own that responsibility (SD-ENT decision). |
| 2 | `DoUpdate` queuing mutations | A component's `DoUpdate` calls `Domain::QueueAddComponent` or `QueueDestroy`. Safe? | Yes — the mutation queue is separate from the update walk. Mutations accumulate during `Update` and are applied at the subsequent `EndOfFrame`. No iterator invalidation. |
| 3 | Non-alive entity in pool | Between `QueueDestroy` and `EndOfFrame`, the entity slot is still in the pool. The `IsAlive` check in the walk skips it. | Correct. The `IsAlive` guard is the only cost — one generation comparison per slot. Acceptable. |
| 4 | Empty `DIA_UPDATABLE` type | A type declares `DIA_UPDATABLE` but has zero live instances. Cost? | `FindPool` returns the pool (allocated on first attach). Pool iteration skips all slots (none alive). Cost is O(capacity) generation checks — ~1024 comparisons for an empty pool. If this becomes a concern, track a live-count on the pool and skip iteration when zero. Not a v1 concern. |

## Open Questions

None.

## Status

`Approved`
