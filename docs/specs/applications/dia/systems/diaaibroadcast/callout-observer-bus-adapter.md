# Feature Spec: callout-observer-bus-adapter

**System:** DiaAICallout (spec directory: diaaibroadcast — legacy name)
**App:** Dia
**Status:** Approved

## Summary

`CalloutRegistry` (Done) is deliberately poll-only today — `Emit`/`Query`/`Claim`/`Release`/`Update`, no notification mechanism, per its own system spec (SD-004/SD-005, and the explicit exclusion of DiaStreams — "no cross-PU messaging"). `Query()` stays exactly as it is; it's the only mechanism that answers "what's currently live," which push can never fully replace (a freshly spawned agent, or one newly in range of an old callout, needs to catch up on state push alone can't deliver).

This feature adds a genuinely new capability, not a migration: `ICalloutObserver`/`CalloutObserverSubject`, composed by `CalloutRegistry`, notifying on `Emit`/`Claim`/`Release` only (not TTL expiry — routine and noisy, not worth broadcasting). A `CalloutBusAdapter` forwards those onto the shared `DiaMessageBus::Bus`. `Emit` carries the full `Callout` + `CalloutHandle` (the callout already self-describes its own filtering criteria — kind, position, radius, faction — so listeners filter locally with zero need to call `Query()` in reaction to a fresh emission). `Claim`/`Release` carry just the handle + claimer ID, since a reacting listener already knows about the callout.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaAICallout — [diaaibroadcast.md](diaaibroadcast.md) (Done) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` (`Bus::Broadcast`) |

## Goals

- AI agents can react to a new callout, claim, or release the instant it happens, without polling `Query()` every tick "just in case."
- `Query()`'s role narrows to what it's actually good for: state catch-up (new agents, agents newly in range) — not routine per-tick polling.
- No change to `CalloutRegistry`'s existing poll API or any of its accepted decisions (SD-001 through SD-007).

## Non-Goals

- Notifying on TTL expiry. Deliberately excluded — routine and high-frequency, not a signal worth broadcasting.
- Retiring `Query()`. It stays exactly as-is.
- Providing a generic `Module` wrapper for `CalloutRegistry`. Per SD-002 ("Registry is explicitly constructed, not a singleton"), every consumer owns and drives its own instance — this feature's Subject/Adapter must work for any owner, not assume one owner shape.

## Acceptance Criteria

- `ICalloutObserver` is a new interface with default no-op virtuals: `OnCalloutEmitted(const Callout&, CalloutHandle)`, `OnCalloutClaimed(CalloutHandle, StringCRC claimerEntityId)`, `OnCalloutReleased(CalloutHandle, StringCRC claimerEntityId)`.
- `CalloutObserverSubject` provides `Subscribe`/`Unsubscribe`, same shape as `EconomyObserverSubject` (fixed-capacity array, no heap allocation).
- `CalloutRegistry` composes a `CalloutObserverSubject` member and calls the corresponding notify on: successful `Emit` (always fires — every emission is notification-worthy), successful `Claim` (only when it returns `true`), and actual `Release` (only when it performed a real release — not on any of the documented no-op paths: expired handle, already-unclaimed, wrong claimer).
- `Update(dt)`'s TTL expiry path does **not** call any observer notification.
- `CalloutBusAdapter : ICalloutObserver` forwards each notification to `bus.Broadcast<T>()` with the corresponding event struct.
- `CalloutEmittedEvent`/`CalloutClaimedEvent`/`CalloutReleasedEvent` are declared in a `.diagamemessages` file under `DiaAICallout` and generated via `dia codegen messages`.
- Any existing consumer of `CalloutRegistry` (e.g. `AICalloutTestStageModule`, Done, currently zero `DiaMessageBus` dependency) continues to compile and function unchanged — this feature is additive to the registry, not a breaking change to its existing public API.

## Design

### Where the Subject lives

```cpp
class CalloutRegistry : public CalloutRegistryData {
public:
    // ... existing API unchanged ...

    void Subscribe(ICalloutObserver* observer);   // new — delegates to mObservers
    void Unsubscribe(ICalloutObserver* observer);

private:
    CalloutObserverSubject mObservers;             // new member
};
```

`Emit`, `Claim`, `Release` each call the corresponding `mObservers.Notify*(...)` at the point they already succeed today — no new branching logic beyond what error/no-op paths already exist.

### Adapter

```cpp
class CalloutBusAdapter : public Dia::AICallout::ICalloutObserver {
public:
    explicit CalloutBusAdapter(Dia::MessageBus::Bus& bus) : mBus(bus) {}

    void OnCalloutEmitted(const Callout& c, CalloutHandle h) override {
        mBus.Broadcast(CalloutEmittedEvent{ c, h });
    }
    void OnCalloutClaimed(CalloutHandle h, Dia::Core::StringCRC claimerEntityId) override {
        mBus.Broadcast(CalloutClaimedEvent{ h, claimerEntityId });
    }
    void OnCalloutReleased(CalloutHandle h, Dia::Core::StringCRC claimerEntityId) override {
        mBus.Broadcast(CalloutReleasedEvent{ h, claimerEntityId });
    }

private:
    Dia::MessageBus::Bus& mBus;
};
```

Whoever owns a `CalloutRegistry` (e.g. a future gameplay-facing wrapper, or `AICalloutTestStageModule` if updated) constructs a `CalloutBusAdapter` and calls `registry.Subscribe(&adapter)` — same ownership model as the registry itself, no singleton, no central module.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaAICallout/ICalloutObserver.h` | New |
| `Dia/DiaAICallout/CalloutObserverSubject.h` / `.cpp` | New |
| `Dia/DiaAICallout/CalloutRegistry.h` / `.cpp` | Modify — add `Subscribe`/`Unsubscribe`, compose `CalloutObserverSubject`, call notifications from `Emit`/`Claim`/`Release` |
| `Dia/DiaAICallout/CalloutBusAdapter.h` / `.cpp` | New |
| `Dia/DiaAICallout/Messages/callout_messages.diagamemessages` | New — three event declarations |
| `Dia/DiaAICallout/DiaAICallout.vcxproj` | Add new files; add `DiaMessageBus` reference (adapter only — `CalloutRegistry`/`ICalloutObserver` stay bus-free) |
| `Tests/GoogleTests/AICallout/CalloutObserverBusAdapterTests.cpp` | New |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| SD-002 | Registry explicitly constructed, not a singleton | `CalloutObserverSubject` is a plain composed member, no statics; `CalloutBusAdapter` is explicitly constructed and registered by whichever owner wants it. |
| SD-004 | `Query()` returns unclaimed callouts only | Unchanged — this feature does not touch `Query()`. |
| PD-004 | No STL in public APIs | `ICalloutObserver` signatures use `Callout`/`CalloutHandle`/`StringCRC` value types; `CalloutObserverSubject` uses a fixed-capacity `DynamicArrayC`, matching `EconomyObserverSubject`'s pattern. |

## Open Design Questions

1. **Does the existing (Done) `AICalloutTestStageModule` get updated to demonstrate this wiring, or is that a separate follow-up?** It currently has zero `DiaMessageBus` dependency. Recommend treating as a separate, optional follow-up — not required for this feature to ship.
2. **`CalloutHandle` carries a pointer to `CalloutRegistryData`** (for validity/generation checks). Confirm this pointer remains safe to carry inside a bus message queued for later delivery — since the registry itself is the thing emitting the event, and messages may be drained on a later pass, verify the registry can't be destroyed between `Emit`/`Claim`/`Release` and the Bus's next Primary pass in realistic usage (same-frame, same-owner lifetime — expected to be safe, but worth a explicit check against DiaMailbox's deferred-delivery timing).

## Status

`Approved`
