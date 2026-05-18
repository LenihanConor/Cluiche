# Feature Spec: Stream Tap & Debug Iteration

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Research
@docs/research/event_stream_diapplic/summary.md

## Depends On
- @docs/specs/features/dia/diaapplicationflow/stream-topology-manifest.md (F1, Approved)
- @docs/specs/features/dia/diaapplicationflow/stream-policy-envelope.md (F3, Approved)
- @docs/specs/features/dia/diaapplicationflow/lifecycle-events.md (F2, Approved)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Stream Tap & Debug Iteration | (this file) |

## Problem Statement

Streams today can only be observed by **declared readers** — modules with `EventStreamReader<T>` handles wired in `OnConnectStreams` and listed in `module.reads`. That blocks every external consumer:

- `DiaDebugServer` cannot show "what events are flowing on stream X" without modifying the writer module — so it runs a parallel `SubscriptionManager` keyed on ad-hoc protocol topic strings (`stage_transition`, etc.) instead of consuming streams.
- An editor inspector cannot render a live event log.
- A future telemetry sink or replay recorder cannot subscribe.
- Nothing can iterate the stream registry generically.

This feature adds a **type-erased tap subscription** on any stream and a **registry iteration API** on `IApplicationInspectable`, then **replaces `DiaDebugServer/SubscriptionManager` entirely** — debug-protocol `subscribe`/`unsubscribe` messages route to `AttachTap`/`DetachTap`. The parallel debug pub/sub disappears; one mechanism (streams + taps) covers production wiring, lifecycle observability, and remote debug consumption.

A serializer is registered alongside each `DIA_STREAM_TYPE`, enabling type-erased payload → JSON conversion for WebSocket forwarding.

## Acceptance Criteria

1. **AC1 — `IStreamStore::AttachTap` / `DetachTap`.** New methods on `IStreamStore` (and on every store implementation) that register a `TapCallback` against the store. Returns a `TapHandle` (opaque ID). Detach removes the callback. Both are thread-safe relative to ongoing `Send`/`Consume`.
2. **AC2 — Sync dispatch in `Send`.** `EventStreamStore<T>::SendInternal` walks the tap list under the existing store mutex *after* fan-out to readers, *before* notifying the block-policy condvar. Tap callback receives `(streamId, payloadType, TapEvent)` where `TapEvent = { TimeAbsolute timestamp; StringCRC sender; uint64 sequence; const void* bytes; size_t size; }`.
3. **AC3 — `bytes`/`size` lifetime.** `bytes` points to the `Event<T>::payload` field of the just-stored envelope; `size = sizeof(T)`. The pointer is valid only for the duration of the callback. Tap is contractually required to copy or hand off in O(1) — *no further `Send` calls or blocking work permitted in the callback*.
4. **AC4 — Runtime attach/detach.** Taps may attach at any time after `Application::Start()` returns true. Attaching during a Send acquires the store mutex; the new tap sees subsequent Sends but not the in-flight one. Detach is symmetric.
5. **AC5 — `IApplicationInspectable::EnumerateStreams` extended.** `GetStreamInfo(DynamicArrayC<StreamInfo, 16>&)` populates `StreamInfo` with: `id`, `kind`, `payloadType` (StringCRC), `fromPU`, `toPU`, `overflowPolicy`, `currentSequence` (last-assigned sequence number), `attachedReaderCount`, `attachedTapCount`. The struct's existing fields are preserved; new fields are additive.
6. **AC6 — Serializer registration.** New macro `DIA_STREAM_TYPE_WITH_SERIALIZER(T, jsonSerializerFn)` registers a `(payloadType StringCRC) → (T, void* → Json::Value)` mapping. Existing `DIA_STREAM_TYPE(T)` from F1 still works (no serializer registered → `StreamTypeRegistry::SerializeToJson` returns null Json::Value with a warning logged once per type per process).
7. **AC7 — `StreamTypeRegistry::SerializeToJson`.** New API: given a payload-type StringCRC and `(const void* bytes, size_t size)`, returns `Json::Value` if a serializer is registered, or `Json::nullValue` otherwise. Used by tap consumers (DebugServer bridge and others).
8. **AC8 — `DiaDebugServer/SubscriptionManager` deleted.** The class and its files are removed. Debug-protocol message handlers (`HandleSubscribe`, `HandleUnsubscribe`, `HandleConnectionClosed`) attach/detach taps on the requested streamId instead of writing to the subscription list. WebSocket connection IDs map 1:1 to a tap handle owned by `DebugServer`.
9. **AC9 — `DebugServer` consumes `$lifecycle` via tap.** `BroadcastStageTransition` is removed. The DebugServer attaches its own tap on `$lifecycle` at start; on `kStageTransitionCommitted` events it forwards to subscribed clients (those who subscribed to the legacy `stage_transition` topic continue to work via a topic→stream alias map; new clients can subscribe to `$lifecycle` directly). `NotifySubscribers(dataType, payload)` is rewritten as a thin shim that taps the named stream when first subscribed and dispatches via the registry's serializer.
10. **AC10 — Mock tap consumer test.** GoogleTest `TestStreamTap.cpp`: a mock tap attaches to a writer-only test stream, asserts each `Send` triggers the callback exactly once with the correct `streamId`, `payloadType`, envelope fields, and byte payload.
11. **AC11 — Per-stream tap count visible.** Test asserts `StreamInfo::attachedTapCount` reflects active taps; increments on Attach, decrements on Detach.
12. **AC12 — Lifecycle integration.** GoogleTest: tap on `$lifecycle` stream sees the same six lifecycle event types as a regular reader. Tests that envelope sender is `$application`, sequence is monotonic, and payloadType StringCRC matches `LifecycleEvent`.
13. **AC13 — Stress test.** GoogleTest fires N=4 readers + N=4 taps on a high-frequency event stream (10k events at 60 Hz simulated, 256-deep ring), asserts: no deadlock; no envelope corruption; sequence numbers contiguous from each consumer's perspective; no missed callbacks (drops counted via `EventStreamStore`'s overflow path, equal across readers and taps for the same overflow events).
14. **AC14 — Module doc updated.** `dia.application.architecture.module.md` lists tap API in `public_api`, registry iteration in responsibilities, removes `SubscriptionManager` from forbidden list (it no longer exists in DiaDebugServer's deps).
15. **AC15 — DiaDebugServer doc updated.** `Dia/DiaDebugServer/dia.dia.diadebugserver.architecture.module.md` removes `SubscriptionManager` from public_api; adds tap-based subscription model description.

## Public API

```cpp
namespace Dia::ApplicationFlow {

    // Type-erased tap event.
    struct TapEvent
    {
        Dia::Core::TimeAbsolute timestamp;
        Dia::Core::StringCRC    sender;
        unsigned long long      sequence;
        const void*             bytes;     // valid only during callback
        unsigned int            size;      // bytes
    };

    // Tap callback signature.
    using TapCallback = std::function<void(const Dia::Core::StringCRC& streamId,
                                           const Dia::Core::StringCRC& payloadType,
                                           const TapEvent& event)>;

    // Opaque tap handle returned by AttachTap.
    struct TapHandle
    {
        unsigned int id = 0;            // 0 = invalid
        bool IsValid() const { return id != 0; }
    };

    // IStreamStore (extended)
    class IStreamStore
    {
    public:
        // Existing methods from F1 ...
        virtual TapHandle AttachTap(TapCallback callback) = 0;
        virtual void      DetachTap(TapHandle handle)     = 0;
        virtual unsigned int GetTapCount() const          = 0;
    };

    // StreamInfo (extended)
    struct StreamInfo
    {
        Dia::Core::StringCRC id;
        StreamKind           kind;
        Dia::Core::StringCRC payloadType;          // NEW (F1 stored, F4 surfaces)
        Dia::Core::StringCRC fromPU;
        Dia::Core::StringCRC toPU;
        OverflowPolicy       overflowPolicy;       // NEW
        unsigned long long   currentSequence;      // NEW
        unsigned int         attachedReaderCount;  // NEW
        unsigned int         attachedTapCount;     // NEW
    };
}

namespace Dia::ApplicationFlow {

    // StreamTypeRegistry (extended from F1)
    class StreamTypeRegistry
    {
    public:
        // Existing F1 methods ...

        using JsonSerializer = Json::Value(*)(const void* bytes, size_t size);

        // Register a serializer for a payload type. Called from
        // DIA_STREAM_TYPE_WITH_SERIALIZER's static-init.
        static void RegisterSerializer(const Dia::Core::StringCRC& payloadType,
                                       JsonSerializer serializer);

        // Returns Json::nullValue if no serializer is registered for this type.
        // Logs a "no serializer" warning once per type per process.
        static Json::Value SerializeToJson(const Dia::Core::StringCRC& payloadType,
                                           const void* bytes, size_t size);
    };

    // Macro variant — registers the type AND a serializer.
    #define DIA_STREAM_TYPE_WITH_SERIALIZER(T, serializerFn) \
        DIA_STREAM_TYPE(T); \
        static Dia::ApplicationFlow::StreamTypeSerializerRegistration<T> \
            s_streamserializer_##T{StringCRC(#T), serializerFn}
}
```

**Tap consumer example (illustrative):**
```cpp
TapHandle h = store->AttachTap(
    [](const StringCRC& streamId, const StringCRC& payloadType, const TapEvent& evt)
    {
        // O(1) bookkeeping only — copy or push to own queue.
        Json::Value json = StreamTypeRegistry::SerializeToJson(
            payloadType, evt.bytes, evt.size);
        // ... forward via WebSocket, log, journal, etc.
    });
// ... later
store->DetachTap(h);
```

## Internal Architecture

**Tap storage (per-store):**
```cpp
struct TapEntry { unsigned int id; TapCallback callback; };
DynamicArrayC<TapEntry, kMaxTapsPerStream> mTaps;   // kMaxTapsPerStream = 8
unsigned int mNextTapId = 1;
```

**Send dispatch order (extending F3's flow):**
```
acquire mMutex
1. Apply overflow policy and write envelope to all reader buffers (F3 logic)
2. For each tap in mTaps:
       construct TapEvent { envelope.timestamp, envelope.sender, envelope.sequence,
                            &envelope.payload, sizeof(T) }
       invoke tap.callback(streamId, payloadType, event)
3. notify mNotFullCv
release mMutex
```

Taps fire **after** reader fan-out, in tap-attachment order. This means:
- Readers always see an event before any tap (subscribers can never "race ahead").
- Reader buffers are populated when a tap callback runs, so a tap that needs to know "is this event also queued for a reader" can read `attachedReaderCount` from `StreamInfo`.

**Attach/Detach under store mutex:** both methods acquire the same mutex used by Send and Consume. Attaching while a Send is mid-flight will see the next Send.

**`DebugServer` bridge (replacing `SubscriptionManager`):**
```cpp
class DebugServer {
    struct ClientTap {
        int connectionId;
        Dia::Core::StringCRC streamId;
        TapHandle handle;
    };
    DynamicArrayC<ClientTap, 64> mClientTaps;

    // HandleSubscribe(connId, streamId)
    //   1. Resolve streamId (legacy 'stage_transition' topic alias → '$lifecycle' filter)
    //   2. store = inspectable->FindStreamStoreByPattern(streamId)
    //   3. handle = store->AttachTap(...)
    //   4. mClientTaps.Add({connId, streamId, handle})
    //
    // HandleUnsubscribe(connId, streamId): linear search, DetachTap, remove
    // HandleConnectionClosed(connId): linear search, DetachTap all matching
};
```

The legacy `stage_transition` topic maps to a tap on `$lifecycle` with a filter `event.type == kStageTransitionCommitted`. Other legacy topics get explicit aliases or 1:1 mappings as their stream is created.

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Streams/IStreamStore.h` | Add tap interface (AttachTap/DetachTap/GetTapCount); TapEvent, TapCallback, TapHandle |
| `Dia/DiaApplicationFlow/Streams/EventStreamStore.h` | Implement tap interface; extend SendInternal |
| `Dia/DiaApplicationFlow/Streams/FrameStreamStore.h` | Implement tap interface (Write fans to taps with envelope built from timestamp arg + writer module's id; sender is best-effort) |
| `Dia/DiaApplicationFlow/IApplicationInspectable.h` | Extend StreamInfo with new fields |
| `Dia/DiaApplicationFlow/Application.cpp` | Populate new StreamInfo fields in GetStreamInfo |
| `Dia/DiaApplicationFlow/StreamTypeRegistry.{h,cpp}` | Add serializer registration + SerializeToJson |
| `Dia/DiaApplicationFlow/RegistrationMacrosV2.h` | Add `DIA_STREAM_TYPE_WITH_SERIALIZER` |
| `Dia/DiaApplicationFlow/dia.application.architecture.module.md` | Update public_api, responsibilities |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj{,.filters}` | Add new files |
| `Dia/DiaDebugServer/SubscriptionManager.h` | **Delete** (AC8) |
| `Dia/DiaDebugServer/SubscriptionManager.cpp` | **Delete** (AC8) |
| `Dia/DiaDebugServer/DebugServer.h` | Replace `mSubscriptionManager` with `mClientTaps`; remove `BroadcastStageTransition` declaration |
| `Dia/DiaDebugServer/DebugServer.cpp` | Rewrite Subscribe/Unsubscribe/connection-close handlers as tap operations; remove BroadcastStageTransition; rewrite NotifySubscribers as topic→tap shim or remove if no remaining caller |
| `Dia/DiaDebugServer/DiaDebugServer.vcxproj{,.filters}` | Remove SubscriptionManager files; add dependency on DiaApplicationFlow tap API |
| `Dia/DiaDebugServer/dia.dia.diadebugserver.architecture.module.md` | Update public_api, dependencies, responsibilities |
| `Cluiche/CluicheGameBaseline/Types/InputEvent.h` | Replace `DIA_STREAM_TYPE(InputEvent)` with `DIA_STREAM_TYPE_WITH_SERIALIZER(InputEvent, SerializeInputEventToJson)`; provide serializer fn |
| `Cluiche/CluicheGameBaseline/Types/UICommand.h` | Same — `DIA_STREAM_TYPE_WITH_SERIALIZER(UICommand, ...)` |
| `Dia/DiaApplicationFlow/LifecycleEvent.cpp` | Add `DIA_STREAM_TYPE_WITH_SERIALIZER(LifecycleEvent, SerializeLifecycleEventToJson)` |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreamTap.cpp` | New — AC10/11/13 mock tap, count, stress |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestLifecycleEvents.cpp` | Extend with AC12 lifecycle-via-tap test |
| `Cluiche/Tests/GoogleTests/DiaDebugServer/TestDebugServerTaps.cpp` | New — verifies HandleSubscribe attaches a tap; HandleUnsubscribe detaches; connection close detaches all |

## Dependencies

- **Stream Topology — Manifest-Authoritative** (F1) — required; uses `IStreamStore`, `StreamTypeRegistry`, payload type tags
- **Stream Policy & Envelope** (F3) — required; tap callback receives envelope fields populated by F3
- **Lifecycle Events** (F2) — first-class consumer; debug server bridge taps `$lifecycle` for stage transitions
- **DiaDebugServer** — modified (SubscriptionManager removed)
- **DiaCore/Json** — `Json::Value` for serializer return type (already a DiaDebugServer dep)
- **DiaCore/Containers/DynamicArrayC** — tap entry storage
- **C++20 `<functional>`** — `std::function` for tap callbacks

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — streamId, payloadType, sender all StringCRC |
| PD-002 | Platform | PU/Stage/Module structure | Compliant — taps are observers on existing streams; introduce no new lifecycle |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL containers in public APIs | Compliant — public API uses `DynamicArrayC` for StreamInfo lists. `std::function` for `TapCallback` is necessary for runtime callback storage; same as legacy `MessageHandler`. Documented as the single allowed STL exception. |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant |
| PD-007 | Platform | C++20 | Compliant — `std::function`, `<functional>` |
| PD-008 | Platform | Directory.Build.props owns build settings | Compliant |
| PD-009 | Platform | Generated output in Cluiche/out/ | N/A |
| PD-010 | Platform | .diagame project root | N/A |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — module docs updated for both DiaApplicationFlow and DiaDebugServer |
| AD-002 | Dia App | No STL in public APIs | See PD-004 row — `std::function` documented exception |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant |
| AD-004 | Dia App | PU/Phase/Module | Compliant |
| AD-005 | Dia App | Component-based entities | N/A |
| SD-001 | DiaAppFlow | Config is sole source of truth for structural wiring | **Compliant with documented runtime exception** — taps are runtime debug observers, not structural wiring. Tap subscriptions are inherently dynamic (a debug client connects mid-run). They do not appear in the manifest by design. The structural wiring (which streams exist) remains config-driven. |
| SD-002 | DiaAppFlow | Stages replace phases | N/A |
| SD-003 | DiaAppFlow | ModuleRef<T> sole module access pattern | Compliant — taps do not access modules; they observe streams |
| SD-004 | DiaAppFlow | TransitionTo is app-wide | N/A |
| SD-005 | DiaAppFlow | Transitions execute next frame | N/A |
| SD-006 | DiaAppFlow | Streams framework-owned, declared in config | Compliant — taps observe; do not create streams |
| SD-007 | DiaAppFlow | MessageBus removed | Already done in F1; this feature replaces the *other* parallel pub/sub (DiaDebugServer's SubscriptionManager) |
| SD-008 | DiaAppFlow | One-liner registration macro | Compliant — `DIA_STREAM_TYPE_WITH_SERIALIZER` is the standard one-liner |
| SD-009 | DiaAppFlow | Module failure: assert debug, rollback release | N/A — taps are observational |
| SD-010 | DiaAppFlow | PU startup order | N/A |
| SD-011 | DiaAppFlow | Shutdown is framework-level | Compatible — taps detach automatically on store destruction; `Application::~Application` walks stores and clears taps |
| SD-012 | DiaAppFlow | Hot reload = re-enter stage | Compatible — stream stores survive stage transitions, so taps survive too |
| SD-013 | DiaAppFlow | DoStart returns StartResult | N/A |
| SD-014 | DiaAppFlow | Full manifest validation at load, fail-fast | Compliant — feature is runtime; manifest is unchanged |
| SD-015 | DiaAppFlow | No shared modules across PUs | Compliant |
| SD-016 | DiaAppFlow | IApplicationInspectable for runtime state | **Strengthens** — extends StreamInfo with payload type, policy, sequence, reader/tap counts |
| SD-017 | DiaAppFlow | Clean break, no v1 backward compat | **Compliant** — DiaDebugServer's SubscriptionManager is removed entirely (AC8). The legacy `stage_transition` topic alias is a runtime client-protocol compatibility shim, not a v1-system shim — it can be removed in a future protocol bump without engine impact. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Tap dispatch | Synchronous dispatch under store mutex puts tap latency on the writer's hot path. Is this safe for high-frequency streams? | The contract requires O(1) bookkeeping only (push to a queue, increment a counter). DiaDebugServer's tap handler will copy bytes and queue for the next WebSocket flush — well under a microsecond. Slow tap behaviour is a tap-author bug; the contract is documented. Future async-tap variant can be added if needed without breaking the API. |
| 2 | Tap dispatch | What if a tap callback throws? | Tap callbacks must not throw. `EventStreamStore::SendInternal` does not catch — an unhandled exception crashes the process. Same contract as `MessageHandler` had in v1. Documented. |
| 3 | Tap memory | `kMaxTapsPerStream = 8`. Why? | Matches `kMaxReaders` from F1's defaults. Realistic ceiling: one DebugServer connection's tap, one editor inspector tap, one telemetry tap, one recorder tap, plus headroom. If exceeded, AttachTap returns invalid TapHandle and logs a warning. |
| 4 | DebugServer migration | The legacy `stage_transition` topic is aliased to `$lifecycle` with a filter. How is the filter expressed? | Inside the DebugServer tap handler, not in the engine. `HandleSubscribe(stage_transition)` registers a tap on the `$lifecycle` store with a callback that ignores all events except `kStageTransitionCommitted` before forwarding to the WebSocket client. The engine's tap mechanism is filter-free; topic-level filtering is a DebugServer concern. |
| 5 | DebugServer migration | What about WebSocket clients that subscribe to topics that aren't streams (e.g. ad-hoc `NotifySubscribers` callers)? | Audit shows `NotifySubscribers` is called from at least one site outside `BroadcastStageTransition`. F4 plan includes auditing every call and either: (a) ensuring the topic corresponds to a stream that exists, registering an alias on first subscribe; (b) deleting the call site if the topic is dead; (c) keeping the call by adding a stream for that topic. No `NotifySubscribers` survives in its current form. |
| 6 | DebugServer migration | If the engine and DebugServer are split across compilation units, does adding tap as IStreamStore virtual methods break the ABI? | DiaDebugServer is statically linked against DiaApplicationFlow within the same Cluiche solution; ABI is rebuilt every solution build. No external consumers of IStreamStore exist. |
| 7 | Serializer | `DIA_STREAM_TYPE` from F1 still works without a serializer. Where is this advertised? | F1 stays the simple form; F4 adds a richer form. The F1 macro's docs (in `RegistrationMacrosV2.h`) gain a one-line note: "use `DIA_STREAM_TYPE_WITH_SERIALIZER` if the type needs WebSocket / debug-server forwarding." When `SerializeToJson` is called on an unregistered serializer, a single warning fires per type per process — enough to flag the omission without log spam. |
| 8 | Serializer | Where do serializer functions live? | Next to the type. Engine types: in their owning module's source (e.g. `LifecycleEvent.cpp` provides `SerializeLifecycleEventToJson`). Game types: in the game's `Types/` folder. The serializer is a free function; the macro registers it. |
| 9 | Inspection ABI | Adding fields to `StreamInfo` is a binary-compat break. Are there cross-build consumers? | No. `StreamInfo` is consumed only within the Cluiche solution. The struct is rebuilt with every Application change. |
| 10 | Sequence | Can a tap miss events under overflow? | Taps do not have buffers; they are called synchronously on every Send that accepts the event. If `Send` returns false (kFailLoud overflow rejection from F3), the event is *not* delivered to readers OR taps. If `Send` succeeds with drop-oldest/drop-newest, every tap fires for the events that "succeed" by the policy contract. Tap consumers can detect drops by observing sequence-number gaps. |
| 11 | Thread safety | Multiple threads attaching/detaching while Sends are flying — any race? | All four operations (AttachTap, DetachTap, SendInternal tap walk, GetTapCount) acquire the same store mutex. Order is: Send writes envelope to readers, then walks taps, then notifies condvar. Attach/Detach edit the tap list atomically under the mutex. No race; no lost events. |
| 12 | Stress test | The stress test (AC13) hits 4 readers + 4 taps × 10k events. Is that representative of real load? | It's the high end of plausible. CluicheTest at 60Hz × ~150 InputEvents/sec is two orders of magnitude under this. Stress mainly proves no envelope corruption / no deadlock; not a performance benchmark. |
| 13 | Lifecycle stream tap | F2's `$lifecycle` is read by Application internally and may be tapped by DebugServer. Are there ordering guarantees? | The store's mutex serializes all Sends; sequence numbers are monotonic. Taps see events in the same order Sends happened. The Application emitting (writer) and DebugServer tapping (observer) are both ordered through the store's mutex. |
| 14 | Detach during dispatch | What if a tap callback calls `DetachTap(self)` from inside its own callback? | The tap walk uses an index loop that re-reads the size each iteration. DetachTap on a tap that already fired this Send is fine; on a tap that hasn't fired yet, the loop would skip it. Documented: "callbacks may detach themselves; effect is observed on the *next* Send." Self-detach is safe. |
| 15 | Backwards-compat shim | The `stage_transition` legacy topic alias is a "client protocol compatibility shim". When does it go away? | A protocol version bump in DiaDebugProtocol can deprecate it. Out of scope for F4. The shim is small (~20 lines in DebugServer.cpp) and lives in DebugServer, not the engine. SD-017 ("clean break, no v1 backward compat") applies to engine systems; client-protocol compat is a separate concern. |

## Open Questions

None — all interview answers locked, all AI review questions answered.

## Status

`In Progress` — 2026-05-18. Tap API on IStreamStore + EventStreamStore implemented (AttachTap/DetachTap/GetTapCount, re-entrance guard, NotifyShutdown). DebugServer migration (F4-11 through F4-14) deferred to a separate workstream — requires NotifySubscribers audit first. F4-8 spec example update (SPSC queue pattern) still pending.
