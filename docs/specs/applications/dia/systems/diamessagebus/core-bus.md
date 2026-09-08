# Feature Spec: Core Bus

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

DiaMessageBus needs its runtime substrate before anything else can target it: a `Bus` that wraps a `Dia::Mailbox::Mailbox`, a `MessageBusModule` (`IModule` on SimPU) that drives the per-tick flush, a `BroadcastRouter` for type-level fan-out, and a **last-tick ledger** that records what flowed. DiaMailbox already provides transport, compile-time-capacity typed queues, `__FUNCSIG__`-keyed routing, and router registration; this feature adds callback dispatch, producer/subscriber registration, the two-pass flush, and the ledger tally on top. Every other feature in the system (codegen wiring, entity-router, flush-adapters, the visual debugger) targets this API, so its shape is the system's load-bearing contract.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `Bus::RegisterType<T, kCapacity>(policy)` forwards to `Mailbox::RegisterType<T, kCapacity>(policy)`; returns `false` if T is already registered or the registry is full | Register a type twice — first `true`, second `false` |
| AC-2 | `RegisterProducer<T>(id)` records the producer id in a metadata table and has **no routing effect** — a type with a producer but zero subscribers delivers to nobody | Register a producer, Post, assert zero deliveries in the ledger |
| AC-3 | `Subscribe<T>(id, handler, pass = Pass::Primary)` installs the handler and returns a `BusSubscriptionHandle`; destroying the handle unsubscribes (no further delivery) | Subscribe, Post → delivered; destroy handle, Post → not delivered |
| AC-4 | `Post<T>(addr, msg)` enqueues into the Mailbox ring; returns `false` if T is unregistered (`DIA_ASSERT` in Debug) | Post before/after RegisterType |
| AC-5 | `Broadcast<T>(msg)` is equivalent to `Post<T>(Address{ kBroadcastRouterId, 0 }, msg)` | Broadcast; assert broadcast subscribers receive it |
| AC-6 | `BroadcastRouter::GetRouterId()` == `StringCRC{"broadcast"}`; `Resolve()` copies all live subscribers of the type into `outMatched` (full type-level fan-out) | Unit test: 3 subscribers of T → all 3 matched |
| AC-7 | `MessageBusModule` is an `IModule`; `OnStart()` registers the `BroadcastRouter`; `Update()` runs Pre-Primary → Primary → Reaction; `OnStop()` tears down | Drive module lifecycle; assert router registered after OnStart |
| AC-8 | Pre-Primary step calls `Flush(bus)` on every registered `IFlushAdapter` in registration order, before the Primary pass | Register 2 mock adapters; assert both flushed, in order, before any Primary delivery |
| AC-9 | Primary pass drains all Primary messages, resolves subscribers via the addressed router, and invokes handlers | Post to broadcast + assert handler called during Update |
| AC-10 | Reaction pass drains messages posted **during** Primary handlers to `Pass::Reaction` subscribers, after Primary completes; a handler that attempts to enqueue during the Reaction pass triggers `DIA_ASSERT` in Debug / is silently dropped in Release (SD-MBX2-002, Open Q #1) | Post a reaction message from a Primary handler → delivered same tick in Reaction; re-enqueue from a Reaction handler → assert (Debug) |
| AC-11 | `GetLastTickLedger()` returns the **just-completed** tick's `LedgerSnapshot`; double-buffered so a read mid-tick returns the previous tick, never a partially-filled buffer | Read ledger across two ticks; assert stable previous-tick data |
| AC-12 | Ledger tally is correct: N posts of type T delivered to M subscribers yields an entry with `count == N`, `deliveries == N*M`, correct `routerId` and `pass`; `droppedCount` reflects overflow drops | Post 4 messages to 2 subscribers → entry count 4, deliveries 8 |
| AC-13 | Core-bus GoogleTests assert delivery **through** `GetLastTickLedger()` rather than reaching into Bus internals | Inspect test suite; ledger is the observation surface |

## Design

### Reused pinned interfaces

`Bus`, `MessageBusModule`, `IFlushAdapter`, `BusSubscriptionHandle`, `Pass`, `LedgerMessageEntry`, and `LedgerSnapshot` are defined in the system spec's Public Interfaces section — this feature implements them as written. Notably:

- `template <class T, uint32_t kCapacity> bool RegisterType(Dia::Mailbox::OverflowPolicy policy);` — capacity is **compile-time**, matching `Mailbox::RegisterType<T, kCapacity>` (which allocates `kCapacity * stride` at registration). Codegen emits `kCapacity` as a template argument (see diagamemessages-format).
- `template <class T> BusSubscriptionHandle Subscribe(StringCRC subscriberId, std::function<void(const T&)> handler, Pass pass = Pass::Primary);`
- `const LedgerSnapshot& GetLastTickLedger() const;` — last-tick only. History ring buffer + `ServiceStream` export are separate/deferred (see frame-ledger).

### BroadcastRouter (new)

The only concrete router this feature builds. Implements `Dia::Mailbox::IMailboxRouter`:

```cpp
namespace Dia::MessageBus {
    class BroadcastRouter : public Dia::Mailbox::IMailboxRouter {
    public:
        Dia::Core::StringCRC GetRouterId() const override { return Bus::kBroadcastRouterId; }
        // Fan-out: every live subscriber of the type receives the message.
        void Resolve(const Dia::Mailbox::Address&,
                     const Dia::Mailbox::SubscriberSet& liveSubscribers,
                     Dia::Mailbox::SubscriberSet& outMatched) override {
            outMatched = liveSubscribers;   // address body ignored for broadcast
        }
    };
}
```

The `EntityRouter` is **not** built here (owned by diaentitytemplate — see entity-router-registration); `Bus` only holds the registration point.

### Wrapping DiaMailbox

The `Bus` owns one `Mailbox`. Mapping:

| Bus API | Mailbox substrate |
|---------|-------------------|
| `RegisterType<T, kCapacity>(policy)` | `Mailbox::RegisterType<T, kCapacity>(policy)` |
| `Post<T>(addr, msg)` / `Broadcast<T>` | `Mailbox::Send<T>(addr, msg)` |
| `Subscribe<T>(id, handler, pass)` | `Mailbox::Subscribe<T>(id)` + Bus-side handler+pass table keyed by subscriber id |
| flush passes | `Mailbox::Drain<T, Visitor>()` per registered type; visitor resolves via router and invokes handlers |
| `RegisterRouter` | `Mailbox::RegisterRouter` |

Routing is by `Mailbox::TypeKey<T>()` (`__FUNCSIG__` CRC), **not** `kTypeId` (SD-MBX2-012). `kTypeId` is copied into `LedgerMessageEntry.typeId` for human-readable display only.

### Two-pass flush (Update)

1. **Pre-Primary** — call `Flush(bus)` on each registered `IFlushAdapter` in registration order (they `Post` external events in).
2. **Primary** — drain each type's Primary messages; per message, resolve subscribers via the addressed router; invoke each subscriber's handler; tally into the *building* ledger buffer.
3. **Reaction** — drain messages enqueued during Primary to `Pass::Reaction` subscribers. A re-enqueue attempt during this pass sets off `DIA_ASSERT` (Debug) / silent drop (Release). A `mInReactionPass` flag gates the assert in `Post`.

At end of Update, swap the double-buffered ledger (building → last-completed) so `GetLastTickLedger()` always returns a whole tick.

### Observation (per system-spec instrumentation plan)

`DIA_TRACE_ZONE` around each pass; `DIA_METRIC_INCREMENT` for `dia.msgbus.{posted,delivered,dropped}`; `DIA_LOG_WARNING` on drop / on Post of an unregistered type in Release.

## Open Design Questions

| # | Question | Notes |
|---|----------|-------|
| 1 | `LedgerSnapshot.entries` is sized `DynamicArrayC<…, 64>` but `Mailbox::kMaxTypes == 32` | Recommend aligning the ledger entry cap to `kMaxTypes` (32) so it cannot under-size relative to the type registry. Confirm during implementation. |
| 2 | Does `Mailbox` expose a per-type drain-with-visitor suitable for the resolve-then-dispatch step, or must the Bus iterate `GetSubscribersForType<T>()` and pull? | Determines whether the Primary/Reaction loop is a `Drain<T,Visitor>` or a manual pull. Verify against `Mailbox.h` at implementation start. |
| 3 | Reaction re-entrancy detection — a single `mInReactionPass` bool, or per-type? | A single bool is sufficient given single-threaded flush; confirm no nested-pass scenario exists. |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Bus + `MessageBusModule` + `BroadcastRouter` + `RegisterType`/`RegisterProducer`/`Subscribe`/`Post`/`Broadcast` + single Primary pass + last-tick ledger + `BusSubscriptionHandle` + `IFlushAdapter` interface | `TestDiaMessageBusCore*` | Not Started | sonnet | Plan task 2. TDD — prove RED first. Assert via ledger. |
| 2 | Two-pass flush: `Pass` enum, Reaction pass drain, re-entrancy assert (Debug) / drop (Release) | `TestDiaMessageBusFlush*` | Not Started | sonnet | Plan task 3. Same feature (Q1); split as its own task — hardest single piece. |

## Status

`Approved`
