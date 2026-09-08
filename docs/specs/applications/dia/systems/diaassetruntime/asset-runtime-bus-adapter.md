# Feature Spec: asset-runtime-bus-adapter

**System:** DiaAssetRuntime
**App:** Dia
**Status:** Approved

## Summary

`event-notification.md` ([spec](event-notification.md), Approved) already fully designs `IAssetStateListener` — `OnAssetReady`/`OnAssetUnloading`/`OnAssetLoadFailed`, `RegisterListener`/`UnregisterListener`, dispatched synchronously from state transitions. The system plan marks it `Done` (tasks 16–19, 9 passing tests as part of a 69-test total). **It is not present in the codebase** — `Dia/DiaAssetRuntime/IAssetStateListener.h` does not exist, and `AssetRuntime.h` has no `RegisterListener`/`UnregisterListener`; only a private `IAssetLoadCallback` override (`OnLoadComplete`/`OnLoadFailed`) exists, which is internal plumbing from whatever loader does the actual I/O — not the same thing as `IAssetStateListener`, and not externally visible.

This feature's scope is therefore two parts: **(1)** implement `IAssetStateListener` exactly per the existing Approved `event-notification.md` spec (no redesign — it's already fully specified, just missing), and **(2)** add `AssetRuntimeBusAdapter : IAssetStateListener` forwarding to the shared `DiaMessageBus::Bus`, following the same adapter pattern used across the other systems in this effort.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaAssetRuntime — [diaassetruntime.md](diaassetruntime.md) |
| Rebuilds feature | [event-notification.md](event-notification.md) (Approved, plan marked Done, missing from codebase) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` (`Bus::Broadcast`) |

## Goals

- `IAssetStateListener` exists in the codebase exactly as `event-notification.md` specifies, with the acceptance criteria and tests that spec already defines.
- Gameplay systems can subscribe to asset lifecycle events via the bus without polling `IsAssetReady`/`GetAssetState`.
- The existing private `IAssetLoadCallback` mechanism (loader → `AssetRuntime::OnLoadComplete`/`OnLoadFailed`) is untouched — it's a different, internal concern from the external `IAssetStateListener` notification this feature restores.

## Non-Goals

- Redesigning `IAssetStateListener`'s API. `event-notification.md` already made those decisions (dispatch on `Registered→Staged`, synchronous delivery, deferred-removal-during-dispatch, 16-listener capacity, late-join self-serve via `GetStagedAssets()`) — this feature implements them as written, not from scratch.
- Investigating *why* the implementation is missing despite the plan marking it Done. Out of scope for this spec; the decision made was to rebuild rather than forensically trace the discrepancy.

## Acceptance Criteria

### Part 1 — restore `event-notification.md` (its own ACs apply verbatim; summarized here)

- `IAssetStateListener` abstract interface: `OnAssetReady(assetId, resolvedPath)`, `OnAssetUnloading(assetId)`, `OnAssetLoadFailed(assetId)`.
- `RegisterListener`/`UnregisterListener` on `AssetRuntime`, fixed-capacity `DynamicArrayC<IAssetStateListener*, 16>`.
- Dispatch wired into existing state transitions: `OnAssetReady` on `Registered→Staged`, `OnAssetUnloading` on transition to `Unloading`.
- Deferred removal during dispatch (`mIsDispatching` flag + pending-removal list).
- All 9 tests from the original `EventNotificationTest.cpp` (per the plan's task 19) pass.

### Part 2 — bus adapter (new)

- `AssetRuntimeBusAdapter : IAssetStateListener` forwards each callback to `bus.Broadcast<T>()` with a corresponding event struct (`AssetReadyEvent`, `AssetUnloadingEvent`, `AssetLoadFailedEvent`).
- These three event types are declared in a `.diagamemessages` file under `DiaAssetRuntime` and generated via `dia codegen messages`.
- `AssetRuntimeBusAdapter` is registered via `RegisterListener`, alongside any other direct listener (e.g. a future `MyGraphicsSystem` per the spec's own usage example) — it does not replace direct listener use.

## Design

### Distinguishing the two callback mechanisms already/newly present

```
IAssetLoadCallback (private, pre-existing)         IAssetStateListener (public, this feature restores)
  │                                                   │
  loader → AssetRuntime::OnLoadComplete/OnLoadFailed  AssetRuntime → registered external listeners
  (internal — how AssetRuntime learns a load          (external — how other systems learn AssetRuntime's
   finished from the underlying loader)                state transitions)
```

These are not the same notification path and this feature does not merge them. `IAssetLoadCallback` stays exactly as it is.

### Adapter

```cpp
class AssetRuntimeBusAdapter : public Dia::AssetRuntime::IAssetStateListener {
public:
    explicit AssetRuntimeBusAdapter(Dia::MessageBus::Bus& bus) : mBus(bus) {}

    void OnAssetReady(const Dia::Core::StringCRC& assetId,
                       const Dia::Core::Containers::String512& resolvedPath) override {
        mBus.Broadcast(AssetReadyEvent{ assetId, resolvedPath });
    }
    void OnAssetUnloading(const Dia::Core::StringCRC& assetId) override {
        mBus.Broadcast(AssetUnloadingEvent{ assetId });
    }
    void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) override {
        mBus.Broadcast(AssetLoadFailedEvent{ assetId });
    }

private:
    Dia::MessageBus::Bus& mBus;
};
```

Per `event-notification.md`'s own binding decision SD-ARUN-001 ("No DiaApplicationFlow dependency" — listeners are raw interface pointers registered by game code, no `ProcessingUnit`/`Module` awareness), `AssetRuntimeBusAdapter` itself must stay free of any `DiaApplicationFlow` dependency too; only `DiaMessageBus`. Whoever owns both an `AssetRuntime` and a `Bus` reference constructs and registers the adapter — same explicit-construction pattern as every other adapter in this effort.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaAssetRuntime/IAssetStateListener.h` | New (restore, per `event-notification.md` Task 16) |
| `Dia/DiaAssetRuntime/AssetRuntime.h` / `.cpp` | Modify — restore `RegisterListener`/`UnregisterListener`, listener storage, dispatch flag, dispatch-on-transition wiring (per Tasks 17–18) |
| `Dia/DiaAssetRuntime/AssetRuntimeBusAdapter.h` / `.cpp` | New |
| `Dia/DiaAssetRuntime/Messages/assetruntime_messages.diagamemessages` | New — three event declarations |
| `Dia/DiaAssetRuntime/DiaAssetRuntime.vcxproj` | Add new files; add `DiaMessageBus` reference (adapter only) |
| `Tests/GoogleTests/AssetRuntime/EventNotificationTest.cpp` | Restore (per plan Task 19 — 9 tests) |
| `Tests/GoogleTests/AssetRuntime/AssetRuntimeBusAdapterTests.cpp` | New |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | Matches `event-notification.md`'s existing compliance — `StringCRC`/`String512`/`DynamicArrayC` only. |
| SD-ARUN-001 | No DiaApplicationFlow dependency | `AssetRuntime` and `IAssetStateListener` stay dependency-free; only the new `AssetRuntimeBusAdapter` adds a `DiaMessageBus` reference, and even that stays free of `DiaApplicationFlow`. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | Unaffected — this feature touches runtime-side notification only. |

## Open Design Questions

1. **Should the "missing implementation" discrepancy be investigated separately** (git history on `Dia/DiaAssetRuntime/`) to understand how a plan-marked-Done feature ended up absent, in case the same thing happened elsewhere in the codebase? Explicitly deferred per the decision to just rebuild — flagging here in case it's worth a standalone audit later.

## Status

`Approved`
