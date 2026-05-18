# Feature Spec: Lifecycle Events

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Research
@docs/research/event_stream_diapplic/summary.md

## Depends On
- @docs/specs/features/dia/diaapplicationflow/stream-topology-manifest.md (F1, Approved)
- @docs/specs/features/dia/diaapplicationflow/stream-policy-envelope.md (F3, Approved)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Lifecycle Events | (this file) |

## Problem Statement

The Application's lifecycle transitions — module state changes, stage transitions, rollback attempts, shutdown — are observable today only via `IApplicationInspectable` polling. Every consumer (`DiaDebugServer`, future editor live view, telemetry sink, replay recorder) reinvents its own polling loop or maintains a parallel subscription mechanism (`DiaDebugServer/SubscriptionManager`). The audit's biggest functional gap.

This feature makes lifecycle observable as **subscribable events on a well-known stream**. Application emits a `LifecycleEvent` tagged-union onto a reserved `$lifecycle` event stream whenever a state change occurs. Any module — including future debug taps, telemetry sinks, and recorders — consumes them through the same `EventStreamReader<LifecycleEvent>` API used for game streams. Polling is preserved for snapshot use cases; subscription replaces "polling at frame rate to detect a change."

## Acceptance Criteria

1. **AC1 — `$lifecycle` stream auto-injected.** `Application::Start()` creates a reserved EventStream with id `StringCRC("$lifecycle")`, payload type `LifecycleEvent`, kind `EventStream`, capacity `1024`, policy `drop-oldest`. Manifest does NOT need to declare it.
2. **AC2 — `$`-prefix reservation.** `ManifestValidatorV2` rejects any user-declared stream id, module instance id, or sender StringCRC starting with `$` with a distinct error code (`RESERVED_PREFIX`). Tests cover collision attempts.
3. **AC3 — Six lifecycle event types.** `LifecycleEvent::Type` enum: `kModuleStateChanged`, `kStageTransitionRequested`, `kStageTransitionStarted`, `kStageTransitionCommitted`, `kRollbackAttempted`, `kShutdownRequested`.
4. **AC4 — Module state events.** Whenever a module's `mState` transitions (Inactive↔Starting↔Active↔Stopping↔Failed), Application emits `kModuleStateChanged{ moduleId, puId, oldState, newState }`. Emitted from the same thread that performed the state write (PU thread for `FrameTick` transitions; main thread for `BeginStart`/`BeginStop`). Stream's mutex covers cross-thread safety.
5. **AC5 — Stage transition events.** `TransitionTo()` emits `kStageTransitionRequested{ from, to }` synchronously. `ApplyPendingTransition` emits `kStageTransitionStarted{ from, to }`. `TickTransitionDrain` emits `kStageTransitionCommitted{ from, to }` when the new stage is live.
6. **AC6 — Rollback events.** When `Update` detects `AnyModuleFailed` in a non-boot stage, it emits `kRollbackAttempted{ stage, attempt, maxAttempts }` before initiating the rollback.
7. **AC7 — Shutdown event.** `RequestShutdown()` emits `kShutdownRequested{}` on its first invocation (the existing `mShuttingDown.exchange(true)` guards repeats).
8. **AC8 — Reserved sender.** The framework's writer for `$lifecycle` uses `sender = StringCRC("$application")`. F3's "sender = writer module's instanceId" invariant is relaxed: sender may be any reserved `$`-prefix StringCRC for framework-emitted events. Caller modules cannot pass sender; framework still controls it (per F3 AC2).
9. **AC9 — Per-event-type GoogleTests.** `Cluiche/Tests/GoogleTests/ApplicationFlow/TestLifecycleEvents.cpp` covers each of the six event types: trigger the framework action, attach a reader to `$lifecycle`, assert the event arrives with correct payload and envelope (sender = `$application`, sequence monotonic, timestamp non-decreasing).
10. **AC10 — Reserved-prefix validator coverage.** GoogleTests in `TestStreamTopology.cpp` (created in F1) extended with: user-declared stream `$foo` → validator error; user module instance id `$bar` → validator error; user sender via fail-loud `$baz` (synthesized by attempting to register a module type with that id) → validator error.
11. **AC11 — CluicheTest sanity.** A new GoogleTest boots a minimal Cluiche-like application and asserts the boot sequence emits, in order: `kStageTransitionRequested(Boot)`, `kStageTransitionStarted(?, Boot)`, [N×`kModuleStateChanged`], `kStageTransitionCommitted(?, Boot)`. Confirms ordering invariants under realistic load.
12. **AC12 — Existing log lines preserved.** `DIA_LOG_INFO/ERROR` calls at every emission point remain — the lifecycle stream is additive, not a replacement. Logs and events stay in lockstep.

## Public API

```cpp
namespace Dia::ApplicationFlow {

    // Tagged-union lifecycle event payload.
    struct LifecycleEvent
    {
        enum class Type : unsigned int
        {
            kModuleStateChanged       = 0,
            kStageTransitionRequested = 1,
            kStageTransitionStarted   = 2,
            kStageTransitionCommitted = 3,
            kRollbackAttempted        = 4,
            kShutdownRequested        = 5,
        };

        struct ModuleStateChanged {
            Dia::Core::StringCRC moduleId;
            Dia::Core::StringCRC puId;
            ModuleState          oldState;
            ModuleState          newState;
        };

        struct StageTransition {
            Dia::Core::StringCRC from;   // StringCRC::kZero on Boot
            Dia::Core::StringCRC to;
        };

        struct RollbackAttempted {
            Dia::Core::StringCRC stage;
            unsigned int         attempt;
            unsigned int         maxAttempts;
        };

        // Empty: kShutdownRequested carries no payload data
        struct ShutdownRequested {};

        Type type;
        union {
            ModuleStateChanged moduleStateChanged;
            StageTransition    stageTransition;     // shared by Requested/Started/Committed
            RollbackAttempted  rollbackAttempted;
            ShutdownRequested  shutdownRequested;
        };
    };

    // Static-init registers the type with StreamTypeRegistry (F1).
    DIA_STREAM_TYPE(LifecycleEvent);

    // Well-known IDs.
    namespace Reserved {
        inline const Dia::Core::StringCRC kLifecycleStreamId{"$lifecycle"};
        inline const Dia::Core::StringCRC kApplicationSender{"$application"};
    }
}
```

**Subscriber example (illustrative):**
```cpp
class MyDebugModule : public Module
{
    EventStreamReader<LifecycleEvent> mLifecycle{this, Reserved::kLifecycleStreamId};

    void OnConnectStreams(Application& app) override { mLifecycle.Connect(app); }

    void DoUpdate(float) override
    {
        DynamicArrayC<Event<LifecycleEvent>, 64> events;
        mLifecycle.Consume(events);
        for (const auto& e : events)
        {
            switch (e.payload.type) { /* ... */ }
        }
    }
};
```

For the subscriber's manifest entry, `reads: ["$lifecycle"]` is permitted (validator allows `$lifecycle` specifically as a `reads` target — see AI Q9).

## Internal Architecture

```
Application owns:
  EventStreamWriter<LifecycleEvent> mLifecycleWriter   // sender = "$application"

Application::Start():
  - After F1's manifest store creation, also auto-create the $lifecycle store
    (capacity=1024, policy=drop-oldest, payloadType=StringCRC("LifecycleEvent"))
  - Connect mLifecycleWriter to the auto-created store
  - Validator's reads/writes binding (F1 AC2) is bypassed for $-prefix readers
    of $lifecycle: any module may read $lifecycle by listing it in manifest reads.

Emission points (instrumented in existing code paths):
  Module::FrameTick state transitions   -> kModuleStateChanged (PU thread)
  Module::BeginStart / BeginStop        -> kModuleStateChanged (main thread)
  Application::TransitionTo             -> kStageTransitionRequested
  Application::ApplyPendingTransition   -> kStageTransitionStarted
  Application::TickTransitionDrain      -> kStageTransitionCommitted
  Application::Update (rollback branch) -> kRollbackAttempted
  Application::RequestShutdown          -> kShutdownRequested
```

`Module::FrameTick` runs on the PU's thread; the lifecycle store's mutex (per F3) makes cross-thread Send safe. `Module::BeginStart`/`BeginStop` and Application transitions run on the main thread. The reserved writer is single-handle, multi-thread; F3's writer is owned by a `Module*`, so the framework's writer is a special case using `nullptr` owner with an explicit reserved sender override (see AI Q1).

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/LifecycleEvent.h` | New — `LifecycleEvent` struct + Reserved IDs |
| `Dia/DiaApplicationFlow/LifecycleEvent.cpp` | New — `DIA_STREAM_TYPE(LifecycleEvent)` definition |
| `Dia/DiaApplicationFlow/Application.h` | Add `EventStreamWriter<LifecycleEvent> mLifecycleWriter` member; helper `EmitLifecycle(LifecycleEvent)` private method |
| `Dia/DiaApplicationFlow/Application.cpp` | Auto-create $lifecycle store in Start; emit at the 7 emission points listed |
| `Dia/DiaApplicationFlow/Module.cpp` | Emit `kModuleStateChanged` on every `mState.store(...)` (FrameTick + BeginStart + BeginStop) — must hold a back-channel to the Application's writer |
| `Dia/DiaApplicationFlow/Streams/EventStreamWriter.h` | Add framework-internal constructor: `EventStreamWriter(const StringCRC& streamId, const StringCRC& reservedSender)` — no Module owner; sender override path is reserved-only and asserts on non-`$` sender |
| `Dia/DiaApplicationFlow/Manifest/ManifestValidatorV2.cpp` | New error code `RESERVED_PREFIX`; reject user $-prefix on stream id, module instance id |
| `Dia/DiaApplicationFlow/dia.application.architecture.module.md` | Add LifecycleEvent + $lifecycle to public_api; add lifecycle emission to responsibilities |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj{,.filters}` | Add new files |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestLifecycleEvents.cpp` | New — per-event-type emission tests (AC9) + CluicheTest sanity boot-sequence test (AC11) |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreamTopology.cpp` | Extend with $-prefix collision tests (AC10) |

## Dependencies

- **Stream Topology — Manifest-Authoritative** (F1) — required; uses `DIA_STREAM_TYPE`, `StreamTypeRegistry`, manifest-authoritative store creation, validator extension hooks
- **Stream Policy & Envelope** (F3) — required; uses `Event<T>` envelope (LifecycleEvent gets timestamp/sender/sequence), `OverflowPolicy::kDropOldest`, framework writer's sender override path
- **DiaCore/CRC/StringCRC** — reserved IDs
- **DiaCore/Containers/DynamicArrayC** — reader-side consume
- **DiaLogger** — DIA_LOG_INFO/ERROR remain in lockstep with emission

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — stream id, sender, module/stage/PU ids all StringCRC |
| PD-002 | Platform | PU/Stage/Module structure | Compliant — emission instruments the existing PU/Stage/Module lifecycle without changing it |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `LifecycleEvent` is a POD union; reader API uses `DynamicArrayC<Event<LifecycleEvent>, N>` per F3 |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant |
| PD-007 | Platform | C++20 | Compliant |
| PD-008 | Platform | Directory.Build.props owns build settings | Compliant |
| PD-009 | Platform | Generated output in Cluiche/out/ | N/A |
| PD-010 | Platform | .diagame project root, .diastage stage metadata | Compliant — feature lives in DiaApplicationFlow code; consumer manifests may add `$lifecycle` to a module's `reads` |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — module doc updated |
| AD-002 | Dia App | No STL in public APIs | Compliant |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::ApplicationFlow::` |
| AD-004 | Dia App | PU/Phase/Module | Compliant |
| AD-005 | Dia App | Component-based entities | N/A |
| SD-001 | DiaAppFlow | Config is sole source of truth for structural wiring | **Compliant with documented exception** — `$lifecycle` is auto-created, not declared. The reservation is a framework axiom (every Application has it), not a per-game wiring choice. Manifest still controls *who reads* it via `module.reads`. Documented in module architecture file. |
| SD-002 | DiaAppFlow | Stages replace phases | N/A |
| SD-003 | DiaAppFlow | ModuleRef<T> sole module access pattern | Compliant — feature does not change module access |
| SD-004 | DiaAppFlow | TransitionTo is app-wide | Compliant — kStageTransition* events reflect app-wide transitions |
| SD-005 | DiaAppFlow | Transitions execute next frame | Compliant — kStageTransitionRequested fires synchronously on TransitionTo, kStageTransitionStarted fires at the top of the next Update; the gap is observable |
| SD-006 | DiaAppFlow | Streams framework-owned, declared in config | **Compliant with documented exception** — see SD-001 row. Stream is framework-owned (yes, by Application directly); the "declared in config" half is relaxed for the reserved $-namespace. |
| SD-007 | DiaAppFlow | MessageBus removed | Already done in F1 — this feature does not reintroduce a parallel pub/sub |
| SD-008 | DiaAppFlow | One-liner registration macro | Compliant — `DIA_STREAM_TYPE(LifecycleEvent)` is the standard one-liner |
| SD-009 | DiaAppFlow | Module failure: assert debug, rollback release, shutdown on boot | Compliant — emission is observational, does not change failure handling. `kRollbackAttempted` is fired *before* the rollback so a subscriber can react. |
| SD-010 | DiaAppFlow | PU startup order = array order | N/A |
| SD-011 | DiaAppFlow | Shutdown is framework-level (RequestShutdown) | Compliant — kShutdownRequested fires once on the first call |
| SD-012 | DiaAppFlow | Hot reload = re-enter stage | Compliant — re-enter emits the same kStageTransitionRequested/Started/Committed sequence |
| SD-013 | DiaAppFlow | DoStart returns StartResult | Compliant — kModuleStateChanged covers transitions to kFailed |
| SD-014 | DiaAppFlow | Full manifest validation at load, fail-fast | Compliant — `RESERVED_PREFIX` is a fail-fast validator error |
| SD-015 | DiaAppFlow | No shared modules across PUs | Compliant — feature is framework-side, not a module |
| SD-016 | DiaAppFlow | IApplicationInspectable for runtime state | **Coexists** — inspectable remains for snapshot queries; lifecycle stream covers change-event subscriptions. The two are complementary, not duplicative. |
| SD-017 | DiaAppFlow | Clean break, no v1 backward compat | Compliant — additive feature on the v2 system |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Framework writer | F3 AC2 says "sender = owner->GetInstanceId()" — but Application is not a Module. How does its writer get a sender? | A new framework-internal `EventStreamWriter<T>` constructor takes an explicit reserved sender StringCRC and `nullptr` owner. The writer asserts `sender.AsChar()[0] == '$'` to prevent abuse. F3's invariant becomes: "sender is either the writer's owning module instanceId, or a reserved `$`-prefix StringCRC for framework writers." This generalisation is small and lives in F2's diff. |
| 2 | $-prefix reservation | Why `$` and not another prefix? | `$` is JSON-safe, doesn't need quoting in the StringCRC string source, doesn't appear in any current Cluiche identifier (greppable), and reads as a clear "system" sigil to humans (cf. shell, Stripe API conventions). |
| 3 | Module state granularity | `kModuleStateChanged` fires on every state edge — Inactive→Starting→Active is 2 events per startup. Volume concern? | At capacity 1024, the lifecycle stream holds ~170 module starts (assuming 6 events per module across boot + transition). For CluicheTest's ~10 modules per stage = 60 events per transition. Well within budget. Replay scenarios with many transitions may want larger capacity; capacity is configurable via the auto-create defaults. |
| 4 | Subscriber thread affinity | A `$lifecycle` subscriber's PU may be different from the emitting PU. Per-reader fan-out (F3) handles this — but is there a same-thread emit/consume race? | No. The store's mutex serializes Send with Consume per F3's contract. A reader on the same PU as the writer (e.g. a debug module on MainPU) sees a self-consistent snapshot. |
| 5 | Manifest reads validation | F1 AC2 says `Connect()` verifies `streamId` is in the module's `reads`. Does it accept `$lifecycle` even though `$lifecycle` is not in `manifest.streams[]`? | Yes. The validator special-cases reserved `$`-prefix stream IDs: they are valid `reads` targets even though they don't appear in `manifest.streams[]`. New validator path: `if (streamId.starts_with('$') && IsReservedStream(streamId)) accept;`. Tests in AC10 cover this. |
| 6 | Sequence interleaving | `kModuleStateChanged` fires from multiple threads (PU + main). The lifecycle store's per-stream sequence (F3 AC3) is atomic, so events are well-ordered globally. But "well-ordered relative to the source thread"? | F3 sequence is monotonic across all writers to the store. Order is determined by which thread acquires the store mutex first. This is sufficient for subscribers — replay can compose sequence with timestamp to derive happens-before relations. We do not promise per-thread sub-ordering beyond what `std::mutex` already provides. |
| 7 | Self-events on $lifecycle | Does emission of `kModuleStateChanged` for a debug module's own lifecycle hit the $lifecycle stream the debug module is reading? | Yes. The debug module's own state transitions are observable like any other module's. This is by design — replay needs the full state graph including debug modules. |
| 8 | Bootstrapping race | `Application::Start()` creates $lifecycle store *after* the manifest validation phase but *before* module construction. What if validation itself fails? | Validation errors are still surfaced via `DIA_LOG_ERROR` and the `Start()` `false` return value. The lifecycle stream is for runtime events; pre-stream validation failures don't emit events because no store exists yet. AI Q11 below covers this further. |
| 9 | Editor / debug-server consumption | F4 (stream tap) builds the type-erased subscription path used by DiaDebugServer. Does F2 ship without a working debug-server consumer? | Yes. F2 produces the stream and emission; F4 wires DiaDebugServer onto it. Splitting keeps F2 self-contained. Until F4 lands, lifecycle events are consumable via a normal `EventStreamReader<LifecycleEvent>` from inside the engine — sufficient for unit tests and any in-process consumer. |
| 10 | StringCRC ASCII contract | Is `StringCRC("$application")` valid given the 64-char internal buffer? | Yes — `$application` is 12 chars, well under the 64-char `kStringLength`. CRC produces a valid 32-bit value as for any string. |
| 11 | Manifest validation errors | Manifest validation runs *before* module construction. If a manifest has a `RESERVED_PREFIX` error, no `$lifecycle` stream exists yet. How does that error surface? | Same path as every other manifest error today (F1 path): `DIA_LOG_ERROR` + `Start()` returns false. No event emission — there is no Application to emit from. Tests assert via the existing validator error API, not via the stream. |
| 12 | Performance impact | Adding emission at every module state transition adds work to FrameTick. Cost? | Per state transition: one mutex acquire on the lifecycle store, one envelope copy per registered reader, sequence atomic increment. With zero readers (production with no debug subscribers): mutex + atomic, no fan-out work. Measurable but not in the 60Hz hot path. |
| 13 | StageTransition payload | All three stage events (Requested/Started/Committed) share the `StageTransition { from, to }` shape. Why one struct instead of three? | They genuinely carry the same data. The event `type` enum disambiguates. Reduces struct count in the union. Future events (e.g. kStageTransitionFailed with error info) would use a different struct. |
| 14 | Tests for ordering | AC11 asserts boot-sequence ordering. Is "in order" deterministic given multi-threaded modules? | Per-event-type ordering is deterministic (Requested fires before Started fires before Committed for any one transition; this is sequenced by the framework's own code). `kModuleStateChanged` events for different modules within a transition are partially ordered: the test asserts the bracketing events but counts module events without asserting their relative order. |
| 15 | LifecycleEvent struct ABI | Tagged-union with non-trivial members — is the union safe? | All four union members are POD: `StringCRC` (POD), `ModuleState` (enum), primitives. No constructors/destructors needed. The union is C-style safe under C++20. Writer copies the active variant; reader switches on `type` and reads only that variant. |

## Open Questions

None — all interview answers locked, all AI review questions answered.

## Status

`Done` — 2026-05-18. LifecycleEvent.h/.cpp, $lifecycle store auto-created in Application::Start(), framework-internal writer, 6 emission points (stage transition requested/started/committed, rollback, shutdown, module state changed). Build and ApplicationFlow* tests passing.
