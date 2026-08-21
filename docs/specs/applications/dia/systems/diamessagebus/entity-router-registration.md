# Feature Spec: Entity Router Registration

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

After `core-bus` ships, the bus has only `BroadcastRouter` registered — enabling the System→System and Entity→System patterns via `Address{ kBroadcastRouterId, 0 }`. The remaining two patterns, Entity→Entity and System→Entity, require a second router keyed to `kEntityRouterId`. That router (EntityRouter) is owned by diaentitytemplate, but the bus must expose the registration point it plugs into and a well-known `kEntityRouterId` constant that all producers use when addressing an entity. This feature wires in that registration point alongside the existing `kBroadcastRouterId`, then proves entity-addressed delivery through an integration test covering all four routing patterns without taking any dependency on diaentitytemplate.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `Bus::RegisterRouter(router)` delegates to `mMailbox.RegisterRouter(router)` — the mock router's `Resolve` is invoked during flush of an entity-addressed post | GoogleTest: construct `MessageBusModule`, register a `MockEntityRouter*`, post `HitEvent` to `Address{kEntityRouterId, 42}`, flush; assert `MockEntityRouter::resolveCallCount == 1` |
| AC-2 | `Bus::kEntityRouterId` equals `Dia::Core::StringCRC{"entity"}` and `Bus::kBroadcastRouterId` equals `Dia::Core::StringCRC{"broadcast"}` | GoogleTest value comparison; `static_assert` if `StringCRC::operator==` is constexpr |
| AC-3 | `Bus::Post<HitEvent>(Address{kEntityRouterId, 42}, msg)` followed by a flush calls the handler for the subscriber whose `SubscriberId.value == 42` exactly once and makes zero calls to the handler for `SubscriberId.value == 99` | GoogleTest: subscribe two stubs, post one entity-addressed message, flush; assert handler A call count 1, handler B call count 0 |
| AC-4 | `Bus::Broadcast<HitEvent>(msg)` fans out to all type subscribers regardless of entity handle, unchanged by registering the mock router | GoogleTest (same fixture): after AC-3 post, `Broadcast` a second message; assert both handlers each receive exactly one additional call |
| AC-5 | After AC-3's flush, `Bus::GetLastTickLedger()` contains an entry whose `routerId == Bus::kEntityRouterId` and `count >= 1` for the `HitEvent` type | Assert on `LedgerMessageEntry::routerId` in the same GoogleTest |

## Design

### What this feature adds

The Bus public interface is already pinned in the system spec. This feature's implementation work is:

**1. `Bus::RegisterRouter` implementation** — in `Bus.cpp`, a one-liner:

```cpp
void Bus::RegisterRouter(Dia::Mailbox::IMailboxRouter* router)
{
    mMailbox.RegisterRouter(router);
}
```

The `Mailbox` owns duplicate-ID detection and the 16-slot cap (system spec Open Design Question 3: 16 slots sufficient; current plan is 2 routers — BroadcastRouter + EntityRouter).

**2. `kEntityRouterId` constant** — declared alongside `kBroadcastRouterId` in `Bus.h`:

```cpp
static constexpr Dia::Core::StringCRC kBroadcastRouterId{ "broadcast" };
static constexpr Dia::Core::StringCRC kEntityRouterId{ "entity" };
```

Both are `StringCRC` values (PD-001). They are used exclusively as `Address::routerId` to select which registered router processes a posted message during flush. Per SD-MBX2-012, the internal type routing key is `Mailbox::TypeKey<T>()` (a `__FUNCSIG__` CRC); `kEntityRouterId` is an address discriminator, not a type key.

**3. Integration test with `MockEntityRouter`** — a test-local `IMailboxRouter` subclass that resolves by comparing each `SubscriberId.value` against `addr.payload`. `HitEvent` is a test-only struct defined in the test file:

```cpp
struct HitEvent {
    static constexpr Dia::Core::StringCRC kTypeId{ "HitEvent" };
    uint32_t targetId = 0;
};
```

### Routing patterns this feature enables

| Pattern | `Address` used | Router that resolves delivery |
|---------|---------------|-------------------------------|
| System → System | `{ kBroadcastRouterId, 0 }` | BroadcastRouter fans out to all type subscribers |
| Entity → System | `{ kBroadcastRouterId, 0 }` | BroadcastRouter fans out (sender identity irrelevant — SD-MBX2-003) |
| Entity → Entity | `{ kEntityRouterId, handle.bits }` | EntityRouter delivers to target entity's subscribers only |
| System → Entity | `{ kEntityRouterId, handle.bits }` | EntityRouter delivers to target entity's subscribers only |

After this feature, all four patterns are addressable. The EntityRouter that handles the `kEntityRouterId` rows is registered at stage start by diaentitytemplate — this feature supplies only the plug-in point. The mock in the integration test stands in for the real router.

### `MockEntityRouter` design

```cpp
// Test-local; defined in EntityRouterRegistrationTest.cpp only.
class MockEntityRouter : public Dia::Mailbox::IMailboxRouter {
public:
    uint32_t resolveCallCount = 0;

    Dia::Core::StringCRC GetRouterId() const override {
        return Dia::MessageBus::Bus::kEntityRouterId;
    }

    void Resolve(const Dia::Mailbox::Address&         addr,
                 const Dia::Mailbox::SubscriberSet&   liveSubscribers,
                 Dia::Mailbox::SubscriberSet&          outMatched) override {
        ++resolveCallCount;
        for (uint32_t i = 0; i < liveSubscribers.Size(); ++i) {
            if (liveSubscribers[i].value == addr.payload) {
                outMatched.Add(liveSubscribers[i]);
            }
        }
    }
};
```

`Resolve` is pure payload matching — no allocation, no STL (PD-004). The real EntityRouter (owned by diaentitytemplate) honours the same `IMailboxRouter` contract and maps entity handle bits to component subscriber IDs.

### Non-scope (restated from system spec)

- **EntityRouter implementation** — owned by diaentitytemplate; not built here.
- **Entity subscription semantics** — how entity components register their handlers with the bus is diaentitytemplate's concern.
- **`EntityId` / handle pool types** — live in diaentitytemplate; the test uses raw `uint64_t` payload values directly.

## Tasks

**Prerequisite:** `core-bus` (Bus class, BroadcastRouter, two-pass flush, last-tick ledger).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `Bus::RegisterRouter(IMailboxRouter*)` in `Bus.cpp` delegating to `mMailbox.RegisterRouter(router)`; confirm `kEntityRouterId` and `kBroadcastRouterId` constants are declared in `Bus.h` | AC-1, AC-2 | Not Started | haiku | One-liner delegation; constants may already be present from core-bus scaffold — confirm rather than re-add |
| 2 | Write `EntityRouterRegistrationTest.cpp` with `MockEntityRouter` and GoogleTests covering router delegation, entity-addressed delivery, broadcast fan-out, and ledger `routerId` verification | AC-1 through AC-5 | Not Started | sonnet | `HitEvent` and `MockEntityRouter` are test-local; no diaentitytemplate dependency |

## Status

`Approved`
