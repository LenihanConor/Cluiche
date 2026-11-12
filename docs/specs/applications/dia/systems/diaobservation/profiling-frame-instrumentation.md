# Feature Spec: DiaProfiling — Frame Instrumentation

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diaobservation/diaobservation.md | **profiling-frame-instrumentation** |

**Status:** `Approved` — 2026-05-19

> **Amended 2026-11-09 (Approved):** module-attributed scopes, always-on unit timing, trip reports and `--perf`. See "Amendment 2026-11-09" at the end, and @perf-inspector.md / @perf-live-control.md.
>
> **Amended 2026-11-12 (Approved):** worker-thread profile records and the offline Perfetto converter (`dia perf convert`). See "Amendment 2026-11-12" at the end, and @docs/specs/applications/dia/systems/diathreading/job-observation.md.

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #2 (foundation) — `SessionManager` wires `ProfileFileSink` and drives `Profiler`; session directory must exist. Feature #5 (metrics registry) — `DIA_PROFILE_SCOPE_METRIC` variant feeds `Histogram::Observe()`. Both must be Done before implementation begins.

---

## Problem Statement

After Features #1–#7, the engine has logs, traces, metrics, health, and a DebugServer bridge — but no way to understand per-frame call-tree cost. Traces model causality across multiple frames; profiling is different: it records hierarchical scope cost within a frame, keyed by frame number. This infrastructure feature delivers `DIA_PROFILE_SCOPE`, the `Profiler` singleton, `ProfilerModule` host, `profile.jsonl` flush-per-frame, and `profile-final.json` (last 60 frames). It is the foundation that Feature #9 (domain instrumentation sites) sits on.

## Solution Overview

`Profiler` is a singleton (mirroring `Tracer`). `ScopedZone` (RAII) is the producer: on construction it records `startNs` via `steady_clock::now()`, pushes onto the calling thread's open-scope stack, and captures `parentScopeId` from the stack top. On destruction it records `durationNs`, pops the stack, and writes a `ScopeRecord` into the per-thread closed-record ring.

`ProfilerModule::BeginFrame()` increments the global frame counter atomically and signals the drain thread to flush all closed records for the previous frame to `profile.jsonl`. `EndFrame()` is a hook reserved for v2 GPU boundary integration.

**Category filtering:** `Profiler::ActiveMask()` returns a `ProfileCategory` bitmask initialised from `observation.profile.categories` config at start. Every `DIA_PROFILE_SCOPE(name, category)` call performs a single AND before allocating any scope. When profiling is off (`enabled = false`), `ActiveMask()` is `0` — every macro is ~1 ns, no allocation, no file output.

**Metric bridge:** `DIA_PROFILE_SCOPE_METRIC(name, category, histogram)` feeds the scope duration into `histogram->Observe(durationNs)` on destructor regardless of whether profiling is enabled. When profiling is disabled (`mActive = false`), only the metric is populated — no silent data loss (SD-O30).

`SessionManager::Stop` calls `Profiler::Stop()` which drains remaining records and writes `profile-final.json` with the last 60 complete frames from an in-memory frame ring.

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `DIA_PROFILE_SCOPE("Name", kCategory)` compiles and runs without crash; a closed scope record appears in `profile.jsonl` | Unit test |
| AC2 | `DIA_PROFILE_SCOPE_NAMED(var, "Name", kCategory)` compiles; `var` is accessible as a `ScopedZone&` within its scope | Unit test |
| AC3 | `DIA_PROFILE_SCOPE_METRIC("Name", kCategory, histogram)` — destructor calls `histogram->Observe(durationNs)` | Unit test: assert histogram observation count incremented |
| AC4 | When `Profiler::ActiveMask()` is `0` (profiling off), `DIA_PROFILE_SCOPE` is a no-op: no record written, no scope stack touched | Unit test: set mask = 0, assert no records in `profile.jsonl` |
| AC5 | `profile.jsonl` records contain all required fields: `schema_version:"1.0"`, `record_type:"profile_scope"`, `session_id`, `frame_number`, `thread_id`, `scope_name`, `category`, `scope_id`, `parent_scope_id`, `start_unix_nano`, `duration_ns` | Unit test: parse record, assert all fields present |
| AC6 | Root scope (no enclosing profile scope on the thread) has `parent_scope_id: 0` | Unit test |
| AC7 | Child scope (opened inside a root scope) has `parent_scope_id` equal to the root's `scope_id` | Unit test: nest two scopes, assert parent linkage |
| AC8 | `ProfilerModule::BeginFrame()` increments the frame counter; `Profiler::GetCurrentFrame()` returns 2 after two calls | Unit test |
| AC9 | Records flushed to `profile.jsonl` carry the `frame_number` of the frame they were opened in | Unit test: open scope in frame N, call BeginFrame, assert record `frame_number == N` |
| AC10 | Max scope depth 64: in Debug, opening a 65th nested scope triggers `DIA_ASSERT` | Unit test |
| AC11 | Max scope depth 64: in Release, opening a 65th scope silently truncates — no crash, no further records for that scope | Unit test |
| AC12 | Thread without `RegisterThreadScopeBuffer` call silently drops scope records; no crash | Unit test |
| AC13 | `profile-final.json` is written on `SessionManager::Stop`; contains up to 60 frames grouped by `frame_number` | Unit test: run 70 frames, stop, parse file, assert ≤60 frames captured |
| AC14 | `profile.jsonl` uses LF (`\n`) line endings, not CRLF | Assert on written bytes |
| AC15 | `DIA_PROFILE_SCOPE_METRIC` with `histogram == nullptr` behaves identically to `DIA_PROFILE_SCOPE` (no crash) | Unit test |
| AC16 | `ProfileCategory::kAll` mask enables all built-in categories | Unit test: set mask = kAll, open scope per category, assert all appear in records |
| AC17 | Config `observation.profile.categories.diagraphics = true` sets `ActiveMask()` to `kDiaGraphics` only | Unit test: parse config, assert mask |
| AC18 | CLI `--profile-categories=diagraphics` sets `ActiveMask()` to `kDiaGraphics` | Unit test |
| AC19 | `Profiler::Stop()` drains all remaining closed scope records before file close | Unit test: open + close scope, call `Profiler::Stop()`, assert record in `profile.jsonl` |
| AC20 | `dia pipeline --target cluichetest` green in Debug + Release | Build + run verification |

## Public API

```cpp
// Dia/DiaObservation/Profile/ProfileCategory.h
namespace Dia::Observation::Profile {
    using ProfileCategory = uint32_t;
    namespace Category {
        constexpr ProfileCategory kNone               = 0;
        constexpr ProfileCategory kDiaApplicationFlow = 1 << 0;
        constexpr ProfileCategory kDiaGraphics        = 1 << 1;
        constexpr ProfileCategory kDiaStream          = 1 << 2;
        constexpr ProfileCategory kDiaAssetRuntime    = 1 << 3;
        constexpr ProfileCategory kDiaAnimation       = 1 << 4;
        constexpr ProfileCategory kAll                = ~0u;
    }
}

// Dia/DiaObservation/Profile/ScopeRecord.h
namespace Dia::Observation::Profile {
    struct ScopeRecord {
        Dia::Core::StringCRC name;
        ProfileCategory      category;
        uint64_t             scopeId;
        uint64_t             parentScopeId;  // 0 if root scope
        uint64_t             startUnixNano;
        uint64_t             durationNs;
        uint32_t             frameNumber;
        uint32_t             threadId;
    };
}

// Dia/DiaObservation/Profile/Profiler.h
namespace Dia::Observation::Profile {
    class Profiler {
    public:
        static Profiler& Instance();

        bool Start(const char* profileFilePath, ProfileCategory activeMask,
                   const char* sessionId, int64_t epochOffsetNs);
        void Stop();

        void BeginFrame();   // increments frame counter; triggers drain of previous frame records
        void EndFrame();     // reserved for v2 GPU fence integration
        uint32_t GetCurrentFrame() const;

        void            SetActiveMask(ProfileCategory mask);
        ProfileCategory ActiveMask() const;

        void RegisterThreadScopeBuffer();
        void UnregisterThreadScopeBuffer();

        // Called by ScopedZone only
        uint64_t OnScopeOpen (const Dia::Core::StringCRC& name, ProfileCategory category,
                               uint32_t& outFrameNumber, uint64_t& outParentScopeId);
        void     OnScopeClose(uint64_t scopeId, const Dia::Core::StringCRC& name,
                               ProfileCategory category, uint64_t parentScopeId,
                               uint64_t startNs, uint64_t durationNs,
                               uint32_t frameNumber, uint32_t threadId);
    };
}

// Dia/DiaObservation/Profile/ScopedZone.h
namespace Dia::Observation::Profile {
    class ScopedZone {
    public:
        ScopedZone(const Dia::Core::StringCRC& name, ProfileCategory category,
                   Dia::Observation::Metric::Histogram* histogram = nullptr);
        ~ScopedZone();
        ScopedZone(const ScopedZone&) = delete;
        ScopedZone& operator=(const ScopedZone&) = delete;
    private:
        uint64_t    mScopeId;
        uint64_t    mParentScopeId;
        uint64_t    mStartNs;
        uint32_t    mFrameNumber;
        Dia::Core::StringCRC mName;
        ProfileCategory      mCategory;
        Dia::Observation::Metric::Histogram* mHistogram;
        bool        mActive;
    };
}

// Dia/DiaObservation/Profile/DiaProfile.h — macros
#define DIA_PROFILE_SCOPE(name, category) \
    ::Dia::Observation::Profile::ScopedZone _dia_profile_##__LINE__( \
        ::Dia::Core::StringCRC(name), (category))

#define DIA_PROFILE_SCOPE_NAMED(var, name, category) \
    ::Dia::Observation::Profile::ScopedZone var(::Dia::Core::StringCRC(name), (category))

#define DIA_PROFILE_SCOPE_METRIC(name, category, histogram) \
    ::Dia::Observation::Profile::ScopedZone _dia_profile_##__LINE__( \
        ::Dia::Core::StringCRC(name), (category), (histogram))

// Config amendment (amends Feature #3)
// observation.profile.enabled = false (default off)
// observation.profile.categories.diagraphics = true (per-category enable)
// CLI: --profile-categories=diagraphics,diastream
```

## Wire Format

`profile.jsonl` — one JSON line per closed scope, flushed per-frame at `BeginFrame()`:

```json
{"schema_version":"1.0","record_type":"profile_scope","session_id":"20260519-143022-a3f2c1b9","frame_number":142,"thread_id":1,"scope_name":"module.tick.RenderModule","category":2,"scope_id":3302,"parent_scope_id":7,"start_unix_nano":1747484400100000000,"duration_ns":4200}
```

`profile-final.json` — last ≤60 frames grouped by frame number, written at `SessionManager::Stop`:

```json
{
  "schema_version": "1.0",
  "session_id": "20260519-143022-a3f2c1b9",
  "frames_captured": 60,
  "frames": [
    {
      "frame_number": 710,
      "scopes": [
        {"scope_name":"pu.update","thread_id":1,"scope_id":6001,"parent_scope_id":0,"start_unix_nano":1747484400100000000,"duration_ns":14200000},
        {"scope_name":"module.tick.RenderModule","thread_id":1,"scope_id":6002,"parent_scope_id":6001,"start_unix_nano":1747484400103000000,"duration_ns":4200000}
      ]
    }
  ]
}
```

LF (`\n`) line endings on `profile.jsonl` to match `log.jsonl` / `trace.jsonl` convention.

## Thread-Local Design

Each registered thread maintains:
- `thread_local uint64_t tScopeCounter` — monotonic scope ID per thread (starts at 1)
- `thread_local uint64_t tOpenStack[64]` — scope IDs of open scopes (LIFO stack)
- `thread_local uint64_t tOpenStartNs[64]` — start times parallel to stack
- `thread_local uint64_t tOpenParentId[64]` — parent IDs parallel to stack
- `thread_local Dia::Core::StringCRC tOpenName[64]`
- `thread_local ProfileCategory tOpenCategory[64]`
- `thread_local unsigned int tOpenCount`
- `thread_local ScopeRecord tClosedRing[512]` — circular buffer of completed scopes
- `thread_local unsigned int tClosedHead, tClosedTail`

**Scope open path (ScopedZone constructor):**
1. Check `Profiler::Instance().ActiveMask() & category` — if zero, set `mActive = false` and return
2. `mStartNs = steady_clock::now().count()`
3. `mScopeId = ++tScopeCounter`
4. `mParentScopeId = (tOpenCount > 0) ? tOpenStack[tOpenCount-1] : 0`
5. `mFrameNumber = Profiler::Instance().GetCurrentFrame()`
6. MaxDepth check: Debug → `DIA_ASSERT(tOpenCount < 64)`; Release → truncate silently (no push, mActive = false for this scope only)
7. Push onto stack; increment `tOpenCount`

**Scope close path (ScopedZone destructor):**
1. If histogram != nullptr: `histogram->Observe(static_cast<double>(durationNs))`  ← always, regardless of mActive
2. If `!mActive`: return
3. `durationNs = steady_clock::now().count() - mStartNs`
4. Pop from stack; decrement `tOpenCount`
5. Write `ScopeRecord` into `tClosedRing` (drop-oldest on overflow)

**Frame flush (ProfilerModule::BeginFrame → Profiler::BeginFrame):**
- Increment `mCurrentFrame` (atomic)
- Drain thread wakes, sweeps all registered rings, writes records with `frame_number < mCurrentFrame` to `profile.jsonl`
- Drain thread also appends to in-memory frame ring (last 60 frames) for `profile-final.json`

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaObservation/Profile/ProfileCategory.h` | New — `ProfileCategory` type + built-in constants |
| `Dia/DiaObservation/Profile/ScopeRecord.h` | New |
| `Dia/DiaObservation/Profile/Profiler.h/.cpp` | New |
| `Dia/DiaObservation/Profile/ScopedZone.h/.cpp` | New |
| `Dia/DiaObservation/Profile/ProfileFileSink.h/.cpp` | New — writes `profile.jsonl`; maintains last-60-frames ring for final dump |
| `Dia/DiaObservation/Profile/DiaProfile.h` | New — macro definitions |
| `Dia/DiaObservation/Config/ObservationConfig.h` | Add `profileEnabled`, `profileCategoryMask` fields |
| `Dia/DiaObservation/Config/ObservationConfigLoader.cpp` | Parse `observation.profile` block + CLI `--profile-categories` |
| `Dia/DiaObservation/Session/SessionManager.h/.cpp` | `Start` initialises `Profiler`; `Stop` calls `Profiler::Stop()` (writes `profile-final.json`); `EmergencyDump` calls `GetOpenScopesSnapshot()` |
| `Cluiche/CluicheGameBaseline/Modules/ProfilerModule.h/.cpp` | New application-side host — `DoUpdate` calls `Profiler::BeginFrame()`/`EndFrame()` |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add Profile/ files |
| `Dia/DiaObservation/DiaObservation.vcxproj.filters` | Add Profile/ files |
| `Dia/DiaObservation/Testing/ProfileFixture.h` | New test utility |
| CluicheTest `.diagame` | Add `observation.profile` block |

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | `scope_name` is `StringCRC`. `scope_id`/`parent_scope_id` are per-thread monotonic `uint64_t` opaque IDs, not engine entity IDs — not subject to PD-001. |
| PD-002 | ProcessingUnit/Phase/Module architecture | `Profiler` callable from any thread at any time. `ProfilerModule` hosts frame-boundary calls in application code (SD-O21, SD-O26). |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | `ScopedZone` and `Profiler` public APIs use only `StringCRC`, `uint64_t`, `ProfileCategory`, `const char*`, `bool`. Thread-local arrays are fixed-size. Internal `<chrono>`, `<atomic>`, `<thread>` permitted. |
| PD-005 | x64 only | `thread_local`, `steady_clock` x64-native. `std::atomic<uint32_t>` lock-free on x64. |
| PD-006 | VS project files source of truth | `DiaObservation.vcxproj` updated manually. |
| PD-007 | C++20 required | No new C++20 features beyond existing baseline. |
| PD-008 | Directory.Build.props owns build settings | No overrides added. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | `profile.jsonl` and `profile-final.json` written to session directory. |
| PD-010 | `.diagame` is project root | `observation.profile` block slots into `.diagame` `config` block. |
| AD-001 | Module system with YAML frontmatter | `dia.dia.observation.architecture.module.md` updated to declare `Profile/` subsystem. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new code in `Dia::Observation::Profile::`. |
| SD-O01 | One module for all five pillars | `Profile/` is a subdirectory of `Dia/DiaObservation/`. Not a separate module. |
| SD-O05 | `schema_version: "1.0"` on every record | `ProfileFileSink` emits `"schema_version":"1.0"` on every line; `profile-final.json` carries it at top level. |
| SD-O16 | Session ID on every record | `ProfileFileSink` writes `session_id` from value passed at construction. |
| SD-O20 | Test utilities in `Testing/` | `ProfileFixture.h` placed in `Dia/DiaObservation/Testing/`. |
| SD-O21 | DiaCore is only required dependency | `Profiler` depends only on `DiaCore`. `ProfilerModule` is application code — not a `DiaObservation` dependency. |
| SD-O24 | Profiling is a 5th pillar, frame-structured | `ScopeRecord` carries `frame_number`; records form a tree per frame via `parent_scope_id`. Distinct from `Tracer` span records which model causality. |
| SD-O25 | `DIA_PROFILE_SCOPE` and `DIA_TRACE_ZONE` both exist; neither replaces the other | Feature introduces `DIA_PROFILE_SCOPE`; `DIA_TRACE_ZONE` from Feature #4 is unchanged and unaffected. |
| SD-O26 | `ProfilerModule::BeginFrame`/`EndFrame` drives frame boundary | `ProfilerModule` (application code) calls these; `Profiler` itself does not depend on DiaApplicationFlow. |
| SD-O27 | Profiling defaults OFF | `observation.profile.enabled = false` by default; `ActiveMask()` returns `0`. |
| SD-O28 | Profiling uses integer bitmask categories | `ProfileCategory = uint32_t`; single AND at each call site. |
| SD-O29 | Category constants as `uint32_t` bitmasks | Built-in constants in `Dia::Observation::Profile::Category`. Application code extends with additional constants. |
| SD-O30 | Profiling and metrics are separate pillars; one-way bridge | `DIA_PROFILE_SCOPE_METRIC` feeds `Histogram::Observe()` on destructor; bridge is one-way (profile duration → metric). |

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | `BeginFrame()` flushes the *previous* frame. What if `EndFrame()` was never called for that frame? | `EndFrame()` is a no-op in v1. There is no "frame open/close" concept — `BeginFrame()` simply marks the boundary. Records with `frame_number < mCurrentFrame` are written regardless of whether `EndFrame()` was called. |
| OQ2 | Ring size for closed scope records — 512 entries. Justification? | A busy frame at 64 scopes × 60fps = ~3840 scopes/sec. At 512 entries, the ring holds ~130ms of head-room for the drain thread before drop-oldest kicks in. Drain runs faster than 60fps in practice. |
| OQ3 | Config amendment: Feature #3 is Approved. Does adding `observation.profile` break it? | No — additive fields with defaults. Same precedent as Feature #4's `observation.sinks.trace_file` amendment. |
| OQ4 | `profile-final.json` groups scopes by frame. Does this require a secondary sort pass? | Drain thread maintains an in-memory frame ring (not `profile.jsonl`). Scopes are appended to the current frame's list in the ring as they arrive. No secondary sort needed — within-frame ordering is drain-arrival order. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | `BeginFrame()` increments `mCurrentFrame` atomically. Can a scope opened before `BeginFrame()` be attributed to the wrong frame? | No — `mFrameNumber` is captured in the `ScopedZone` constructor before the scope is pushed onto the stack. Even if `BeginFrame()` runs on another thread between open and close, the scope carries its original frame number. The drain writes records with `frame_number < mCurrentFrame` — a scope opened in frame N remains in frame N. |
| 2 | Threading | Two threads open scopes simultaneously. Can `parent_scope_id` accidentally cross thread boundaries? | No — the scope stack and parent lookup are `thread_local`. `parent_scope_id` is always the ID of the innermost open scope *on the same thread*. Cross-thread relationships are not representable in v1. |
| 3 | Performance | What is the producer-side cost of `DIA_PROFILE_SCOPE` when category is enabled? | `ActiveMask() & category` (~1ns), `steady_clock::now()` (~30ns on Windows x64), thread-local stack push (~5ns), scope ID increment (~1ns). Total: ~37ns. When disabled: single AND = ~1ns. No allocation, no lock. |
| 4 | Metric bridge | `DIA_PROFILE_SCOPE_METRIC` feeds the histogram even when profiling is disabled. Does `steady_clock::now()` fire twice? | Yes — once at construction (`mStartNs`), once at destruction for `durationNs`. Both fire regardless of `mActive` when `mHistogram != nullptr`. The destructor computes `durationNs = now() - mStartNs` before checking whether to write a scope record. The two `now()` calls are unavoidable for correct duration measurement; total cost is ~60ns at this site. |
| 5 | Drain | Frame flush at `BeginFrame()` — what if the drain thread hasn't finished the previous flush? | Flush signals the drain thread via an atomic flag. If it hasn't finished flushing frame N-1 when frame N+1's BeginFrame fires, the drain processes both batches. Records are never lost — drop-oldest only applies to the thread-local ring, not the drain's work queue. At `Profiler::Stop()`, drain is joined and all remaining records are flushed before file close. |
| 6 | `profile-final.json` | The frame ring holds 60 frames. What if a session runs for exactly 60 frames and then stops? | All 60 frames are written. The ring capacity is 60 complete frames; on the 61st frame's `BeginFrame()`, the oldest frame is dropped. A 60-frame session captures every frame. |
| 7 | Crash | Open scopes at crash — are they included in `crashes/<n>.json`? | `SessionManager::EmergencyDump` (Feature #2) calls `Profiler::Instance().GetOpenScopesSnapshot()` — a new method that reads the per-thread open stacks via the registered buffer list. Open scopes appear in `crashes/<n>.json` under `open_scopes_at_crash` with `start_unix_nano` and `duration_ms_so_far`. Not written to `profile.jsonl` (undefined end time). |
| 8 | Lifecycle | What if `SessionManager::Start()` is never called but `DIA_PROFILE_SCOPE_METRIC` is used with a valid histogram? | `Profiler::ActiveMask()` defaults to `0` (`mActive = false`). Destructor still feeds the histogram (histogram is checked independently of `mActive`). No session directory opened. Correct — metrics work without a session. |
| 9 | `ProfilerModule` | `ProfilerModule` lives in application code. Does this mean all Dia library code (DiaStream, DiaGraphics) can still call `DIA_PROFILE_SCOPE` without depending on `ProfilerModule`? | Yes — `DIA_PROFILE_SCOPE` calls `Profiler::Instance()` directly (singleton). `ProfilerModule` only drives the frame boundary (`BeginFrame`/`EndFrame`). Library code never needs to know `ProfilerModule` exists. |
| 10 | Config | What happens if `observation.profile.categories.diagraphics = true` but `observation.profile.enabled = false`? | `enabled = false` forces `ActiveMask() = 0` regardless of category config. Per-category enables are only applied when `enabled = true`. This is the master switch behaviour, same as for traces (SD-O27). |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ProfileCategory.h` — `ProfileCategory` typedef + built-in constants | AC16, AC17 | Planned | haiku | Plain header |
| 2 | `ScopeRecord.h` struct | AC5 | Planned | haiku | Plain header |
| 3 | Thread-local scope infrastructure — open stack depth 64, closed ring 512, scope ID counter, parent tracking | AC6, AC7, AC10, AC11 | Planned | sonnet | Core of feature; see Thread-Local Design section |
| 4 | `Profiler` singleton — `Start`/`Stop`, drain thread, frame counter, `BeginFrame`/`EndFrame`, `ActiveMask`, thread buffer registration, `GetOpenScopesSnapshot` | AC8, AC9, AC12, AC16, AC17, AC18, AC19 | Planned | sonnet | Mirror Tracer pattern from Feature #4 |
| 5 | `ScopedZone` — constructor/destructor; category check; mActive path; metric bridge | AC1, AC2, AC3, AC4, AC6, AC7, AC15 | Planned | sonnet | |
| 6 | `DiaProfile.h` macro definitions | AC1, AC2, AC3 | Planned | haiku | |
| 7 | `ProfileFileSink` — `profile.jsonl` per-frame flush + last-60-frame ring + `profile-final.json` writer | AC5, AC13, AC14 | Planned | sonnet | Frame-grouped final dump per OQ4 |
| 8 | `ObservationConfig` + `ObservationConfigLoader` amended — `profile.enabled`, `profile.categories`, CLI `--profile-categories` | AC17, AC18 | Planned | haiku | Amends Feature #3 files |
| 9 | `SessionManager` amended — `Start` initialises Profiler; `Stop` calls `Profiler::Stop()`; `EmergencyDump` gets open scope snapshot | AC13, AC19 | Planned | sonnet | Amends Feature #2 files |
| 10 | `ProfilerModule` application host — `DoUpdate` calls `BeginFrame()`/`EndFrame()` | AC8, AC9 | Planned | haiku | Lives in CluicheGameBaseline (SD-O21) |
| 11 | Test utility `ProfileFixture.h` in `Testing/` | Supporting ACs | Planned | haiku | |
| 12 | GoogleTests — AC1–AC19 | All ACs | Planned | sonnet | |
| 13 | Update `DiaObservation.vcxproj` + `.vcxproj.filters` | AC20 | Planned | haiku | |
| 14 | Update `dia.dia.observation.architecture.module.md` — add `Profile/` subsystem | — | Planned | haiku | |

---

## Status

`Approved` — 2026-05-19. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.


---

## Amendment 2026-11-09: module attribution, always-on unit timing, trip reports

**Status of this amendment:** `Approved` — 2026-11-09 (with @perf-inspector.md and @perf-live-control.md). **Revised 2026-11-10** after the plan audit: subject is a 4-byte CRC value (A1), Sim timing per fixed step (A2), trip skips and no I/O on the PU thread (A3); AA10-AA12 added.
**Why:** the S-1 panel perf investigation (docs/specs/applications/cow/systems/s1-worldgeneration/worldgen-panel.plan.md, "Perf fix") needed hand-written timers because (1) every module update uses the same scope name `module.update` with no module identity, (2) profiling and tracing default OFF, so a session had an empty `trace.jsonl` and no `profile.jsonl`, and (3) the only over-budget signal was a per-frame `pu.over_budget` log line (`ProcessingUnit.cpp`), which fed the panel's log push and kept the unit over budget (a feedback loop).

### A1. Module-attributed scopes
- `ScopeRecord` gains `uint32_t subject`: the CRC **value** of the module or PU instance id; zero when not set. `name` stays `module.update` / `pu.update`, so existing filters keep working. `profile.jsonl` gains a `"subject"` field holding the instance id as text (omitted when zero).
- The subject is the 4-byte CRC value, not a `StringCRC`: a `StringCRC` carries a 64-char string (about 68 bytes), which would grow every per-thread closed ring and the `ProfileFileSink` 60 x 512 frame ring by about 2 MB and copy 64 chars per scope. Names are resolved when the file is written, from a subject name table (`Profiler::RegisterSubjectName(uint32_t, const char*)`) filled once when a module or PU is created. An unknown value is written as hex (`"0x1a2b3c4d"`).
- `ScopedZone` gains an overload taking a subject; `Module::Update` and `ProcessingUnit::Update` pass their instance ids. No string building on the hot path (the CRC value is read from the module's id).
- Sections inside a module (`DIA_PROFILE_SCOPE`) inherit the subject of the enclosing module scope through the thread-local open stack, so no call-site changes are needed to attribute them.

### A2. Always-on unit timing (independent of the profile pillar)
- Each `ProcessingUnit` keeps a `UnitTimingTable` (new, pure, `Dia/DiaObservation/Profile/UnitTimingTable.h`): for each module, last ms, EWMA, and a ring of the last 128 frames' ms; plus the unit's own tick ms. Filled by the PU with two `steady_clock` reads per module per frame. Cost target: under 0.5 us per module per frame; the table's own cost is published as the gauge `perf.self_ms`.
- **Frame definition.** One table entry is one forward pass (`RunForwardPass`). Main and Render run one pass per `Update()`. SimPU's fixed-step accumulator runs 0 to N passes per `Update()`, so on Sim an entry is one fixed step, and an `Update()` that runs no step adds no entry. Each Sim `Update()` also records its step count; the counter `pu.<unit>.catchup_updates` counts calls that ran more than one step, so catch-up is visible on its own and is never read as slow modules.
- Gauges: `pu.<unit>.module.<id>.ms` (EWMA), registered when the module starts and unregistered when it stops (the gauge name is built once, off the hot path). Existing `pu.<unit>.last_tick_ms` is unchanged.
- Always compiled in (Debug and Release). Profile scopes stay opt-in (SD-O27 unchanged).

### A3. Trip detector and trip report
- `TripDetector` (new, pure, same folder): per unit, trips when the tick exceeds the budget for `tripFrames` consecutive frames (default 10) or exceeds `tripFactor` x budget in one frame (default 2.0). It re-arms after `rearmFrames` frames under budget (default 30). After a trip, further trips for that unit are suppressed for `minIntervalMs` (default 5000) and counted.
- On a trip, **one** record is written to `trips.jsonl` in the session directory: unit, frame range, budget, peak ms, the top 5 modules by mean ms over the trip window (with sections), the worst frame's full breakdown, the last 60 frames of that unit from the ring, and the suppressed-trip count since the previous record. One Warning log line `pu.trip id=... frames=... peak_ms=... top=...` accompanies it.
- The per-frame `pu.over_budget` Warning in `ProcessingUnit.cpp` is replaced by the counter `pu.<unit>.over_budget_frames` and the trip record. The log line must never be emitted at tick rate.
- Budget: `target_ms` from the unit frequency, times `observation.perf.budget_scale` (default 1.0; Debug runs may set 1.5-2.0). Reports state the build configuration. The budget applies per entry (per fixed step on Sim, see A2), never to a whole multi-step Sim `Update()`.
- The detector ignores the unit's first entry and any entry in which a module of that unit is starting (module start costs), as the old warning skipped tick 1. The old warning's 7% jitter allowance is dropped: `tripFrames` filters jitter, and `budget_scale` covers slow builds.
- The detector runs on the unit's own thread. It never does file I/O there: the trip record goes to the observation drain, which writes `trips.jsonl`. With no session started, only the counter and the log line happen.

### A4. Config and CLI
- `observation.perf { trip_frames, trip_factor, rearm_frames, min_interval_ms, budget_scale }`; CLI `--perf` (shorthand: profile enabled with the ApplicationFlow category, trips on, and a `perf-summary.json` written at exit with per-unit, per-module p50/p95/max and trip counts). `dia run|launch <target> --perf[=seconds]` forwards `--perf` and, when given a duration, exits the app after that many seconds (automation exit path).

### Acceptance criteria (amendment)
| ID | Criterion | Verification |
|----|-----------|--------------|
| AA1 | A `module.update` record in `profile.jsonl` carries `subject` equal to the module's instance id; two modules in one frame are distinguishable | Unit test with two test modules |
| AA2 | A section scope opened inside a module update inherits that module's subject | Unit test |
| AA3 | `UnitTimingTable` reports last, EWMA and p95 per module; a module sleeping 5 ms shows 5 ms +/- 1 ms | Unit test |
| AA4 | With profiling OFF, the per-module gauges still update | Unit test |
| AA5 | `TripDetector`: 10 consecutive frames over budget produce exactly one trip; 1,000 frames over budget produce at most 1 trip per `minIntervalMs` and the next record states the suppressed count | Unit test with a fake clock |
| AA6 | A trip record lists the top modules in descending mean ms and includes the last 60 frames | Unit test |
| AA7 | A unit that is over budget for 1,000 frames writes no more than 1 `pu.trip` log line per 5 s and zero `pu.over_budget` lines | Integration test (the S-1 case: card open, Debug) |
| AA8 | `UnitTimingTable` costs under 0.5 us per module per frame in Release | Benchmark test (reported, not gating) |
| AA9 | `--perf` writes `perf-summary.json` with every module of every unit, including modules that stopped before exit | Integration test |
| AA10 | A Sim `Update()` that runs 3 fixed steps within the per-step budget raises no trip, adds 3 table entries and counts one `catchup_updates`; one with 0 steps adds no entry | Unit test |
| AA11 | No trip is raised on a unit's first entry or on an entry in which one of its modules is starting | Unit test |
| AA12 | `profile.jsonl` writes the subject as the module's instance id text; an unregistered subject value is written as hex | Unit test |

### Binding decisions (amendment)
| ID | Decision | How it applies |
|----|----------|----------------|
| SD-PERF-1 | Module timing is always on and independent of profile categories | A2 is not gated by `profileEnabled` (SD-O27 stays for scopes) |
| SD-PERF-2 | Telemetry never writes at tick rate | A3 replaces the per-frame warning; any future diagnostic needs a rate limit at the source |
| SD-PERF-3 | Pure data types in DiaObservation; hosts in application code | `UnitTimingTable` and `TripDetector` depend only on DiaCore (SD-O21); `ProcessingUnit` (DiaApplicationFlow) fills them |

### Open design questions (amendment)
1. ~~`ScopeRecord` grows by 4 bytes. Is the 512-entry closed ring still within budget?~~ **Resolved 2026-11-10:** it grows by 4 bytes only because the subject is a `uint32_t` CRC value (see A1); a `StringCRC` would have been about 68 bytes.
2. ~~Where does the per-module gauge set live when modules are created and destroyed per stage?~~ **Resolved 2026-11-10:** register on module start, unregister on stop (A2). If `MetricRegistry` has no unregister, adding one is part of task A2.
4. ~~What is a "frame" on SimPU, which runs 0 to N fixed steps per `Update()`?~~ **Resolved 2026-11-10:** one fixed step (A2, A3).
3. Should trips also be raised by a single module exceeding a share of the frame (for example 40%) even when the unit is under budget? (Deferred; the mockup lists it as a trip option.)

### Tasks (amendment)
| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| A1 | `ScopeRecord.subject` (`uint32_t`), subject name table, `ScopedZone` subject overload, `profile.jsonl` field, module/PU call sites, inheritance through the open stack | AA1, AA2, AA12 | Planned | sonnet | Dia |
| A2 | `UnitTimingTable` + PU hookup (per forward pass) + Sim step count and `catchup_updates` + per-module gauges + `perf.self_ms` | AA3, AA4, AA8, AA10 | Planned | sonnet | Dia |
| A3 | `TripDetector` (first-entry and starting-module skips) + `trips.jsonl` via the drain + replace `pu.over_budget` | AA5, AA6, AA7, AA11 | Planned | sonnet | Dia; edits `ProcessingUnit.cpp` |
| A4 | Config block, `--perf` flag, `perf-summary.json`, `dia run|launch --perf[=seconds]` | AA9 | Planned | sonnet | DiaCLI |
| A5 | Update `dia.dia.observation.architecture.module.md` | - | Planned | haiku | |

## Amendment 2026-11-12: worker threads and offline analysis

**Status of this amendment:** `Approved` — 2026-11-12. Builds on Amendment 2026-11-09 (A1 subject) and @docs/specs/applications/dia/systems/diathreading/job-observation.md (`IJobObserver`).
**Why:** DiaDeferredStep (weather, ecology) and in-frame parallel ranges move cost onto job workers. Workers never call `RegisterThreadScopeBuffer`, so their scopes are dropped (AC12). Offline, the only deep view is the Inspector's 60 s export, which needs a live game.

### B1. Worker-thread scopes
- `JobObserver::OnWorkerStart` registers the worker's scope buffer and adds `Worker N` to a new thread-name table (`Profiler::RegisterThreadName(uint32_t threadId, const char*)`). `profile.jsonl` writes the name once per thread as a `record_type:"thread_name"` line.
- Each job runs inside a root scope `job.run` whose subject is the job owner (A1), so `DIA_PROFILE_SCOPE` sections inside the job inherit it without call-site changes.
- `frame_number` on a worker is the global frame counter when the scope opened. Async work (a deferred step spans keys N to N+k) is placed by its timestamps and `tag`, not by frame number.

### B2. Job records in the profile file
- When profiling is on, each `JobTimingRecord` is also written to `profile.jsonl` from the worker's ring (never from a PU thread, never at tick rate on the PU) as:
  `{"record_type":"profile_job","thread_id":12,"submit_thread":3,"owner":"ecology","tag":4,"range":2,"range_count":8,"submit_unix_nano":...,"start_unix_nano":...,"end_unix_nano":...}`
- `owner` is resolved through the subject name table (A1), and unknown values are written as hex. The job's `job.run` scope already carries its duration. This record adds the submit time and the submitting thread, which give queue wait and the cross-thread link.

### B3. Offline converter: `dia perf convert`
- `dia perf convert <session-dir|profile.jsonl> [--out trace.json] [--from s --to s]` writes Chrome-trace JSON that Perfetto loads:
  - one track per thread, named from `thread_name` records;
  - scopes as complete events (`ph:"X"`) with the subject in `args`;
  - `profile_job` records as flow events (`ph:"s"` on the submitting thread at `submit_unix_nano`, `ph:"f"` on the worker's `job.run`), so a job shows as an arrow from its submitter;
  - trips (`trips.jsonl`) and perf markers as instant events (`ph:"i"`).
- Pure Python in DiaCLI, streaming the input (sessions can be large). No engine change.
- This is the offline deep-analysis path. There is no perf Editor plugin for now (decision 2026-11-12, see @perf-inspector.md Revision 2026-11-12).

### Acceptance criteria (amendment 2026-11-12)
| ID | Criterion | Verification |
|----|-----------|--------------|
| AB1 | A `DIA_PROFILE_SCOPE` inside a job appears in `profile.jsonl` with the worker's `thread_id` and the job owner as subject | Integration test |
| AB2 | Each worker has one `thread_name` record (`Worker N`) | Unit test |
| AB3 | With profiling on, each completed job writes one `profile_job` record; with profiling off, none | Unit test |
| AB4 | `dia perf convert` on a recorded session produces JSON that passes a Chrome-trace schema check, with one track per thread, job flow arrows and trip instants | pytest on a fixture session |
| AB5 | `dia perf convert` on a 200 MB `profile.jsonl` runs in bounded memory (streaming, under 500 MB peak) | pytest (reported) |
| AB6 | Manual: the converted S-3 Ecology session loads in Perfetto and shows Sim, Main, Render and worker tracks with job arrows | Manual check recorded in notes |

### Binding decisions (amendment 2026-11-12)
| ID | Decision | How it applies |
|----|----------|----------------|
| SD-PERF-2 | Telemetry never writes at tick rate | `profile_job` records go through the worker ring and the drain, not the PU thread |
| SD-PERF-7 | Telemetry cost budgets (@perf-inspector.md Revision 2026-11-12) | Worker scope and job-record cost count toward the detached budget |

### Open design questions (amendment 2026-11-12)
1. **Ring size on busy workers.** The 512-entry closed ring per thread may overflow when a worker runs many small jobs per frame. Raise it for workers only, or accept drops (which are already counted)? Proposal: keep 512, report drops, and revisit after the 10,000 jobs/s flood benchmark.

### Tasks (amendment 2026-11-12)
| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| B1 | Thread-name table, `thread_name` records, `job.run` root scope with owner subject (in `JobObserver`) | AB1, AB2 | Planned | sonnet | Needs job-observation tasks 2 and 4 |
| B2 | `profile_job` record from the worker ring via the drain | AB3 | Planned | sonnet | |
| B3 | `dia perf convert` (DiaCLI), streaming, flow events, trips and markers | AB4, AB5, AB6 | Planned | sonnet | Can start before B1/B2 against today's `profile.jsonl` (thread tracks and scopes) |
