# Feature Spec: order-observer-bus-adapter

**System:** DiaOrder
**App:** Dia
**Status:** Approved

## Summary

`OrderQueue<TContext>`/`IOrderQueueObserver<TContext>` (Done) stays as-is — it's a generic template and remains one. This feature defines the pattern for bridging its notifications onto the shared `DiaMessageBus::Bus`, but — unlike DiaEconomy — the actual adapter can't live inside the generic `DiaOrder` module itself: `.diagamemessages`-declared bus message types are concrete structs, not templates, and `OnOrderStarted(const IOrder<TContext>&)` passes a polymorphic interface reference, which isn't copyable into a queued message. The migration has to happen at whichever application-layer site first instantiates a concrete `TContext`.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaOrder — [diaorder.md](diaorder.md) (Done) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` |

## Goals

- Define the concrete-type bridging pattern once, so any future `OrderQueue<TContext>` instantiation can reuse it instead of re-deriving the same design.
- Order events on the bus carry order *data* (an order ID plus whatever fields matter to subscribers), never a reference to the polymorphic `IOrder<TContext>` object.

## Non-Goals

- Modifying `DiaOrder` itself. The generic module has no bus dependency before or after this feature — bridging is entirely an application-layer concern.
- Picking `TContext` for the engine. That's determined by whoever first uses `OrderQueue` in a real game/stage.

## Acceptance Criteria

- A documented pattern (with a worked example against a placeholder concrete `TContext`) shows: an `IOrderQueueObserver<TContext>` implementation living at the application layer that extracts order data (`OrderId` or equivalent StringCRC, plus relevant fields the concrete `IOrder<TContext>` subtype exposes) and calls `Bus::Broadcast<T>()` with a plain struct — never forwarding the `IOrder<TContext>&` itself.
- `OnQueueEmpty()` (parameterless) maps directly to a bus broadcast with no payload beyond the queue's own identity, if the queue itself is identifiable (see open question).
- The pattern explicitly documents that this bridging observer is registered per concrete `OrderQueue<ConcreteContext>` instance, same as any other `IOrderQueueObserver<ConcreteContext>` — no generic/templated bus adapter is attempted.

## Design

### Why no generic adapter is possible

```cpp
// This does NOT work as a bus adapter — IOrder<TContext>& isn't a value
// type, and TContext isn't concrete, so there's no kTypeId to register.
void OnOrderStarted(const IOrder<TContext>& order) override { /* ??? */ }
```

Once a real game picks a concrete `TContext` (e.g. `struct UnitOrderContext { ... }`) and a concrete `IOrder<UnitOrderContext>` subtype (e.g. `MoveOrder`), the bridging observer extracts value data from that concrete subtype:

```cpp
// Application-layer, once TContext = UnitOrderContext is real:
class OrderBusAdapter : public Dia::Order::IOrderQueueObserver<UnitOrderContext> {
public:
    explicit OrderBusAdapter(Dia::MessageBus::Bus& bus) : mBus(bus) {}

    void OnOrderStarted(const IOrder<UnitOrderContext>& order) override {
        mBus.Broadcast(OrderStartedEvent{ order.GetOrderId(), /* concrete fields */ });
    }
    void OnOrderFinished(const IOrder<UnitOrderContext>& order) override {
        mBus.Broadcast(OrderFinishedEvent{ order.GetOrderId() });
    }
    void OnOrderCancelled(const IOrder<UnitOrderContext>& order) override {
        mBus.Broadcast(OrderCancelledEvent{ order.GetOrderId() });
    }
    void OnQueueEmpty() override {
        mBus.Broadcast(OrderQueueEmptyEvent{ /* queue identity, if any */ });
    }

private:
    Dia::MessageBus::Bus& mBus;
};
```

This assumes `IOrder<TContext>` exposes some kind of stable order identifier (`GetOrderId()` or equivalent) — needs confirming against the actual `IOrder.h` interface at implementation time.

## Files Touched

_None in `DiaOrder` itself._ Application-layer files, determined by the first real `TContext` consumer:

| File | Change |
|---|---|
| `<application>/Messages/order_messages.diagamemessages` | New — `OrderStartedEvent`/`OrderFinishedEvent`/`OrderCancelledEvent`/`OrderQueueEmptyEvent` |
| `<application>/OrderBusAdapter.h` / `.cpp` | New — concrete-`TContext` bridging observer |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | Bridging observer's event structs use `StringCRC`/plain value types only. |
| SD-MBX2-005 | Message schemas live in the application layer, not the engine | Directly matches — `DiaOrder` stays schema-free; the bridging observer and its message types live wherever `TContext` is concretized. |

## Open Design Questions

1. **Where is the first real `OrderQueue<TContext>` instantiation in the codebase?** This feature can't be fully concrete until that site is identified — grep for `OrderQueue<` usage before implementation. If none exists yet, this spec stays as a documented pattern until a real consumer needs it.
2. **Does `IOrder<TContext>` expose a stable order identifier today?** Needed for `OrderId` in the bus events. Check `IOrder.h` before finalizing the event struct shape.
3. **Does `OnQueueEmpty()` need a queue identity in its payload?** If a game only ever has one `OrderQueue` per entity, the entity's own handle (via `kEntityRouterId` addressing) may be the more natural fit than a broadcast — revisit once a real consumer exists, since this may want entity-addressed delivery like DiaBehaviourTree rather than `Broadcast`.

## Status

`Approved`
