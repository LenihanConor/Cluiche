# Research: Explore — Event Stream in DiaApplicationFlow

**Session date:** 2026-05-17
**Folder:** docs/research/event_stream_diapplic/

## Problem Space Overview

DiaApplicationFlow v2 already ships an **event stream** primitive — `EventStreamStore<T>` plus typed `EventStreamWriter<T>` / `EventStreamReader<T>` handles — that lets a Module on one ProcessingUnit fan discrete events out to one-or-many Modules on other PUs. Streams are owned by `Application`, looked up by `StringCRC` ID, and connected once at startup via `Module::OnConnectStreams()`. Today's mechanism is a per-reader fixed-capacity ring buffer with drop-oldest overflow, and it carries the live cross-PU traffic in CluicheTest (`InputToSim`, `SimToUI`, `UIToSim`).

Sitting next to it is `MessageBus` — the v1 type-erased Subscribe/Post/SendImmediate bus — explicitly marked superseded in the module's architecture doc. So the platform has already chosen typed streams over a string-keyed pub-sub bus, and the open question is **how far that primitive should be evolved into a general application-level event bus** without backsliding into the v1 design. Gaps appear when you want filtering, retention/replay, lifecycle-event introspection, debug taps, persistence, or richer envelope metadata (timestamps, sender ID, sequence numbers).

The research goal: survey what an "event stream" feature could mean in this codebase given what's already built, and produce a bounded list of candidate enhancements that improve the bus's reach without replacing the typed-channel model.

## Existing Approaches

- **Typed per-channel streams (current Dia model)** — one `EventStreamStore<T>` per StringCRC ID, one writer convention, N readers with per-reader buffers. Used by Cluiche today.
- **Type-erased pub-sub bus (legacy `MessageBus`)** — `std::function` handlers keyed by message-type StringCRC, payload as `void*`+size, immediate or queued dispatch. Superseded.
- **Observer / ObserverSubject (`DiaCore/Architecture/Observer.h`)** — classic GoF observer with integer message tag; in-process, single-thread oriented, used for low-volume signals.
- **Game-engine event bus patterns** — global "EventManager" with subscribe/publish (Source, Unreal `UGameplayMessageSubsystem`).
- **Channel-per-type with reflection** — UE5 GameplayMessage, Bevy `Events<T>` resource — typed reader/writer pairs with frame-windowed retention.
- **Disruptor / SPSC ring buffer** — high-throughput single-producer/single-consumer ring; readers track their own cursors; closely matches today's `EventStreamStore`.
- **Reactive streams (Rx-style)** — observables, operators (filter/map/merge); heavy and not idiomatic for a C++ engine.
- **Persistent log (Kafka/journal model)** — append-only log readers can replay from any offset; useful for recording/replay debugging.
- **Signals / slots (Qt, sigslot)** — direct callback wiring; no central bus, no decoupling between PUs.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Topology** | One-to-many (current); many-to-many; hierarchical (parent/child topics) | EventStreamStore::Send fans to all readers; multi-writer is allowed in code but not asserted. |
| **Typing** | Typed per stream (current); type-erased envelope; tagged-union envelope | Typed gives compile-time safety; erased gives debug-server / scripting reach. |
| **Identity** | StringCRC ID per stream (current); type-derived ID; namespaced (PU + stream); UUID | Current is flat StringCRC — collisions are silent. |
| **Delivery** | Drop-oldest (current); drop-newest; block writer; back-pressure/credit | Drop-oldest is fine for input but wrong for "module failed" lifecycle events. |
| **Ordering** | FIFO per reader (current); causal across streams; total order with seq numbers | No cross-stream ordering today. |
| **Retention** | Drained on consume (current); time-windowed; latest-N; full log | Current is consume-and-forget — no replay possible. |
| **Lifecycle** | Connect at startup (current); dynamic subscribe at runtime; stage-scoped subscribe | Today readers are wired pre-thread-launch and frozen. |
| **Filtering** | Manual in DoUpdate (current); predicate at consume; subtopic key | All events arrive at every reader; consumer-side discard is the only filter. |
| **Envelope metadata** | Bare `T` (current); timestamp; sender StringCRC; sequence; correlation ID | EventStreamStore has none of these; FrameStreamStore has timestamp. |
| **Cross-process reach** | In-process only (current); WebSocket relay; serialized stream tap | DiaDebugServer has its own SubscriptionManager — not unified with streams. |
| **Application lifecycle as events** | Polled via `IApplicationInspectable` (current); emitted on a well-known stream | Stage transitions and module state are observable but not eventful. |
| **Source of stream wiring** | Hard-coded module C++ (current); `.diaapp` manifest; data-driven topology | Connecting a stream is "code in OnConnectStreams" — not declared. |

## Known Tradeoffs

- **Typed channels vs erased bus** — typed is safer and zero-allocation but forces every new event class to recompile its writer + reader; erased lets debug tooling and scripts subscribe but loses compile-time safety.
- **Per-reader ring vs single ring** — per-reader fan-out (current) burns memory (`capacity × kMaxReaders × sizeof(T)` per stream) but isolates slow readers; a single ring with cursors can overflow if any reader stalls.
- **Drop-oldest vs blocking** — drop-oldest keeps producers free-running (good for input/render) but silently loses signals that may matter (a single "stage failed" event lost = whole rollback policy breaks).
- **Static connect vs dynamic subscribe** — static-at-startup is simple and lock-free at runtime, but rules out runtime tooling that wants to attach mid-stage (e.g. an editor debug panel).
- **Retention vs memory** — keeping a journal of events lets you record/replay/scrub, at the cost of unbounded growth and serialization work.
- **Manifest-declared streams vs code-declared** — declaring streams in `.diaapp` makes the topology inspectable and validatable but locks the channel API behind manifest schema changes.

## Known Pitfalls (C++ / game engine context)

- `static_cast<EventStreamStore<T>*>(istore)` in `EventStreamWriter::Connect` trusts that the first registrant's `T` matches the second — type mismatch is undefined behavior, no runtime check.
- `kMaxReaders = 8` and `kMaxStreams = 16` are baked into class statics — no compile error if you exceed them, just a silent registration failure (`-1` reader index, `nullptr` store).
- `Send` lock is held while iterating all reader buffers; a fast-frequency stream with 8 readers serializes every send.
- `std::function` (in `MessageBus`) heap-allocates for non-trivial captures — main reason it was superseded.
- Subscribe-at-runtime races against dedicated PU threads if not gated; v2 deliberately closes that door at `Start()`.
- StringCRC collisions are silent — two streams with hash-equal IDs would alias the same store.
- Drop-oldest under contention can shred event correlation: the "open" event of a session is dropped while the "close" is delivered.
- C++20 templates inflate compile times when many distinct `T` instantiate `EventStreamStore<T>`; current header-only inline implementation pays this cost in every TU that includes it.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaApplicationFlow** | Owns `EventStreamStore<T>`, `Event/StreamWriter/Reader`, `Application`, `Module::OnConnectStreams`. Direct home of any extension. |
| **DiaCore** | StringCRC, `DynamicArrayC<T,N>`, atomics, `TimeAbsolute`. Existing primitives an upgraded bus would compose on. |
| **DiaCore/Architecture/Observer** | Lower-tier in-process notification — could become the per-PU same-thread cousin of the cross-PU stream. |
| **DiaLogger** | Already the channel for warnings (`DIA_LOG_WARNING` from EventStreamStore overflow). A unified event-tap could publish lifecycle events through here. |
| **DiaDebugServer / DiaDebugProtocol** | Has its own `SubscriptionManager` and subscribes to Application state separately from streams. Candidate for unification. |
| **DiaSerializer** | Already a dependency of DiaApplicationFlow — required for any "event journal" or persistence candidate. |
| **DiaWebSocket** | Transport for any cross-process tap. |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| **PD-001 StringCRC for IDs** | Stream IDs are already StringCRC; any new event taxonomy must follow. |
| **PD-002 ProcessingUnit/Phase/Module** | Event lifecycle is bounded by PU update tick + Module start/stop; runtime subscribe must respect PU thread affinity. |
| **PD-003 Component-based entities** | If events flow across the entity layer, must integrate with `IComponent` not bypass it. |
| **PD-004 No STL containers in public APIs** | Today `MessageBus` uses `std::deque`, `std::vector`, `std::unordered_map` — non-compliant. EventStreamStore uses raw arrays — compliant. New work must follow EventStream's pattern. |
| **PD-005 x64 Windows only** | Atomics and memory ordering can assume x86-TSO. |
| **PD-007 C++20 required** | Concepts are available — could constrain `EventStreamStore<T>` to trivially-copyable. |

## Open Questions for Ideation

- Should application **lifecycle events** (stage transition started/committed, module failed, shutdown requested, rollback attempted) become first-class events on a well-known stream, or stay as `IApplicationInspectable` polling?
- Is there a need for a **runtime subscribe** path (editor / debug server attaching mid-run), or does startup-only connect cover every real consumer?
- Should the stream layer carry **timestamps + sender + sequence** in the envelope, or stay bare-`T` and let each event type embed its own metadata?
- Is **drop-oldest** the right default for every stream, or do some streams need block / drop-newest / fail-loudly?
- Should there be an **event journal / replay** facility, or is that out of scope for the live bus and a separate recording subsystem instead?
- Is **manifest-declared stream topology** worth the schema cost — i.e. should `.diaapp` list streams, kinds, and writer/reader bindings — or is code-side declaration sufficient?
- Should the **debug server's SubscriptionManager** be unified with streams (one mechanism, taps over WebSocket), or kept separate?
- Should typed streams be augmented with a **type-tagged envelope** for cross-type taps (debug logger, recorder), and if so, how does that interact with PD-004?
- Are **per-reader rings** still right, or should we move to a single-ring + cursor model now that we know typical reader counts?
- Should there be a **same-PU low-overhead variant** of the stream (no mutex, no per-reader buffers) for intra-PU module fan-out, given Observer is the only current option?
