# Feature Spec: DiaObservation Foundation

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diaobservation/diaobservation.md | **foundation** |

**Status:** `Done` — 2026-05-19

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #1 (skeleton-and-logger-fold) — implementation is serial; spec may proceed in parallel.

---

## Problem Statement

After Feature #1 lands, the `DiaObservation` module has a logger with an async drain but no session identity, no per-session output directory, no structured JSON-line sink, and no mechanism to capture how a run ended or whether it crashed. This feature delivers the full observation session substrate that all later pillars (traces, metrics, health) and the future `DiaE2E` framework depend on.

---

## Solution Overview

`SessionModule` (concrete, lives in `CluicheGameBaseline/`) owns a `SessionManager` by value. `SessionManager` drives the session lifecycle:

1. **Start** — generates a session ID (`YYYYMMDD-HHMMSS-XXXXXXXX`), creates the session directory `Cluiche/out/<App>/sessions/<id>/`, constructs + registers a `ObservationFileSink` with the Logger, installs `std::set_terminate`.
2. **Tick** — no-op in v1 (future: retention ring age-out, health polling interval).
3. **Stop (clean)** — polls health reporters, flushes pending log entries, writes `session.json`, unregisters `ObservationFileSink`.
4. **Stop (terminate/assert)** — `set_terminate` handler calls `SessionManager::EmergencyDump()`, which writes a best-effort `crashes/<n>.json` directly (bypassing the Logger sink stack) then re-raises or aborts.

`SessionManager` is a plain class — **no singleton**. All external subsystems that need session context (ObservationFileSink path, scenario step tag) receive it at construction time or via explicit registration.

The `session.json` and `log.jsonl` schemas are frozen at `v1.0` by this feature (SD-O08).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `SessionModule` starts successfully: session directory exists at `Cluiche/out/<AppName>/sessions/<YYYYMMDD-HHMMSS-XXXXXXXX>/` after `DoStart` completes | Unit test asserts directory is created with correct format |
| AC2 | Session ID format is `YYYYMMDD-HHMMSS-XXXXXXXX` (UTC date, UTC time, 8 lowercase hex chars); fits in `char[32]` | Unit test validates format via regex/manual parse |
| AC3 | Two `SessionModule` starts in the same second produce different session IDs | Unit test starts two sessions back-to-back, asserts IDs differ |
| AC4 | `ObservationFileSink` is auto-registered with `Logger` on `SessionManager::Start` (when enabled — default true); auto-unregistered on `Stop` | Unit test: log entry written after Start appears in `log.jsonl`; nothing written after Stop. Feature #3 adds the config knob; Feature #2 hardcodes enabled=true. |
| AC5 | `log.jsonl` records contain: `schema_version:"1.0"`, `ts_unix_nano`, `session_id`, `level`, `severity_number`, `channel`, `scenario_step`, `thread_id`, `msg` — all fields present and correctly typed | Unit test parses a written record and asserts each field |
| AC6 | `severity_number` follows OTel mapping: Trace=1, Debug=5, Info=9, Warning=13, Error=17 | Unit test logs one entry at each level, asserts correct number |
| AC7 | `ts_unix_nano` is captured at producer side (not drain side) — monotonic `std::chrono::steady_clock` | Unit test: two entries written in tight loop; assert `ts_unix_nano` of second > first |
| AC8 | `scenario_step` in log record reflects the active step tag pushed on `SessionManager` at time of log call | Unit test: push step, log, pop step, log; assert first record carries step, second carries empty |
| AC9 | `session.json` is written on clean stop; contains all required top-level keys: `schema_version`, `session`, `exit`, `counts`, `scenario_steps`, `modules`, `retained_warnings_and_errors`, `files` | Unit test starts + stops a session, parses `session.json`, asserts all keys present |
| AC10 | `exit.reason` is `"normal_exit"` on clean stop | Unit test asserts field value |
| AC11 | `exit.reason` is `"terminate"` when `std::terminate` is called; `crashes/0.json` is written | Unit test spawns subprocess that calls `std::terminate`; asserts both files exist |
| AC12 | `crashes/<n>.json` contains: `schema_version`, `session` (id + app), `exit.reason`, `retained_warnings_and_errors`, `open_log_entries_at_crash` | Assert fields after subprocess crash |
| AC13 | Retention ring holds the last 256 `Warning`/`Error` entries; older entries are evicted on overflow (FIFO) | Unit test writes 300 Warning entries, asserts ring contains exactly 256 and they are the last 300 − 256 = 44 onwards |
| AC14 | Retained entries are embedded in `session.json` under `retained_warnings_and_errors` and in `crashes/<n>.json` | Assert after clean stop and after crash-subprocess respectively |
| AC15 | `session.json` `files` array lists every artifact created (at minimum: `log.jsonl`); each entry has `path`, `kind`, `bytes` | Assert after stop |
| AC16 | Scenario step push/pop is LIFO; log records carry the innermost active step | Unit test: push A, push B, log (expect B), pop, log (expect A), pop, log (expect empty) |
| AC17 | Calling `SessionManager::Start()` a second time (without Stop) returns false and logs a warning; no second directory is created | Unit test |
| AC18 | If `SessionManager::Start()` is never called, `ObservationFileSink` is not registered and `Logger` continues to work via StdOut/Debug sinks — no crash | Unit test: log without session; assert no crash, no jsonl file |
| AC19 | `session.json` is written via temp-file-and-rename on clean stop (atomic write) | Unit test: assert no partial `session.json` appears during the write interval (observe via filesystem watcher or assert rename semantics) |
| AC20 | On crash, `session.json` is written directly (no temp-rename) before process exits | Assert `session.json` exists after crash subprocess terminates |
| AC21 | `session.json` `counts.errors` and `counts.warnings` match the number of Error/Warning log entries written during the session | Unit test |
| AC22 | `SessionModule` compiles cleanly; `dia pipeline --target cluichetest` green in Debug + Release | Build verification |

---

## Public API

### `SessionManager` (lives in `Dia/DiaObservation/Session/`)

```cpp
namespace Dia::Observation {

struct SessionConfig {
    char appName[64];
    char buildVersion[64];
    char buildConfig[16];   // "Debug" | "Release"
    char outRootDir[256];   // absolute path to Cluiche/out/<AppName>/
};

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager();

    // Lifecycle — called by SessionModule
    bool Start(const SessionConfig& config);
    void Tick(float deltaTime);
    void Stop();

    // Identity
    const char* GetSessionId() const;       // "YYYYMMDD-HHMMSS-XXXXXXXX"
    const char* GetSessionDirectory() const; // absolute path

    // Scenario tagging (thread-local stack per caller thread)
    void PushScenarioStep(const Dia::Core::StringCRC& step);
    void PopScenarioStep();
    Dia::Core::StringCRC GetCurrentScenarioStep() const;

    // Health registration (used by features #6)
    void RegisterHealthReporter(Health::IHealthReporter* reporter);
    void UnregisterHealthReporter(Health::IHealthReporter* reporter);

    // Crash/terminate path — called by set_terminate handler; writes crashes/<n>.json
    // then re-raises. MUST NOT log via DIA_LOG_* (re-entrant).
    void EmergencyDump();

private:
    bool           mStarted = false;
    char           mSessionId[32]  = {};
    char           mSessionDir[512] = {};
    SessionConfig  mConfig         = {};

    // Retention ring — 256 Warning/Error LogEntry copies
    // (ring is a fixed circular buffer, not DynamicArrayC, to avoid allocation on crash path)
    // ...

    Log::ObservationFileSink* mObservationFileSink = nullptr; // owned, registered with Logger on Start
};

} // namespace Dia::Observation
```

### `SessionModule` (lives in `Cluiche/CluicheGameBaseline/Modules/`)

```cpp
// Concrete application module — owns SessionManager by value.
// Mirrors existing LoggerModule pattern.
class SessionModule : public Dia::Application::IModule {
public:
    static const Dia::Core::StringCRC kUniqueId;

    void DoInit(const Dia::Application::ModuleContext& ctx) override;
    void DoStart() override;
    void DoUpdate(float dt) override;
    void DoStop() override;

private:
    Dia::Observation::SessionManager mSessionManager;
};
```

### `ObservationFileSink` (lives in `Dia/DiaObservation/Log/`)

```cpp
namespace Dia::Observation::Log {

class ObservationFileSink : public ISink {
public:
    // path: absolute path to log.jsonl in the session directory
    // epochOffsetNs: system_clock - steady_clock at session start, added to every ts_unix_nano
    explicit ObservationFileSink(const char* path, const char* sessionId, int64_t epochOffsetNs);
    ~ObservationFileSink() override;

    void OnLog(const LogEntry& entry) override;

private:
    // FILE* or equivalent — written synchronously on the drain thread
    // (drain thread is owned by Logger, not by this sink)
    FILE*  mFile      = nullptr;
    char   mSessionId[32] = {};
};

} // namespace Dia::Observation::Log
```

### Session ID generation (internal helper)

```cpp
// Dia/DiaObservation/Session/SessionIdGenerator.h  (internal, not public API)
namespace Dia::Observation::Internal {
    // Writes "YYYYMMDD-HHMMSS-XXXXXXXX\0" into out[32].
    // Uses UTC time + mt19937 seeded from steady_clock ^ process id.
    void GenerateSessionId(char (&out)[32]);
}
```

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaObservation/Session/SessionManager.h/.cpp` | New |
| `Dia/DiaObservation/Session/SessionIdGenerator.h/.cpp` | New (internal) |
| `Dia/DiaObservation/Log/ObservationFileSink.h/.cpp` | New |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add new files |
| `Dia/DiaObservation/DiaObservation.vcxproj.filters` | Add new files |
| `Cluiche/CluicheGameBaseline/Modules/SessionModule.h/.cpp` | New (concrete app module) |
| `Cluiche/CluicheGameBaseline/CluicheGameBaseline.vcxproj` | Add SessionModule |
| `Dia/DiaObservation/Testing/MockSessionManager.h` | New (test utility) |
| `Dia/DiaObservation/Testing/SessionFixture.h` | New (test utility) |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Session step tags, module name, reason codes, sink registration — all use `StringCRC`. Session ID itself is a `char[32]` (it is an output artefact string, not an engine identifier). |
| PD-002 | ProcessingUnit/Phase/Module architecture | `SessionModule` is a concrete `IModule` hosted in `CluicheGameBaseline`. `SessionManager` is plain data owned by the module — not tied to PU/Phase. |
| PD-003 | Component-based entities | Orthogonal — observation is engine infrastructure, not entity data. |
| PD-004 | No STL containers in public APIs | `SessionManager` and `ObservationFileSink` public APIs use only `char[]`, `StringCRC`, `float`, `bool`, `IHealthReporter*`. Internal use of `<chrono>`, `<atomic>`, `<thread>`, `<random>` permitted. Retention ring is a fixed circular buffer, not `std::vector`. |
| PD-005 | x64 only | `std::chrono::steady_clock`, `thread_local`, `GetCurrentProcessId` all x64-native. |
| PD-006 | VS project files are source of truth | `DiaObservation.vcxproj` and `CluicheGameBaseline.vcxproj` updated manually. |
| PD-007 | C++20 required | Uses `<chrono>`, `<atomic>`, `<random>` (all pre-C++20 but valid). No C++20-specific features used in this feature beyond what Feature #1 already established. |
| PD-008 | Directory.Build.props owns build settings | `DiaObservation.vcxproj` does not override `OutDir`, `IntDir`, `PlatformToolset`, `LanguageStandard`. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | Session directories written to `Cluiche/out/<AppName>/sessions/<id>/`. |
| PD-010 | `.diagame` is project root; typed imports route loading | Feature #2 does not read `.diagame` — that is Feature #3 (Config). `SessionConfig` is populated by `SessionModule::DoInit` from hardcoded constants for now; Feature #3 will add the `.diagame` path. |
| AD-001 | Module system with YAML frontmatter | `dia.dia.observation.architecture.module.md` updated in Feature #1; this feature adds `Session/` to the module's declared subsystems. |
| AD-002 | No STL containers in public APIs | Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new code in `Dia::Observation::` and `Dia::Observation::Log::`. |
| SD-O01 | One module for all four pillars | `Session/` lives inside `Dia/DiaObservation/`, not a sibling module. |
| SD-O04 | Two sinks: human console + AI JSON-line | `ObservationFileSink` is added as the AI JSON-line sink; `StdOutSink`/`DebugOutputSink` from Feature #1 continue unchanged. |
| SD-O05 | `schema_version: "1.0"` on every record | `ObservationFileSink::OnLog` emits `"schema_version":"1.0"` on every line; `session.json` writer emits it at top level. |
| SD-O06 | OTel wire-format, no SDK | `log.jsonl` fields use OTel names (`severity_number`, `ts_unix_nano`). No `opentelemetry-cpp` dependency. |
| SD-O07 | Session directory at `Cluiche/out/<App>/sessions/<id>/` | `SessionManager::Start` creates this path via `CreateDirectoryA`. |
| SD-O08 | Session layout + `session.json` are public contracts; frozen at v1.0 | All keys documented in schema section below; changes bump `schema_version` major. |
| SD-O09 | Sessions flat; suites are siblings | `sessions/` is flat under the app's `out/` dir. `suites/` not touched here. |
| SD-O10 | Health reporters polled at exit AND on crash | `SessionManager::Stop()` calls `PollHealthReporters()`. `EmergencyDump()` also calls it. |
| SD-O13 | `ended_unix_nano: null` means "started but never closed" | `scenario_steps` entries in `session.json` carry `null` for steps active at crash/terminate. |
| SD-O14 | Monotonic `uint64_t timestampNs` at producer side | `LogEntry::timestampNs` captured in `Logger::Log` (producer side), not in drain. |
| SD-O15 | 256-entry never-drop retention ring | Fixed circular buffer in `SessionManager`; `OnRetainableEntry()` called by drain thread. |
| SD-O16 | Session ID on every record | `ObservationFileSink` writes `session_id` from the `mSessionId` it received at construction. |
| SD-O20 | Test utilities in `Dia/DiaObservation/Testing/` | `MockSessionManager.h`, `SessionFixture.h` shipped here. |
| SD-O21 | No singleton; DiaCore is only required dependency | `SessionManager` is a plain class owned by `SessionModule`. `DiaObservation` only includes `DiaCore`. |
| SD-O22 | DiaCore cannot include observation headers | `SessionManager` and `ObservationFileSink` do not include any DiaCore-internal headers. |
| **CONFLICT — SD-O21 vs system spec Public Interface** | System spec (§ Public Interfaces → Session) shows `static SessionManager& Instance()` | **Resolution:** System spec API block is aspirational; interview confirmed module-owned design with no singleton. Feature #2 implements the non-singleton pattern. System spec §Public Interfaces needs an amendment once this feature is Approved. No implementation impact. |

---

## Wire Schemas (frozen at v1.0 by this feature)

### `log.jsonl` record

```json
{
  "schema_version": "1.0",
  "ts_unix_nano":   1747484400000000000,
  "session_id":     "20260517-143022-a3f2c1b9",
  "level":          "info",
  "severity_number": 9,
  "channel":        "asset",
  "scenario_step":  "load",
  "thread_id":      12345,
  "msg":            "Loaded texture: player.png"
}
```

`scenario_step` is `""` (empty string) when no step is active.

### `session.json` top-level shape

```json
{
  "schema_version": "1.0",
  "session": {
    "id":               "20260517-143022-a3f2c1b9",
    "app_name":         "CluicheTest",
    "build_version":    "0.1.0",
    "build_config":     "Debug",
    "platform":         "win64",
    "started_unix_nano": 1747484400000000000,
    "ended_unix_nano":   1747484460000000000,
    "duration_ms":       60000
  },
  "exit": {
    "reason":        "normal_exit",
    "code":          0,
    "stage_at_exit": ""
  },
  "counts": {
    "errors":                   0,
    "warnings":                 2,
    "frames":                   3600,
    "scenario_steps_completed": 1
  },
  "scenario_steps": [
    { "step": "smoke", "started_unix_nano": 1747484401000000000, "ended_unix_nano": 1747484450000000000 }
  ],
  "modules": [],
  "retained_warnings_and_errors": [],
  "files": [
    { "path": "log.jsonl", "kind": "log", "bytes": 51200 }
  ]
}
```

`exit.reason` enum: `"normal_exit"` | `"terminate"` | `"assert"` *(future, via AssertSinkBridge)*.

`scenario_steps[n].ended_unix_nano` is `null` for steps active at crash (SD-O13).

`modules` array is empty in Feature #2; populated by Feature #6 (health reporters).

`stage_at_exit` is `""` in Feature #2; populated by DiaApplicationFlow integration (future).

### `crashes/<n>.json` shape

```json
{
  "schema_version": "1.0",
  "session": {
    "id":       "20260517-143022-a3f2c1b9",
    "app_name": "CluicheTest"
  },
  "exit": {
    "reason": "terminate"
  },
  "retained_warnings_and_errors": [ /* last ≤256 Warning/Error entries */ ],
  "open_log_entries_at_crash":    [ /* entries in thread rings not yet drained */ ]
}
```

`<n>` is a zero-padded integer counting crash files in the session directory (starts at `0`).

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | System spec `§Public Interfaces → Session` shows `static SessionManager& Instance()`. Feature #2 implements a non-singleton. System spec needs amendment. | Flagged as CONFLICT in binding table above. Amendment to system spec is a documentation task after this feature is Approved. No implementation impact. |
| OQ2 | Feature #3 (Config) will populate `SessionConfig` from `.diagame`. Feature #2 hardcodes values in `SessionModule::DoInit`. Is a TODO comment acceptable, or must the integration point be explicitly stubbed? | Acceptable to hardcode in Feature #2; Feature #3 will override. No stub needed — Feature #3 reads the same `SessionModule::DoInit` and replaces the hardcoded values. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Crash | `EmergencyDump` is called from `std::set_terminate`. Can it safely call `CreateFile` / `fopen` / `fwrite` on Windows from a terminate context? | Yes — Windows terminate handlers run on the faulting thread before CRT shutdown; file I/O is safe. `EmergencyDump` must NOT call `malloc` (use stack buffers), must NOT call `DIA_LOG_*` (re-entrant), must NOT call `Logger::FlushBuffers` (drain thread may be in an inconsistent state). Documented as constraint in implementation notes. |
| 2 | Crash | What if a second `std::terminate` fires inside `EmergencyDump`? | Re-entrant `EmergencyDump` is guarded by an `std::atomic_flag` (test-and-set). If already running, the re-entrant call returns immediately. After `EmergencyDump` finishes it calls `std::abort()` to ensure the process actually exits. |
| 3 | Threading | The retention ring is written from the drain thread and read from `SessionManager::Stop()` or `EmergencyDump()` (potentially different threads). How is it synchronised? | The retention ring is a fixed circular buffer protected by a `std::mutex`. Lock is held only for the duration of one entry push or a full copy-out. Not a hot path (only Warning/Error entries) so mutex is acceptable. |
| 4 | Threading | `PushScenarioStep` / `PopScenarioStep` — is the scenario step stack thread-local? | Yes. Each thread has its own `thread_local` LIFO stack (fixed size, e.g. 8 deep). `LogEntry::scenarioStep` is populated in `Logger::Log` by reading the calling thread's top-of-stack. No cross-thread step propagation (SD-O22 context; OTel would use context propagation — out of scope for v1). |
| 5 | Threading | `ObservationFileSink::OnLog` is called from the Logger drain thread. Is the `FILE*` accessed from any other thread? | No. `ObservationFileSink` is opened on `SessionManager::Start` (main thread) and closed on `SessionManager::Stop` (main thread, after drain-thread join). All writes go through `OnLog` on the drain thread. The `FILE*` is single-owner after construction. |
| 6 | Schema | `log.jsonl` uses `ts_unix_nano` but `LogEntry::timestampNs` is captured from `steady_clock` (monotonic, not epoch-based). How is the field correctly named `unix_nano` if it is not wall-clock? | Industry practice: compute a one-time epoch offset at session start — `epochOffsetNs = system_clock::now() - steady_clock::now()` (both sampled as close together as possible). `ObservationFileSink` receives this offset at construction and emits `ts_unix_nano = entry.timestampNs + epochOffsetNs`. Steady clock remains the producer-side timestamp (monotonic, no NTP jumps); the offset converts to UNIX epoch only at serialisation. Matches OTel convention. Single-field wire format — no second field needed. |
| 7 | Schema | `session.json` `counts.frames` — who increments this? `SessionManager::Tick` is a no-op in v1. | `counts.frames` is incremented by `SessionModule::DoUpdate` via `SessionManager::IncrementFrameCount()` (a simple `uint64_t` atomic add). Added to `SessionManager`'s public interface. CluicheTest and CluicheEditor both tick their modules, so the counter is accurate for both apps. Headless tools that don't tick will show 0 — acceptable, as `counts.frames` is informational context for the E2E consumer, not a correctness signal. |
| 8 | Schema | `files` array: who populates it? `ObservationFileSink` writes `log.jsonl`, but `SessionManager` owns `session.json`. Does `SessionManager` query the directory at stop time, or does each sink register its artifact? | `SessionManager` queries the session directory at stop time (`FindFirstFile`), stats each file, and builds the `files` array. Simpler than a registration protocol; no risk of stale entries if a sink fails to write. |
| 9 | Session ID | Session ID collision probability: same second, same process (two `Start` calls in < 1 s). AC3 requires different IDs. Does the 8-char hex (2^32 space) guarantee this? | `mt19937` seeded with `steady_clock::now().count() ^ GetCurrentProcessId()`. Two consecutive calls within the same second differ in `steady_clock` by at least a few hundred nanoseconds, changing the seed. Collision probability ≈ 1/2^32 per pair. Acceptable for a game engine. |
| 10 | Session ID | Is `YYYYMMDD-HHMMSS` UTC or local time? | UTC. `gmtime_s` used for formatting. Local time would make cross-machine CI comparisons ambiguous. |
| 11 | Lifecycle | What happens if the session directory cannot be created (e.g. permissions, disk full)? | `SessionManager::Start` returns `false` and logs a `DIA_LOG_ERROR` via the StdOut/Debug sinks (which do not depend on the session directory). `ObservationFileSink` is NOT registered. The session continues to function without file output. |
| 12 | Lifecycle | `SessionModule::DoStop` — does it need to wait for the Logger drain thread to finish before calling `SessionManager::Stop`? | Yes. `Logger::Stop()` (Feature #1) joins the drain thread before returning. `SessionModule::DoStop` must call `Logger::Stop()` before `mSessionManager.Stop()`, ensuring all log entries are flushed to `ObservationFileSink` before `session.json` is written and the sink is closed. Order documented in implementation notes. |
| 13 | Retention ring | The ring holds `LogEntry` copies (each ~1080 bytes per Feature #1 design). 256 × 1080 ≈ 275 KB. Is this acceptable as a static allocation inside `SessionManager`? | Yes. `SessionManager` is owned by `SessionModule` (stack-allocated inside a `ProcessingUnit`). 275 KB on the stack is too large; the ring buffer must be heap-allocated (`new` in `Start`, `delete` in destructor) or declared as a fixed member array in a heap-allocated sub-object. Implementation note: use `DynamicArrayC<LogEntry>` pre-allocated to 256 at `Start` rather than a raw stack array. |
| 14 | Testing | AC11 requires a subprocess that calls `std::terminate`. How is this done in a GoogleTest? | Using `ASSERT_DEATH` (GoogleTest's death-test mechanism, which spawns a subprocess). The child process calls `SessionManager::Start`, then `std::terminate`. The parent asserts the child exited non-zero and that the expected files exist in the temp session directory. |
| 15 | Build | `SessionModule` lives in `CluicheGameBaseline`. Does `CluicheGameBaseline.vcxproj` already have a `Modules/` filter, or does this feature create it? | Feature #1 does not touch `CluicheGameBaseline`. This feature creates `Cluiche/CluicheGameBaseline/Modules/SessionModule.h/.cpp` and adds both files to the vcxproj + filters. Check existing filters before creating a new one. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `SessionIdGenerator` — implement `GenerateSessionId(char(&)[32])` with UTC timestamp + mt19937 hex suffix | AC2, AC3 | Planned | haiku | Internal helper; no public API surface |
| 2 | `SessionManager` skeleton — `Start`/`Stop`/`Tick`; directory creation; `ObservationFileSink` wiring; `set_terminate` install | AC1, AC4, AC17, AC18, AC22 | Planned | sonnet | Core of this feature |
| 3 | `ObservationFileSink` — `OnLog` writes full JSON-line record per AC5 schema | AC4, AC5, AC6, AC7, AC8 | Planned | sonnet | epoch offset from AI-Q6 |
| 4 | `SessionManager` retention ring — 256-entry FIFO for Warning/Error entries | AC13, AC14 | Planned | sonnet | Heap-allocated per AI-Q13 |
| 5 | `session.json` writer — all fields per schema section; temp-file-and-rename on clean stop | AC9, AC10, AC15, AC19, AC21 | Planned | sonnet | |
| 6 | `EmergencyDump` + `set_terminate` handler — crash path writes `crashes/<n>.json` | AC11, AC12, AC20 | Planned | sonnet | Re-entrancy guard per AI-Q2 |
| 7 | Scenario step stack — `thread_local` LIFO; `PushScenarioStep`/`PopScenarioStep`/`GetCurrentScenarioStep` | AC8, AC16 | Planned | haiku | |
| 8 | `SessionModule` in `CluicheGameBaseline` — `DoInit`/`DoStart`/`DoUpdate`/`DoStop`; hardcoded `SessionConfig` | AC22 | Planned | haiku | Feature #3 will add `.diagame` population |
| 9 | Test utilities — `SessionFixture.h`, `MockSessionManager.h` in `Testing/` | Supporting AC1–AC21 | Planned | haiku | |
| 10 | GoogleTests — all AC1–AC21 covered | All ACs | Planned | sonnet | Death test for AC11 per AI-Q14 |
| 11 | Update `DiaObservation.vcxproj` + `.vcxproj.filters`; update `CluicheGameBaseline.vcxproj` + `.vcxproj.filters` | AC22 | Planned | haiku | |
| 12 | Update `docs/reference/registry/module-registry.md` | — | Planned | haiku | |

---

## Status

`Approved` — 2026-05-17. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
