# Feature Spec: economy-observer-bus-adapter

**System:** DiaEconomy
**App:** Dia
**Status:** Approved

## Summary

`EconomyObserverSubject`/`IEconomyObserver` (Done) stays exactly as it is — direct, synchronous in-process observers remain legal. This feature adds one new concrete observer, `EconomyBusAdapter : IEconomyObserver`, that forwards each notification onto the shared `DiaMessageBus::Bus` via `Bus::Broadcast<T>()`. It also fixes a correctness gap the existing event structs have that only matters once delivery becomes deferred: `PoolChangedEvent`, `TransactionClampedEvent`, and `TransferCompletedEvent` currently carry a raw `const EconomyInstance*`/reference into the *live* instance. That's safe today because `Notify()` is an immediate synchronous call. Once a copy of the message sits in a bus ring buffer until the next Primary pass, that pointer can be stale or dangling by the time a handler runs.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaEconomy — [diaeconomy.md](diaeconomy.md) (Done) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` (`Bus::Broadcast`) |

## Goals

- Gameplay systems can subscribe to economy events via `Bus::Subscribe<T>` without DiaEconomy taking any dependency on DiaMessageBus itself in its core types.
- Event payloads are safe to queue and deliver on a later pass — no raw pointers/references to mutable live state.
- Existing direct `IEconomyObserver` consumers (if any exist today, e.g. `DiaEconomyInspector`) are not broken by the payload redesign.

## Non-Goals

- Changing `EconomyObserverSubject`'s Subscribe/Unsubscribe/Notify mechanism itself.
- Deciding module update ordering platform-wide — this feature only flags the same-tick-vs-next-tick question for whoever owns the Economy module.

## Acceptance Criteria

- `PoolChangedEvent`, `TransactionClampedEvent`, `TransferCompletedEvent` are redesigned to carry snapshot values (resource name, relevant amounts) instead of `const EconomyInstance*`/`&`. No pointer/reference to live, mutable state in any bus-forwarded payload.
- `OnPoolReachedMaximum`/`OnPoolReachedMinimum` (currently non-struct overloads taking `const EconomyInstance&`) get equivalent value-carrying structs (`PoolReachedMaximumEvent`, `PoolReachedMinimumEvent`) so they can also be forwarded.
- `EconomyBusAdapter : IEconomyObserver` implements all five notifications, each calling `bus.Broadcast<T>(...)` with the corresponding (now-safe) event struct.
- All five event types are declared in a `.diagamemessages` file under `DiaEconomy` and generated via `dia codegen messages`.
- `EconomyBusAdapter` is registered as one more subscriber via `EconomyObserverSubject::Subscribe`, alongside any other direct observers — it does not replace them.
- Whatever currently implements `IEconomyObserver` directly (if anything) continues to compile and function against the redesigned event structs.

## Design

### Adapter shape (same pattern as flush-adapters, inverted)

```cpp
// Lives in DiaEconomy — forwards synchronous Notify() calls onto the bus.
class EconomyBusAdapter : public Dia::Economy::IEconomyObserver {
public:
    explicit EconomyBusAdapter(Dia::MessageBus::Bus& bus) : mBus(bus) {}

    void OnPoolChanged(const PoolChangedEvent& e) override { mBus.Broadcast(e); }
    void OnTransactionClamped(const TransactionClampedEvent& e) override { mBus.Broadcast(e); }
    void OnTransferCompleted(const TransferCompletedEvent& e) override { mBus.Broadcast(e); }
    void OnPoolReachedMaximum(const PoolReachedMaximumEvent& e) override { mBus.Broadcast(e); }
    void OnPoolReachedMinimum(const PoolReachedMinimumEvent& e) override { mBus.Broadcast(e); }

private:
    Dia::MessageBus::Bus& mBus;
};
```

`IEconomyObserver`'s two non-struct overloads (`OnPoolReachedMaximum(const EconomyInstance&, StringCRC)` / `OnPoolReachedMinimum(...)`) must change signature to take the new structs — this is a breaking change to the interface, not additive, since C++ virtual overrides can't coexist with a renamed signature cleanly. Any existing implementer must be updated in the same change.

### Event struct redesign (snapshot, not pointer)

```cpp
struct PoolChangedEvent {
    Dia::Core::StringCRC resourceName;
    float                 newValue;
    float                 delta;
    // instanceId, not const EconomyInstance* — resolve identity via whatever
    // stable ID EconomyInstance already exposes, if consumers need to know which instance.
};
```

The exact identity field (does `EconomyInstance` have a stable ID today, or does identity need to be added?) is an open question — see below.

### Timing

`EconomyBusAdapter::On*` fires synchronously, whenever `EconomySystem::Tick()` calls `Notify()` — there's no designated pre-Primary step like `IFlushAdapter` has. Whether this lands in the same tick's Primary pass or the next tick's depends on whether the Economy-owning module runs before or after `MessageBusModule` in the stage's module order.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEconomy/IEconomyObserver.h` | Modify — redesign `PoolChangedEvent`/`TransactionClampedEvent`/`TransferCompletedEvent` to snapshot values; add `PoolReachedMaximumEvent`/`PoolReachedMinimumEvent`; change the two overload signatures |
| `Dia/DiaEconomy/EconomyBusAdapter.h` / `.cpp` | New |
| `Dia/DiaEconomy/Messages/economy_messages.diagamemessages` | New — five event type declarations |
| `Dia/DiaEconomy/DiaEconomy.vcxproj` | Add new files; add `DiaMessageBus` reference |
| Any existing `IEconomyObserver` implementer (grep before starting — none confirmed at spec time) | Modify for new struct signatures |
| `Tests/GoogleTests/Economy/EconomyBusAdapterTests.cpp` | New |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | Event structs use `StringCRC`/plain value types; no pointers to STL, no `std::string`. |
| SD-MBX2-001 | Bus is sole subscriber for cross-system traffic | `EconomyBusAdapter` is the only thing that crosses from DiaEconomy to other systems via the bus; direct `IEconomyObserver` use stays same-module/same-tick only. |

## Open Design Questions

1. **Does `EconomyInstance` need a stable identity field for bus consumers, now that the raw pointer is gone?** If a subscriber needs to know *which* economy instance changed (not just which resource), the event needs an ID field. Check whether `EconomyInstance` already exposes one before finalizing the struct shape.
2. **Are there any existing direct implementers of `IEconomyObserver`** (e.g. inside `DiaEconomyInspector`) that need updating for the new struct signatures? Grep before implementation.
3. **Module ordering** — does whatever owns `EconomySystem::Tick()` need to run before `MessageBusModule` for same-tick delivery, or is a one-tick lag acceptable for economy events? Recommend: one-tick lag is fine; economy changes aren't latency-critical.

## Status

`Approved`
