# Feature Spec: DiaTrace Spans

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **trace-spans** |

**Status:** `Draft`

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #2 (foundation) — `SessionManager` wires `TraceFileSink` at `Start`. Feature #3 (config) — `sinks.trace_file` added to `ObservationConfig`. Implementation is serial.

---

## Problem Statement

After Features #1–#3, the engine can log structured records and write them to a session file, but has no way to measure the duration of operations, track call hierarchies, or correlate work across threads. This feature adds RAII span instrumentation — `DIA_TRACE_ZONE` / `DIA_TRACE_ZONE_NAMED` — that emits OTel-compatible span records to `trace.jsonl` in the session directory.

---

## Solution Overview

`Tracer` is a singleton (mirroring `Logger`) with a dedicated drain thread. `ScopedZone` (RAII) is the producer: on construction it records `start_unix_nano`, generates `span_id` + `trace_id`, and pushes onto the calling thread's open-span stack. On destruction it records `end_unix_nano`, pops the stack, and writes a closed `SpanRecord` into a per-thread ring. The drain thread sweeps all registered rings and writes `trace.jsonl` via `TraceFileSink`.

**`trace_id` semantics (OTel model):** Each root span (no parent on the open-span stack) generates a fresh random `trace_id`. Child spans inherit the current `trace_id` from the thread-local stack. This groups one logical operation tree (a frame, an asset load, a scenario step) under a single `trace_id`.

**`span_id` / `trace_id` generation:** Random `uint64_t` via per-thread `mt19937` seeded at thread registration. ~10 ns per generation, well within the <200 ns span budget (system spec AI-Q16).

`SessionManager::Start` constructs + registers `TraceFileSink` with `Tracer`. `SessionManager::Stop` unregisters it and joins the drain thread. `ObservationConfig` gains `sinks.trace_file` (default `true`) as an amendment to Feature #3's config block.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `DIA_TRACE_ZONE("Name")` compiles and runs without crash; a closed span record appears in `trace.jsonl` | Unit test |
| AC2 | `DIA_TRACE_ZONE_NAMED(var, "Name")` compiles; `var` is accessible as a `ScopedZone&` within its scope | Unit test |
| AC3 | `DIA_TRACE_ZONE` with no name argument uses `std::source_location::current().function_name()` as the span name | Unit test: assert `name` field in record matches calling function name |
| AC4 | `trace.jsonl` records contain all required fields: `schema_version`, `record_type:"span"`, `session_id`, `trace_id`, `span_id`, `parent_span_id`, `name`, `start_unix_nano`, `end_unix_nano`, `thread_id`, `scenario_step` | Unit test: parse record, assert all fields present and correctly typed |
| AC5 | `start_unix_nano` < `end_unix_nano` for a completed span | Unit test |
| AC6 | Root span (no enclosing zone) has `parent_span_id: "0000000000000000"` | Unit test |
| AC7 | Child span (opened inside a root span) has `parent_span_id` equal to the root span's `span_id` | Unit test: nest two zones, assert parent linkage |
| AC8 | Two root spans opened sequentially on the same thread have different `trace_id` values | Unit test |
| AC9 | Child span inherits `trace_id` from its parent | Unit test: nest two zones, assert same `trace_id` |
| AC10 | Two spans opened concurrently on different threads have different `thread_id` values | Unit test: two threads each open a zone, assert distinct `thread_id` |
| AC11 | `span_id` values are unique across all spans in a session (no collision in a 10 000-span test run) | Unit test |
| AC12 | `MaxOpenSpans` (default 64) triggers a `DIA_ASSERT` in Debug when exceeded | Unit test: open 65 nested zones in Debug build, assert fires |
| AC13 | In Release, exceeding `MaxOpenSpans` force-closes the oldest open span with a Warning log entry (no crash) | Unit test: open 65 nested zones in Release build, assert no crash + warning logged |
| AC14 | `TraceFileSink` is registered on `SessionManager::Start` when `sinks.trace_file: true` (default); not registered when `false` | Unit test for both cases |
| AC15 | `trace.jsonl` uses LF (`\n`) line endings, not CRLF | Assert on written bytes |
| AC16 | `start_unix_nano` / `end_unix_nano` use the same epoch-offset approach as `log.jsonl` (steady_clock + session epoch offset) | Assert values are plausible UNIX nanosecond timestamps |
| AC17 | `scenario_step` in span record reflects the active step on the calling thread at span-open time | Unit test: push step, open zone, close zone, assert record carries step |
| AC18 | Thread that calls `DIA_TRACE_ZONE` without registering with `Tracer` silently drops the span (no crash) | Unit test |
| AC19 | `Tracer::Stop()` drains all remaining closed spans before the drain thread exits | Unit test: open + close span, call `Tracer::Stop()`, assert record appears in `trace.jsonl` |
| AC20 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run verification |

---

## Public API

### `Span` / `SpanRecord` (lives in `Dia/DiaObservation/Trace/`)

```cpp
namespace Dia::Observation::Trace {

struct SpanRecord {
    uint64_t             traceId;
    uint64_t             spanId;
    uint64_t             parentSpanId;    // 0 if root span
    Dia::Core::StringCRC name;
    uint64_t             startSteadyNs;   // steady_clock at open (drain converts to unix)
    uint64_t             endSteadyNs;     // steady_clock at close
    uint32_t             threadId;
    Dia::Core::StringCRC scenarioStep;    // captured at span-open time
};

} // namespace Dia::Observation::Trace
```

### `Tracer` singleton (lives in `Dia/DiaObservation/Trace/`)

```cpp
namespace Dia::Observation::Trace {

class Tracer {
public:
    static Tracer& Instance();

    // Lifecycle — called by SessionManager
    bool Start(const char* traceFilePath, int64_t epochOffsetNs);
    void Stop();

    // Thread registration — call once per thread before first DIA_TRACE_ZONE
    void RegisterThreadSpanBuffer();
    void UnregisterThreadSpanBuffer();

    // Internal — called by ScopedZone only
    void OnSpanOpen(SpanRecord& record);   // fills traceId, spanId, parentSpanId, scenarioStep
    void OnSpanClose(const SpanRecord& record); // writes to thread-local closed ring
};

} // namespace Dia::Observation::Trace
```

### `ScopedZone` (lives in `Dia/DiaObservation/Trace/`)

```cpp
namespace Dia::Observation::Trace {

class ScopedZone {
public:
    explicit ScopedZone(const Dia::Core::StringCRC& name,
                        const std::source_location& loc =
                            std::source_location::current());
    ~ScopedZone();
    ScopedZone(const ScopedZone&) = delete;
    ScopedZone& operator=(const ScopedZone&) = delete;

private:
    SpanRecord mRecord;
};

} // namespace Dia::Observation::Trace
```

### Macros

```cpp
// Auto-name from source_location when no name given
#define DIA_TRACE_ZONE(name) \
    ::Dia::Observation::Trace::ScopedZone _dia_zone_##__LINE__(::Dia::Core::StringCRC(name))

#define DIA_TRACE_ZONE_NAMED(var, name) \
    ::Dia::Observation::Trace::ScopedZone var(::Dia::Core::StringCRC(name))

// Zero-overhead no-name variant — uses source_location function name
#define DIA_TRACE_ZONE_AUTO() \
    ::Dia::Observation::Trace::ScopedZone _dia_zone_##__LINE__({})
// (empty StringCRC triggers source_location path in ScopedZone constructor)
```

### `TraceFileSink` (lives in `Dia/DiaObservation/Trace/`)

```cpp
namespace Dia::Observation::Trace {

// Receives closed SpanRecords from drain thread; writes trace.jsonl
class TraceFileSink {
public:
    explicit TraceFileSink(const char* path, const char* sessionId, int64_t epochOffsetNs);
    ~TraceFileSink();

    void OnSpan(const SpanRecord& record);

private:
    FILE*   mFile         = nullptr;
    char    mSessionId[32] = {};
    int64_t mEpochOffsetNs = 0;
};

} // namespace Dia::Observation::Trace
```

### Config amendment — `ObservationConfig` (amends Feature #3)

```cpp
// Add to ObservationConfig in Dia/DiaObservation/Config/ObservationConfig.h:
bool enableTraceFileSink = true;
```

```json
// Add to .diagame observation.sinks block:
"sinks": {
  "stdout":           true,
  "debug_output":     true,
  "observation_file": true,
  "trace_file":       true
}
```

---

## Wire Format — `trace.jsonl` record

```json
{
  "schema_version":  "1.0",
  "record_type":     "span",
  "session_id":      "20260517-143022-a3f2c1b9",
  "trace_id":        "a3f2c1b9e4d50f12",
  "span_id":         "7c3a1b2d9e4f0561",
  "parent_span_id":  "0000000000000000",
  "name":            "RenderFrame",
  "start_unix_nano": 1747484400100000000,
  "end_unix_nano":   1747484400133000000,
  "thread_id":       12345,
  "scenario_step":   "smoke"
}
```

- `trace_id` / `span_id` / `parent_span_id`: 16-char lowercase hex strings (zero-padded `uint64_t`). Root span `parent_span_id` is `"0000000000000000"`.
- `scenario_step`: `""` when no step is active.
- LF (`\n`) line endings (matches `log.jsonl` convention, SD-O14 context).

---

## Thread-Local Design

Each registered thread maintains:

```
thread_local mt19937_64         tRng;              // seeded at RegisterThreadSpanBuffer
thread_local uint64_t           tCurrentTraceId;   // 0 when no root span is open
thread_local SpanRecord         tOpenStack[64];    // LIFO stack of open spans (MaxOpenSpans=64)
thread_local unsigned int       tOpenCount;
thread_local SpanRecord         tClosedRing[256];  // circular buffer of closed spans
thread_local unsigned int       tClosedHead, tClosedTail;
```

**Span open path (`ScopedZone` constructor):**
1. `startSteadyNs = steady_clock::now()`
2. `spanId = tRng()`
3. If `tOpenCount == 0` → root span: `traceId = tRng()`, `tCurrentTraceId = traceId`, `parentSpanId = 0`
4. Else → child: `traceId = tCurrentTraceId`, `parentSpanId = tOpenStack[tOpenCount-1].spanId`
5. `scenarioStep = SessionManager::GetCurrentScenarioStep()` (thread-local, no lock)
6. `MaxOpenSpans` check: Debug → `DIA_ASSERT(tOpenCount < 64)`; Release → force-close oldest + DIA_LOG_WARNING
7. Push onto `tOpenStack`

**Span close path (`ScopedZone` destructor):**
1. `endSteadyNs = steady_clock::now()`
2. Pop from `tOpenStack`; if `tOpenCount == 0` → `tCurrentTraceId = 0`
3. Write `SpanRecord` into `tClosedRing` (drop-oldest on overflow)
4. Signal drain thread (atomic wake hint)

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaObservation/Trace/SpanRecord.h` | New |
| `Dia/DiaObservation/Trace/Tracer.h/.cpp` | New |
| `Dia/DiaObservation/Trace/ScopedZone.h/.cpp` | New |
| `Dia/DiaObservation/Trace/TraceFileSink.h/.cpp` | New |
| `Dia/DiaObservation/Trace/DiaTrace.h` | New — macro definitions |
| `Dia/DiaObservation/Config/ObservationConfig.h` | Add `enableTraceFileSink` field |
| `Dia/DiaObservation/Config/ObservationConfigLoader.cpp` | Parse `sinks.trace_file` |
| `Dia/DiaObservation/Session/SessionManager.h/.cpp` | `Start` constructs + registers `TraceFileSink`; `Stop` unregisters + joins drain |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add new files |
| `Dia/DiaObservation/DiaObservation.vcxproj.filters` | Add new files |
| `Dia/DiaObservation/Testing/TraceFixture.h` | New test utility |
| CluicheTest `.diagame` | Add `sinks.trace_file: true` to `observation.sinks` block |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Span `name` and `scenarioStep` are `StringCRC`. `trace_id`/`span_id` are `uint64_t` opaque identifiers (not engine entity IDs — not subject to PD-001). |
| PD-002 | ProcessingUnit/Phase/Module architecture | `Tracer` is callable from any thread at any time (same constraint as `Logger`). No `IModule` subclass in this feature. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | `ScopedZone`, `Tracer`, `TraceFileSink` public APIs use only `StringCRC`, `uint64_t`, `const char*`, `bool`. Thread-local arrays are fixed-size. Internal use of `<random>`, `<chrono>`, `<atomic>`, `<thread>` permitted. |
| PD-005 | x64 only | `thread_local`, `std::mt19937_64`, `steady_clock` all x64-native. |
| PD-006 | VS project files source of truth | `DiaObservation.vcxproj` updated manually. |
| PD-007 | C++20 required | `std::source_location` used for auto-name fallback (SD-O18 context). |
| PD-008 | Directory.Build.props owns build settings | No overrides added. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | `trace.jsonl` written to session directory under `Cluiche/out/<App>/sessions/<id>/`. |
| PD-010 | `.diagame` is project root | `sinks.trace_file` slots into `config.observation.sinks` block, forward-compatible with Feature #3's shape. |
| AD-001 | Module system with YAML frontmatter | `dia.dia.observation.architecture.module.md` updated to declare `Trace/` subsystem. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new code in `Dia::Observation::Trace::`. |
| SD-O01 | One module for all four pillars | `Trace/` is a subdirectory of `Dia/DiaObservation/`. |
| SD-O05 | `schema_version: "1.0"` on every record | `TraceFileSink::OnSpan` emits `"schema_version":"1.0"` on every line. |
| SD-O06 | OTel wire-format, no SDK | Field names match OTel span fields (`trace_id`, `span_id`, `parent_span_id`, `start_unix_nano`, `end_unix_nano`). No `opentelemetry-cpp` dependency. |
| SD-O14 | Monotonic `uint64_t timestampNs` at producer side | `startSteadyNs` captured in `ScopedZone` constructor (producer side); drain applies epoch offset for wire format. |
| SD-O16 | Session ID on every record | `TraceFileSink` writes `session_id` from the value received at construction. |
| SD-O18 | `MaxOpenSpans` dev-time guard | Debug: `DIA_ASSERT(tOpenCount < 64)`. Release: force-close oldest + `DIA_LOG_WARNING`. |
| SD-O20 | Test utilities in `Dia/DiaObservation/Testing/` | `TraceFixture.h` placed there. |
| SD-O21 | DiaCore is only required dependency; callable from anywhere | `Tracer` singleton pattern mirrors `Logger`. No `DiaApplicationFlow` dependency. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `ObservationConfig` amendment — Feature #3 is already Approved. Does adding `enableTraceFileSink` to the struct constitute a breaking change to Feature #3's contract? | No — it is an additive field with a default value. Feature #3's ACs are unaffected. The amendment is noted in the Files Touched table. |
| OQ2 | Should `DIA_TRACE_ZONE` be a no-op in Release builds (like `DIA_LOG_TRACE`/`DIA_LOG_DEBUG`)? | No. Spans are useful in Release for profiling and E2E timing. The per-span budget is <200 ns (system spec AI-Q16) — acceptable for Release. Channel-level filtering (if needed) is a v2 config addition. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | What happens if a thread calls `DIA_TRACE_ZONE` without calling `RegisterThreadSpanBuffer`? | `ScopedZone` constructor checks `tOpenCount` — uninitialised `thread_local` is zero-initialised in C++. The span is silently dropped (no ring is registered, no drain reference exists). Same contract as unregistered Logger threads (system spec AI-Q3). Documented as a programmer error. |
| 2 | Threading | How does the drain thread discover all registered thread rings? | Same pattern as `Logger`: threads call `Tracer::RegisterThreadSpanBuffer()` which adds a pointer to the thread's `tClosedRing` + `tClosedHead`/`tClosedTail` into a `Tracer`-owned list (protected by a `std::mutex`). Drain thread holds the mutex briefly to snapshot the list, then reads each ring lock-free. |
| 3 | Threading | `UnregisterThreadSpanBuffer` — what if the drain thread is mid-sweep of a ring that is being unregistered? | Unregister acquires the same mutex as Register. The drain thread takes a snapshot of the list before each sweep pass; if a ring is unregistered mid-sweep, the snapshot is stale for one pass at most, and the drain thread checks a `valid` flag on each ring entry before reading. |
| 4 | Timestamps | Same epoch-offset issue as `log.jsonl` — `startSteadyNs`/`endSteadyNs` are steady_clock, wire format wants unix_nano. | `TraceFileSink` receives `epochOffsetNs` at construction (same value `SessionManager` computed in Feature #2). Emits `start_unix_nano = startSteadyNs + epochOffsetNs`, same approach as `ObservationFileSink`. |
| 5 | Crash | Open spans at crash — system spec AI-Q12 says they appear in `crashes/<n>.json` under `open_spans_at_crash`. Who writes this? | `SessionManager::EmergencyDump` (Feature #2) iterates `Tracer::Instance().GetOpenSpansSnapshot()` (new method, returns a read of all thread-local open stacks via the registered ring list). Writes them into `crashes/<n>.json` with `start_unix_nano` and `duration_ms_so_far = (steady_clock::now() - startSteadyNs) / 1e6`. They are NOT written to `trace.jsonl` (AC12 / system spec AI-Q12). |
| 6 | MaxOpenSpans | Force-close in Release: which span is force-closed — the oldest open or the new one being opened? | The **oldest open** (bottom of `tOpenStack`). This preserves the most recent context (the innermost spans are more likely to be relevant to the current code path). The force-closed span gets `endSteadyNs = startSteadyNs` (zero duration) to signal it was abnormally closed. |
| 7 | Performance | `mt19937_64` for ID generation — is it seeded per-thread at registration or once globally? | Per-thread at `RegisterThreadSpanBuffer`, seeded with `steady_clock::now().count() ^ thread_id ^ Tracer::Instance().GetSessionSeed()` where `GetSessionSeed()` is a random value generated once at `Tracer::Start`. Ensures IDs are unique across threads and across sessions. |
| 8 | Wire format | `trace_id` and `span_id` as 16-char hex strings vs raw uint64_t integers — why strings? | OTel trace context uses opaque hex strings. Tools consuming `trace.jsonl` (e.g. Jaeger, custom AI analysis) expect them as strings. `printf("%016llx", id)` is trivial; no performance concern on the drain thread. |
| 9 | Config | Feature #3 is already Approved. Is amending `ObservationConfig` and `ObservationConfigLoader` in this feature clean? | Yes — additive field, default value, backward-compatible. The amendment is confined to two files already in Feature #3's file list. No Feature #3 ACs are invalidated. |
| 10 | Lifecycle | Should `SessionModule::DoStart` also call `Tracer::Instance().RegisterThreadSpanBuffer()` for the main thread? | Yes — same pattern as `Logger::RegisterThreadBuffer()` which `SessionModule` (or `LoggerModule` historically) calls for the main thread. `SessionModule::DoStart` registers both Logger and Tracer for the main PU thread. Other threads register themselves. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `SpanRecord` struct | AC4 | Planned | haiku | Plain header |
| 2 | Thread-local span infrastructure — open stack, closed ring, `mt19937_64` seed | AC6, AC7, AC8, AC9, AC11, AC12, AC13 | Planned | sonnet | Core of feature; see Thread-Local Design section |
| 3 | `Tracer` singleton — `Start`/`Stop`, drain thread, `RegisterThreadSpanBuffer`/`UnregisterThreadSpanBuffer`, ring list management | AC14, AC18, AC19 | Planned | sonnet | Mirror Logger pattern |
| 4 | `ScopedZone` — constructor/destructor; `source_location` auto-name path | AC1, AC2, AC3, AC5, AC7, AC17 | Planned | sonnet | |
| 5 | `DiaTrace.h` macro definitions | AC1, AC2, AC3 | Planned | haiku | |
| 6 | `TraceFileSink` — `OnSpan` writes full JSON-line record per wire format section | AC4, AC15, AC16 | Planned | sonnet | epoch offset per AI-Q4 |
| 7 | `MaxOpenSpans` guard — Debug assert + Release force-close | AC12, AC13 | Planned | sonnet | Conditional compile |
| 8 | `SessionManager` updated — `Start` constructs + registers `TraceFileSink`; `Stop` unregisters + joins; `EmergencyDump` calls `GetOpenSpansSnapshot` | AC14, AC19 | Planned | sonnet | Amends Feature #2 files |
| 9 | `ObservationConfig` + `ObservationConfigLoader` amended — `enableTraceFileSink` + `sinks.trace_file` parsing | AC14 | Planned | haiku | Amends Feature #3 files |
| 10 | `SessionModule::DoStart` registers main thread with `Tracer` | AC20 | Planned | haiku | Per AI-Q10 |
| 11 | Test utility `TraceFixture.h` in `Testing/` | Supporting ACs | Planned | haiku | |
| 12 | GoogleTests — all AC1–AC19 | All ACs | Planned | sonnet | Death test not needed here |
| 13 | Update `DiaObservation.vcxproj` + `.vcxproj.filters` | AC20 | Planned | haiku | |
| 14 | Update `dia.dia.observation.architecture.module.md` — add `Trace/` subsystem | — | Planned | haiku | |

---

## Status

`Approved` — 2026-05-17. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
