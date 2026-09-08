# Feature Spec: behaviourtree-observer-bus-adapter

**System:** DiaBehaviourTree
**App:** Dia
**Status:** Approved

## Summary

`IBehaviourTreeEventListener`/`BehaviourTreeComponent::AddEventListener`/`RemoveEventListener` (Done) stays as-is. This feature adds a `BehaviourTreeBusAdapter : IBehaviourTreeEventListener` that forwards `OnNodeEntered`/`OnNodeCompleted`/`OnTreeCompleted` onto the shared `DiaMessageBus::Bus`. Unlike DiaEconomy (global `Broadcast`), this is inherently entity-scoped — each `BehaviourTreeComponent` belongs to one entity — so delivery uses `{Bus::kEntityRouterId, ownerEntity.bits}` per [entity-router-bus-wiring.md](../diaentity/entity-router-bus-wiring.md)'s addressing convention, not a broadcast.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaBehaviourTree — [diabehaviourtree.md](diabehaviourtree.md) (Done) |
| Depends on feature | [entity-router-bus-wiring.md](../diaentity/entity-router-bus-wiring.md) (Approved) — this feature's addressing depends on `EntityRouter` being live on the Bus |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` |

## Goals

- Other systems can subscribe to "this entity's tree finished / entered node X" without depending on DiaBehaviourTree directly (e.g. an animation system reacting to a tree-driven trigger).
- Delivery is addressed to the specific owning entity, not broadcast to every subscriber of the type.

## Non-Goals

- Changing `IBehaviourTreeEventListener`'s signatures for direct, same-module listeners.
- Solving entity-handle threading for every future per-component adapter pattern — this feature solves it for `BehaviourTreeComponent` specifically.

## Acceptance Criteria

- `BehaviourTreeBusAdapter` implements `IBehaviourTreeEventListener`; each callback calls `bus.Post<T>({Bus::kEntityRouterId, ownerEntity.bits}, msg)`, never `Broadcast`.
- The adapter knows its owning entity's handle at construction time (see Design — `BehaviourTreeComponent` does not currently expose this to a registered listener).
- `NodeEnteredEvent`/`NodeCompletedEvent`/`TreeCompletedEvent` are declared in a `.diagamemessages` file under `DiaBehaviourTree` and generated via `dia codegen messages`.
- Registering `BehaviourTreeBusAdapter` via `AddEventListener` does not prevent any other direct listener from also being registered on the same component.

## Design

### The entity-handle gap

`IBehaviourTreeEventListener`'s callbacks (`OnNodeEntered(StringCRC nodeId)`, `OnNodeCompleted(StringCRC, NodeResult)`, `OnTreeCompleted(NodeResult)`) carry no entity handle — by design, `BehaviourTreeComponent` doesn't know it needs to identify itself to its listeners, since existing listeners (e.g. debug logging) don't need it. But `BehaviourTreeBusAdapter` needs the owning entity's handle to construct `{kEntityRouterId, ownerEntity.bits}`.

Since one adapter instance is registered per `BehaviourTreeComponent` (components are per-entity), the handle can be bound at the adapter's construction time rather than threaded through the listener interface:

```cpp
// One adapter instance per BehaviourTreeComponent, constructed wherever
// the component is attached to an entity (whoever has the entity handle then).
class BehaviourTreeBusAdapter : public Dia::BehaviourTree::IBehaviourTreeEventListener {
public:
    BehaviourTreeBusAdapter(Dia::MessageBus::Bus& bus, Dia::Entity::Entity owner)
        : mBus(bus), mOwner(owner) {}

    void OnNodeEntered(Dia::Core::StringCRC nodeId) override {
        mBus.Post<NodeEnteredEvent>({Bus::kEntityRouterId, mOwner.bits()}, {nodeId});
    }
    void OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result) override {
        mBus.Post<NodeCompletedEvent>({Bus::kEntityRouterId, mOwner.bits()}, {nodeId, result});
    }
    void OnTreeCompleted(NodeResult result) override {
        mBus.Post<TreeCompletedEvent>({Bus::kEntityRouterId, mOwner.bits()}, {result});
    }

private:
    Dia::MessageBus::Bus& mBus;
    Dia::Entity::Entity   mOwner;
};
```

This avoids changing `IBehaviourTreeEventListener`'s signature (which would also affect any existing direct listener) — the entity identity lives in the adapter, not the interface.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaBehaviourTree/BehaviourTreeBusAdapter.h` / `.cpp` | New |
| `Dia/DiaBehaviourTree/Messages/behaviourtree_messages.diagamemessages` | New — three event types |
| `Dia/DiaBehaviourTree/DiaBehaviourTree.vcxproj` | Add new files; add `DiaMessageBus` reference |
| Wherever `BehaviourTreeComponent` is attached to an entity (game/stage code) | Construct and register `BehaviourTreeBusAdapter` with the entity handle at attach time |
| `Tests/GoogleTests/BehaviourTree/BehaviourTreeBusAdapterTests.cpp` | New |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | Event structs use `StringCRC`/`NodeResult` value types only. |
| SD-MBX2-003 | Sender identity irrelevant, only Address determines receivers | This is an Entity→System pattern — the component posts, addressed by its own entity, to whichever systems subscribed. |

## Open Design Questions

1. **Where does the entity handle actually get supplied to the adapter?** Whatever attaches `BehaviourTreeComponent` to an entity (blueprint loading, spawner, etc.) needs to also construct and register `BehaviourTreeBusAdapter` at that same point. Identify that call site before implementation.
2. **Does this depend on [entity-router-bus-wiring.md](../diaentity/entity-router-bus-wiring.md) being built first?** Yes — `kEntityRouterId` addressing only resolves to real subscribers once that feature ships. This feature can be implemented in parallel but can't be verified end-to-end until that one lands.

## Status

`Approved`
