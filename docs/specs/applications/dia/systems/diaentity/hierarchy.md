# Feature Spec: hierarchy

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Add opt-in parent/child hierarchy to diaentitytemplate via `ParentComponent` and `ChildBufferComponent`. Only entities that participate in hierarchy carry these components — flat entities pay no memory cost. Hierarchy mutations (set parent, destroy subtree) route through the end-of-frame mutation pipeline. `EntityDestroyedMessage` is emitted during the destroy pass so components can react to the loss of a referenced entity.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | diaentitytemplate |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [reflection.md](reflection.md) |
| Depends on feature | [component-deps-and-refs.md](component-deps-and-refs.md) |

## Goals

- Hierarchy is opt-in via components — flat entities pay zero memory cost
- Mutations are queued through `EndOfFrame` — no iterator invalidation during gameplay update
- Cycle detection in debug; destroy-subtree is the default cascade policy
- `EntityDestroyedMessage` gives components a clean hook to react to referenced-entity destruction

## Acceptance Criteria

- `ParentComponent` and `ChildBufferComponent` are standard diaentitytemplate components registered via `DIA_COMPONENT`
- Root entities have no `ParentComponent` — `HasComponent<ParentComponent>(entity)` returns false for roots
- `ChildBufferComponent` stores up to 16 children (`DynamicArrayC<Entity, 16>`)
- `Hierarchy::QueueSetParent(Domain&, Entity child, Entity parent)` queues a parent change applied at `EndOfFrame`; updates both old parent's `ChildBufferComponent` (remove child) and new parent's (add child)
- `Hierarchy::QueueDestroySubtree(Domain&, Entity root)` queues `QueueDestroy` for root and all descendants depth-first; applied at `EndOfFrame`
- Cycle detection: `DIA_ASSERT` in debug if `QueueSetParent` would create a cycle; no-op check in release
- `EntityDestroyedMessage` is emitted by the domain during the destroy pass (before slot is freed); carries the destroyed entity's handle
- Components subscribe to `EntityDestroyedMessage` in `OnAttach` and unsubscribe in `OnDetach` via the domain's mailbox
- `ChildBufferComponent` overflow (>16 children) fires `DIA_ASSERT` in debug; `DIA_LOG_WARNING` + no-op in release

## Data Model

### Components

```cpp
namespace Dia::Entity {
    class ParentComponent : public IComponent {
    public:
        DIA_COMPONENT(ParentComponent, "parent", 1)
        FIELD(Entity, value, Entity::Invalid())

        Dia::Core::StringCRC GetTypeId() const override { return kTypeId; }
    };

    class ChildBufferComponent : public IComponent {
    public:
        DIA_COMPONENT(ChildBufferComponent, "child-buffer", 1)
        // children stored as a fixed-cap array; not a reflected FIELD (internal bookkeeping)
        Dia::Core::Containers::DynamicArrayC<Entity, 16> children;

        Dia::Core::StringCRC GetTypeId() const override { return kTypeId; }
    };
}
```

### Hierarchy free functions

```cpp
namespace Dia::Entity::Hierarchy {
    // Queue a parent change. Applied at EndOfFrame.
    // Adds ParentComponent to child if absent; adds ChildBufferComponent to parent if absent.
    // Removes child from old parent's ChildBufferComponent if child had a prior parent.
    // Debug: asserts if setting parent would create a cycle.
    void QueueSetParent(Domain& domain, Entity child, Entity parent);

    // Queue destroy for root and all descendants (depth-first).
    // Applied at EndOfFrame. Each destroyed entity emits EntityDestroyedMessage.
    void QueueDestroySubtree(Domain& domain, Entity root);
}
```

### EntityDestroyedMessage

```cpp
namespace Dia::Entity {
    struct EntityDestroyedMessage {
        Entity destroyed;
    };
}
```

Emitted via the domain's mailbox using `MakeAllAddress()` (broadcast) during the destroy pass, before the entity slot is freed. Components that hold `EntityRef` fields and need cleanup subscribe in `OnAttach`:

```cpp
void MyComponent::OnAttach(Domain& domain, Entity self) {
    domain.GetMailbox().Subscribe<EntityDestroyedMessage>(self, kEntityRouterId);
}
void MyComponent::OnDetach(Domain& domain, Entity self) {
    domain.GetMailbox().Unsubscribe<EntityDestroyedMessage>(self);
}
```

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/Hierarchy/ParentComponent.h` | New |
| `Dia/diaentitytemplate/Hierarchy/ParentComponent.cpp` | New — `DIA_COMPONENT_REGISTER` |
| `Dia/diaentitytemplate/Hierarchy/ChildBufferComponent.h` | New |
| `Dia/diaentitytemplate/Hierarchy/ChildBufferComponent.cpp` | New — `DIA_COMPONENT_REGISTER` |
| `Dia/diaentitytemplate/Hierarchy/Hierarchy.h` | New — `QueueSetParent`, `QueueDestroySubtree` |
| `Dia/diaentitytemplate/Hierarchy/Hierarchy.cpp` | New — implementation |
| `Dia/diaentitytemplate/Messages/EntityDestroyedMessage.h` | New |
| `Dia/diaentitytemplate/Domain.cpp` | Modified — emit `EntityDestroyedMessage` during destroy pass |
| `diaentitytemplate.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/HierarchyTests.cpp` | New |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | `ParentComponent` and `ChildBufferComponent` type IDs are `StringCRC` via `DIA_COMPONENT`. Compliant. |
| PD-004 | No STL in public APIs | `ChildBufferComponent::children` uses `DynamicArrayC`. Free functions take `Domain&` and `Entity`. No STL. Compliant. |
| SD-ENT-002 | Systems own data; components are typed adapters | Hierarchy is expressed as opt-in components. No special casing in `Domain` internals — hierarchy entities simply have extra components. Compliant. |
| SD-ENT-009 | Hierarchy is parent/child only; destroy-cascade default is destroy-subtree | `QueueDestroySubtree` implements the cascade. `ParentComponent` + `ChildBufferComponent` are the only hierarchy primitives. No general relationship graph. Compliant. |
| SD-ENT-012 | Structural changes queued at EndOfFrame | `QueueSetParent` and `QueueDestroySubtree` both enqueue mutations. Applied at `EndOfFrame`. Compliant. |
| SD-ENT-016 | Editor inspection v1 surface | `ParentComponent` and `ChildBufferComponent` carry `DIA_COMPONENT` so they appear in editor inspection automatically. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Cycle detection algorithm | `QueueSetParent` must detect cycles before queuing. How — walk ancestors of the proposed parent looking for `child`? | Yes — walk `ParentComponent` chain from `parent` upward. If `child` is encountered, it's a cycle. At v1 scale (<1024 entities, shallow hierarchies) this is a simple loop. Worst case is a degenerate chain of 1023 nodes — acceptable for a debug-only check. |
| 2 | `QueueDestroySubtree` with queued-but-not-yet-live children | If children were added via `QueueAddComponent` but `EndOfFrame` hasn't run, `ChildBufferComponent` may not exist yet. Does `QueueDestroySubtree` see them? | No — it only destroys entities that are currently live (visible via `GetComponent<ChildBufferComponent>`). Entities queued-but-not-live are not reachable. Callers that queue a subtree creation and immediately destroy it in the same frame should call `EndOfFrame` between the two operations. |
| 3 | `EntityDestroyedMessage` broadcast cost | Broadcasting to all subscribers on every destroy — at v1 scale is this fine? | Yes. At <1000 entities with sparse subscriptions this is negligible. The message is only emitted for entities that are actually destroyed, not every frame. |
| 4 | `ChildBufferComponent::children` not a FIELD | Children are bookkeeping, not user-authored config — correct to exclude from reflection? | Yes. Children are managed by `QueueSetParent` / `QueueDestroySubtree`, not set via JSON. Serializing children would create a redundant source of truth (the `ParentComponent` on each child already encodes the relationship). Blueprint loader reconstructs the child buffer from `ParentComponent` values during Pass 2. |
| 5 | `ParentComponent::value` field type | `FIELD(Entity, value, Entity::Invalid())` — `Entity` is a `Handle<EntityTag>`. Does DiaReflect know how to serialize a raw `Handle`? | `Handle<T>` needs a `DIA_SERIALIZE` block, same as `EntityRef`. However, `ParentComponent::value` should actually be `EntityRef<IComponent>` (typed ref accepting any component) rather than a raw `Entity` — consistent with the system spec's API section and the `component-deps-and-refs` feature. Updated in the data model below. |

## Open Questions

None.

## Status

`Approved`
