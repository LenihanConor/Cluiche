# Feature Spec: Stream Policy & Envelope

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Research
@docs/research/event_stream_diapplic/summary.md

## Depends On
@docs/specs/features/dia/diaapplicationflow/stream-topology-manifest.md (F1, Approved)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Stream Policy & Envelope | (this file) |

## Problem Statement

EventStream today has **one** overflow policy (drop-oldest with a warning) and **zero** framework-supplied event metadata — the buffered value is bare `T`. Both choices are baked into `EventStreamStore<T>` as code constants. That has three consequences:

1. **Correctness streams (lifecycle, network, save) have no way to refuse silent loss.** A "module failed" event is as droppable as an input keystroke. The audit's biggest functional gap.
2. **Every payload that wants timestamp / sender / sequence has to embed them itself.** No replay, no ordered debug view, no telemetry attribution without per-payload boilerplate.
3. **Downstream features are blocked.** F2 (lifecycle events) needs a fail-loud option. F4 (stream tap) needs envelope metadata to display useful info to the debug server. Both wait on this feature.

This feature adds (a) per-stream overflow policy declared in the manifest with four options (drop-oldest, drop-newest, block-with-timeout, fail-loud), and (b) a framework-supplied `Event<T>` envelope carrying `TimeAbsolute timestamp`, `StringCRC sender`, and `uint64 sequence` on every event.

## Acceptance Criteria

1. **AC1 — `Event<T>` envelope.** `EventStreamStore<T>` buffers `Event<T> = { TimeAbsolute timestamp; StringCRC sender; uint64 sequence; T payload; }` (not bare `T`). Reader's `Consume` returns `DynamicArrayC<Event<T>, N>&`.
2. **AC2 — Framework-stamped fields.** On `EventStreamWriter<T>::Send(payload)`, the framework stamps `timestamp = TimeAbsolute::GetSystemTime()`, `sender = owner->GetInstanceId()`, `sequence = store.NextSeq()`. Callers cannot pass these fields; they cannot be omitted or spoofed.
3. **AC3 — Per-stream sequence number.** `EventStreamStore<T>` holds a `uint64` counter starting at 0, incremented atomically on every `Send`. `Event<T>::sequence` is monotonic per stream; observable invariant.
4. **AC4 — Four overflow policies.** Each `EventStreamStore<T>` has one of: `kDropOldest` (current behaviour: warn + advance tail), `kDropNewest` (drop incoming + warn), `kBlockWithTimeout` (Send waits on a condvar up to manifest-declared `block_timeout_ms`, then falls back to drop-oldest with warning), `kFailLoud` (DIA_ASSERT in debug; return `false` + DIA_LOG_ERROR in release).
5. **AC5 — `Send` signature change.** `EventStreamWriter<T>::Send(const T&)` returns `bool`. `true` for success (including drop-oldest/drop-newest cases — they "succeeded" by their own contract). `false` only for kFailLoud overflow.
6. **AC6 — Manifest schema.** Stream entries gain `"overflow"` (string: one of `"drop-oldest"`, `"drop-newest"`, `"block"`, `"fail-loud"`; default `"drop-oldest"`) and `"block_timeout_ms"` (integer, default 10, only meaningful for `"block"`). Validator rejects unknown policy strings.
7. **AC7 — FrameStream untouched.** FrameStream<T> has no envelope, no policy, no `Send` return value. Manifest fields `overflow` / `block_timeout_ms` on a `kind: "FrameStream"` entry produce a validator warning and are ignored.
8. **AC8 — CluicheTest migration.** `cluiche_main.diaapp` updated: each event stream declares an explicit `overflow` policy (all current streams = `drop-oldest`, the existing default — no behaviour change). Existing readers (`UIModule.mUICommands`, `InputStreamModule.mInput`, GoogleTests `TestStreams.cpp`) updated to use `.payload` accessor.
9. **AC9 — Per-policy unit tests.** Each of the four policies has dedicated GoogleTests in `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreamPolicy.cpp`: drop-oldest preserves last-N, drop-newest preserves first-N, block returns within timeout under reader-stall, fail-loud asserts in debug + returns false in release.
10. **AC10 — Envelope unit tests.** GoogleTests verify (a) timestamp is non-decreasing across consecutive Sends, (b) sender == writer module's instanceId, (c) sequence is contiguous and monotonic, (d) per-reader fan-out delivers identical envelope to every reader.

## Public API

```cpp
namespace Dia::ApplicationFlow {

    // Framework-supplied event envelope.
    template<typename T>
    struct Event
    {
        Dia::Core::TimeAbsolute timestamp;
        Dia::Core::StringCRC    sender;
        unsigned long long      sequence;
        T                       payload;
    };

    enum class OverflowPolicy
    {
        kDropOldest,        // default; current behaviour
        kDropNewest,
        kBlockWithTimeout,
        kFailLoud
    };

    // Reader API change: Consume returns Event<T>, not T.
    template<typename T>
    class EventStreamReader
    {
    public:
        EventStreamReader(Module* owner, const StringCRC& streamId);

        template<unsigned int N>
        void Consume(DynamicArrayC<Event<T>, N>& outEvents);

        bool HasPending() const;
        bool IsConnected() const;
        void Connect(Application& app);
    };

    // Writer API: Send returns bool (true = delivered or dropped per policy;
    //                                false = kFailLoud overflow).
    template<typename T>
    class EventStreamWriter
    {
    public:
        EventStreamWriter(Module* owner, const StringCRC& streamId);

        bool Send(const T& payload);    // signature change: void -> bool
        bool IsConnected() const;
        void Connect(Application& app);
    };

    // Store gains policy + sequence counter.
    template<typename T>
    class EventStreamStore : public IStreamStore
    {
    public:
        EventStreamStore(const StringCRC& id,
                         const StringCRC& payloadType,
                         OverflowPolicy   policy            = OverflowPolicy::kDropOldest,
                         unsigned int     blockTimeoutMs    = 10,
                         unsigned int     capacity          = kDefaultCapacity,
                         unsigned int     maxReaders        = kDefaultMaxReaders);

        // Internal — called only by EventStreamWriter<T>::Send.
        bool SendInternal(const Event<T>& envelope);

        // ...
    private:
        unsigned long long NextSeq();   // atomic increment
        std::atomic<unsigned long long> mNextSequence{0};
        OverflowPolicy                  mPolicy;
        unsigned int                    mBlockTimeoutMs;
        std::condition_variable         mNotFullCv;   // kBlockWithTimeout only
    };
}
```

## Manifest Schema

**v2.2 stream entry (extends F1's v2.1):**
```json
{
  "id": "InputToSim",
  "kind": "EventStream",
  "payload_type": "InputEvent",
  "from": "MainPU",
  "to": "SimPU",
  "capacity": 256,
  "max_readers": 1,
  "overflow": "drop-oldest",      // NEW; default "drop-oldest"
  "block_timeout_ms": 10          // NEW; only for "block", default 10
}
```

`overflow` accepts: `"drop-oldest"`, `"drop-newest"`, `"block"`, `"fail-loud"`. Unknown strings → validator error. `block_timeout_ms` ignored unless `overflow == "block"`.

For `kind: "FrameStream"`, both fields are accepted but flagged with a warning and ignored.

## Internal Architecture

**Send dispatch by policy (inside `EventStreamStore<T>::SendInternal`):**
```
acquire mMutex

if any reader buffer is full:
    switch (mPolicy):
        kDropOldest:    advance tail, decrement count, log warning
        kDropNewest:    release mutex, log warning, return true
        kBlockWithTimeout:
            mNotFullCv.wait_for(lock, blockTimeoutMs, predicate=any-reader-has-space)
            if still full: fall through to kDropOldest
        kFailLoud:
            release mutex
            DIA_ASSERT in debug
            DIA_LOG_ERROR + return false in release

write envelope to all reader buffers
release mMutex
notify_all on mNotFullCv (consumers wake any blocked writer)
return true
```

**Consume side (`EventStreamStore<T>::Consume`)** also notifies `mNotFullCv` after draining a reader's buffer, so blocked writers wake up promptly.

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Streams/Event.h` | New — `Event<T>` envelope struct |
| `Dia/DiaApplicationFlow/Streams/OverflowPolicy.h` | New — enum + parser from `StringCRC` |
| `Dia/DiaApplicationFlow/Streams/EventStreamStore.h` | Buffer changes from `T` to `Event<T>`; add policy + sequence + condvar |
| `Dia/DiaApplicationFlow/Streams/EventStreamWriter.h` | `Send` signature `void → bool`; stamps envelope before forwarding to store |
| `Dia/DiaApplicationFlow/Streams/EventStreamReader.h` | `Consume<N>(DynamicArrayC<Event<T>, N>&)` |
| `Dia/DiaApplicationFlow/Streams/IStreamStore.h` | (no change) |
| `Dia/DiaApplicationFlow/Streams/FrameStreamStore.h` | (no change) |
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestV2.h` | `StreamDeclaration`: add `OverflowPolicy overflow`, `unsigned int blockTimeoutMs` |
| `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.cpp` | Parse new fields; warn for FrameStream |
| `Dia/DiaApplicationFlow/Manifest/ManifestValidatorV2.cpp` | Reject unknown overflow strings; warn FrameStream + overflow combo |
| `Dia/DiaApplicationFlow/Application.cpp` | Pass overflow + blockTimeoutMs into store ctor when creating from manifest |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj{,.filters}` | Add new files |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Add explicit `"overflow": "drop-oldest"` to all event streams (no behaviour change; explicit > implicit) |
| `Cluiche/CluicheGameBaseline/Modules/InputStreamModule.cpp` | Update `mInput.Consume` call site to use `Event<InputEvent>::payload` |
| `Cluiche/CluicheGameBaseline/Modules/UIModule.cpp` | Update `mUICommands.Consume` call site for `Event<UICommand>::payload` |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreams.cpp` | Update reader assertions to read `.payload` |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreamPolicy.cpp` | New — per-policy tests (AC9) |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestStreamEnvelope.cpp` | New — envelope correctness tests (AC10) |

## Dependencies

- **Stream Topology — Manifest-Authoritative** (F1) — required; this feature extends F1's manifest schema, type registry, and validator paths
- **Streams** (current feature) — superseded together with F1
- **DiaCore/Time/TimeAbsolute** — `TimeAbsolute::GetSystemTime()` for envelope timestamp
- **DiaCore/CRC/StringCRC** — `sender` field; policy parsing
- **C++20 `<atomic>`, `<condition_variable>`** — kBlockWithTimeout

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — `Event<T>::sender` is `StringCRC` (writer module's instanceId) |
| PD-002 | Platform | PU/Stage/Module structure | Compliant — feature operates inside existing Module lifecycle |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL containers in public APIs | Compliant — public API uses `DynamicArrayC<Event<T>, N>`. Internal `std::condition_variable` is implementation detail. |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant |
| PD-007 | Platform | C++20 | Compliant — uses `std::atomic`, `std::condition_variable_any`, `std::chrono` |
| PD-008 | Platform | Directory.Build.props owns build settings | Compliant |
| PD-009 | Platform | Generated output in Cluiche/out/ | N/A |
| PD-010 | Platform | .diagame project root, .diastage stage metadata | Compliant — schema extension is on `.diaapp` files |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — module doc updated to mention overflow policy and Event envelope |
| AD-002 | Dia App | No STL in public APIs | Compliant (reinforces PD-004) |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::ApplicationFlow::` |
| AD-004 | Dia App | PU/Phase/Module | Compliant |
| AD-005 | Dia App | Component-based entities | N/A |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — overflow policy and timeout live in manifest |
| SD-002 | DiaAppFlow | Stages replace phases | N/A |
| SD-003 | DiaAppFlow | ModuleRef<T> sole module access pattern | Compliant |
| SD-004 | DiaAppFlow | TransitionTo is app-wide | N/A |
| SD-005 | DiaAppFlow | Transitions execute next frame | N/A |
| SD-006 | DiaAppFlow | Streams framework-owned, declared in config | Compliant — extends config declaration |
| SD-007 | DiaAppFlow | MessageBus removed | Already done in F1 |
| SD-008 | DiaAppFlow | One-liner registration macro | Compliant — feature does not introduce new registration |
| SD-009 | DiaAppFlow | Module failure: assert debug, rollback release, shutdown on boot | Compliant — `kFailLoud` follows the same pattern: assert in debug, recoverable signal (`Send` returns false) in release. Stream overflow on a non-boot stream is a producer-recoverable error, not a module-lifecycle failure. |
| SD-010 | DiaAppFlow | PU startup order = array order | N/A |
| SD-011 | DiaAppFlow | Shutdown is framework-level | N/A |
| SD-012 | DiaAppFlow | Hot reload = re-enter stage | N/A |
| SD-013 | DiaAppFlow | DoStart returns StartResult | N/A |
| SD-014 | DiaAppFlow | Full manifest validation at load | Compliant — new fields validated at load (AC6) |
| SD-015 | DiaAppFlow | No shared modules across PUs | N/A |
| SD-016 | DiaAppFlow | IApplicationInspectable for runtime state | Compatible — `StreamInfo` may later expose policy + sequence in F4; not in F3 scope |
| SD-017 | DiaAppFlow | Clean break, no v1 backward compat | Compliant — no shim. Manifest field is additive with explicit default. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Envelope shape | Does `Event<T>` increase per-event memory significantly? | Per event: `TimeAbsolute` (8 B) + `StringCRC` (~68 B incl. mString) + `uint64` (8 B) = ~84 B overhead per event in the ring. For CluicheTest's 256-deep `InputToSim` ring: 256 × 84 = ~21 KB extra per reader buffer. Acceptable. If `StringCRC`'s 64-byte `mString` becomes a memory concern in F4/F8, optimisation is to store only the `unsigned int` CRC value in the envelope and resolve the name on demand — orthogonal to F3. |
| 2 | Sequence | What if `mNextSequence` overflows `uint64`? | At 1M events/sec, `uint64` lasts ~584,000 years. Not a real concern. Counter is documented as monotonic-until-Application-restart; resets on `Application::Start()`. |
| 3 | Block policy | Could `kBlockWithTimeout` deadlock if writer and reader are on the same PU thread? | Yes — and this is a manifest authoring error. Validator rule: when a stream's `overflow == "block"`, `fromPU` must differ from `toPU`. Single-PU streams cannot use block policy. |
| 4 | Block policy | Is `block_timeout_ms` per-Send or total across multiple full readers? | Per-Send. The condvar wait covers the entire send attempt. If the wait times out and we fall back to drop-oldest, the next Send starts a fresh timeout. |
| 5 | Fail-loud | If `Send` returns false, does the writer module need to retry/recover? | The framework does nothing further — the `false` return is informational. Caller chooses: ignore (caller decided fail-loud was sufficient signal), retry next frame, or escalate (e.g. `Application::RequestShutdown`). The framework intentionally does not auto-shutdown because correctness streams in different modules will have different escalation policies. |
| 6 | Envelope timestamp | Is `TimeAbsolute::GetSystemTime()` monotonic? | Yes — `TimeAbsolute` is documented to be monotonic system time, not wall clock. Replay and ordering can rely on it. Cross-PU clock skew is zero (single process, single std::steady_clock source). |
| 7 | API migration | The `Send` signature change `void → bool` affects every existing call site. How is migration sequenced? | Migration in this feature's plan: (a) update `EventStreamWriter::Send` to return `bool`; (b) update CluicheTest writer call sites (`KernelModule.cpp`, `DummyLevelModule.cpp`) to ignore the return for drop-oldest streams (no behaviour change for them); (c) update `Consume` call sites for `.payload`; (d) tests. All in one feature plan, no cross-feature shim. |
| 8 | Default policy | Why is `drop-oldest` the default? | Preserves current behaviour for every existing stream that omits `overflow` from the manifest. Keeps F3 a strictly-additive change for unmodified manifests. F1's CluicheTest migration explicitly sets `drop-oldest` so behaviour is documented, not implicit. |
| 9 | Block on shutdown | If a writer is `kBlockWithTimeout`-blocked when shutdown begins, what happens? | `Application::RequestShutdown` sets a `mShuttingDown` atomic. Stores monitor it via the condvar predicate; on shutdown the predicate becomes true and `wait_for` returns immediately. Send falls through to `drop-oldest` fallback as if timed out. No deadlock on shutdown. |
| 10 | Sender for shared writers | If `multi_writer: true`, multiple modules write to one stream — does sender always identify the correct module? | Yes. Each `EventStreamWriter<T>` carries its own `Module* mOwner`. Different modules have different writer handles, even on the same stream. `sender = mOwner->GetInstanceId()` is per-handle. |
| 11 | Capacity | If overflow is `kFailLoud` and capacity is misconfigured (too small), every burst fails. Is that a config error or runtime error? | Runtime error caught at the producer. The producer's caller can log and possibly resize via the manifest in the next build. Validator does not predict bursts; it only validates that policy is one of the four enums. Capacity tuning is a runtime observability problem (covered by F4's tap). |
| 12 | FrameStream warning | The validator warns when overflow/block_timeout is set on FrameStream entries. Is this a hard warning (counted in test) or just log? | Hard warning — the validator's existing warning channel produces a structured log entry that tests can assert on. Editor tooling renders it as a yellow indicator. |
| 13 | Ordering across readers | Per-reader fan-out copies the envelope. Are sequence numbers shared across readers? | Yes — sequence is assigned once on `Send`, then copied. Reader A and Reader B both see envelope with sequence=42 for the same event. This makes cross-reader correlation possible (telemetry vs. game logic both saw event 42). |
| 14 | Future cross-stream ordering | F3 gives per-stream sequence. Is a global sequence (for total ordering) ever needed? | Not in F3's scope. If F8 (replay) needs cross-stream order, a recorder can compose per-stream sequences with timestamps to derive a partial order. Adding a global atomic now buys nothing concrete and adds contention. |
| 15 | Thread safety of Connect | The block-policy condvar is created in the store ctor before any thread accesses it. Is initialization safe? | Yes — store ctor runs on the main thread during `Application::Start()` (per F1's manifest-authoritative creation). All dedicated PU threads launch after store creation. The condvar is fully constructed before any concurrent Send/Consume can occur. |

## Open Questions

None — all interview answers locked, all AI review questions answered.

## Implementation Notes (post-hoc divergence record)

**Sender field (AI Q1 resolution):** `Event<T>::sender` is stored as `unsigned int senderCrc` (the raw 32-bit CRC value), not a `StringCRC` instance. This reduces envelope overhead from ~84 B to ~24 B per event. `StringCRC` has no constructor from a raw `uint32`, so `GetSenderCrc()` returns the raw `unsigned int` — name reconstruction via reverse StreamTypeRegistry lookup is available via `StreamTypeRegistry::GetTypeId<T>()` if needed by debug consumers. AC1 and the Binding Decisions Compliance table row for PD-001 are slightly incorrect as written; the actual field is the CRC value, not the `StringCRC` instance.

**Send return type:** `EventStreamWriter<T>::Send` returns `SendResult` (enum with `kDelivered`, `kDroppedOldest`, `kDroppedNewest`, `kBlockedThenDropped`, `kFailLoudRejected`), not `bool`. AC5 and the Public API section are slightly incorrect; `IsSuccess(result)` maps to the old `bool` semantics. Callers that don't need to distinguish can use `(void)mWriter.Send(...)`.

## Status

`Done` — 2026-05-18. All F3 tasks complete; build and ApplicationFlow* tests passing.
