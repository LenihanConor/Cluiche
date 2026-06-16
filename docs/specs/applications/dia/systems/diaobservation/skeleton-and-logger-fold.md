# Feature Spec: DiaObservation Skeleton + DiaLogger Fold + Async Drain

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diaobservation/diaobservation.md | **skeleton-and-logger-fold** |

**Status:** `Done` — 2026-05-18

**Plan:** [skeleton-and-logger-fold.plan.md](skeleton-and-logger-fold.plan.md)

---

## Problem Statement

The `DiaObservation` system has no module on disk yet, and the existing `DiaLogger` system imposes a runtime cost because its drain (`Logger::FlushBuffers`) runs synchronously on the main PU every tick — directly invoking `printf`/`fflush` and `OutputDebugStringA` per entry from the main thread. This feature creates the new `Dia/DiaObservation/` module by folding `DiaLogger` into it as the `Log/` subsystem, deletes `Dia/DiaLogger/` entirely, and moves the drain off the main thread to remove the per-frame perf hit. It is the first feature of the system because every other observation feature builds on this skeleton.

---

## Solution Overview

This feature is a **two-part landing**: a mechanical fold, then a behavioural change.

### Part A — Skeleton + Fold (mechanical, no behaviour change)

1. Create `Dia/DiaObservation/` as a new module:
   - `DiaObservation.vcxproj` and `.vcxproj.filters` — registered in `Cluiche.sln`
   - `dia.dia.observation.architecture.module.md` — YAML module documentation
   - Subdirectory layout: `Log/` (this feature), `Session/` `Trace/` `Metric/` `Health/` `Config/` (placeholders only — populated by features 2–6), `Testing/` (this feature: mock sink relocation)
2. Move every file from `Dia/DiaLogger/` into `Dia/DiaObservation/Log/`:
   - `Logger.h/.cpp`, `LogEntry.h`, `ISink.h/.cpp`, `LogLevel.h/.cpp`
   - `ThreadLogBuffer.h/.cpp`
   - `StdOutSink.h/.cpp`, `DebugOutputSink.h/.cpp`
   - `AssertSinkBridge.h/.cpp`
   - `DiaLog.h` (the `DIA_LOG_*` macro definitions)
3. Hard-switch the namespace from `Dia::Logger::` to `Dia::Observation::Log::` in moved files.
4. Hard-switch every caller's includes from `<DiaLogger/...>` to `<DiaObservation/Log/...>` and namespace references from `Dia::Logger::` to `Dia::Observation::Log::`. Macro API (`DIA_LOG_TRACE/DEBUG/INFO/WARNING/ERROR`) is preserved verbatim — call sites do not change. ~155 files reference DiaLogger today; all get rewritten.
5. Update `Cluiche.sln`: remove `DiaLogger` project, add `DiaObservation` project. Update every `.vcxproj` that lists DiaLogger as a dependent project.
6. Delete `Dia/DiaLogger/` from disk entirely.
7. Update `docs/reference/registry/module-registry.md`.
8. Update DiaLogger system spec status to `Superseded` (final flip — was `Superseded (pending)`).
9. Verification: build green in Debug + Release. All existing GoogleTests pass. No behaviour change.

### Part B — Async Drain

10. Add a dedicated `std::thread mDrainThread` member to `Logger` plus a `std::atomic<bool> mDrainRunning` flag.
11. On first `RegisterSink()` call (or first `RegisterThreadBuffer()` — whichever comes first after Logger construction), start `mDrainThread` running a loop that wakes every 1 ms, calls the existing internal flush logic, and goes back to sleep.
12. `Logger::FlushBuffers()` becomes a no-op for external callers — it is now the drain thread's exclusive responsibility. Existing call sites in `LoggerModule::DoUpdate` and `EditorModelModule::DoUpdate` are updated to no longer call it (or the call is left as a no-op for safety; pick one — see Implementation Notes).
13. `Logger::Stop()` (new method, called by host module's `DoStop`) signals `mDrainRunning = false`, drains all remaining ring entries synchronously, then `mDrainThread.join()`. Sinks are unregistered AFTER the join completes.
14. On crash / unhandled exception, sinks may receive entries from the drain thread; the existing `AssertSinkBridge::DispatchImmediate` path remains synchronous (bypasses the drain) so asserts surface immediately.
15. Verification: a unit test asserts that sinks are called from a thread that is NOT the test's main thread (proves drain is async).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Dia/DiaObservation/DiaObservation.vcxproj` exists and builds in Debug + Release | `dia pipeline --target diaobservation` returns success |
| AC2 | `Dia/DiaObservation/Log/` contains the 9 file pairs moved from `Dia/DiaLogger/` (Logger, LogEntry, ISink, LogLevel, ThreadLogBuffer, StdOutSink, DebugOutputSink, AssertSinkBridge, DiaLog.h) | File-presence check |
| AC3 | `Dia/DiaLogger/` does not exist on disk | `ls Dia/DiaLogger` returns "no such directory" |
| AC4 | `Dia/DiaLogger.vcxproj` is removed from `Cluiche.sln` | grep `DiaLogger` in `Cluiche.sln` returns zero matches |
| AC5 | All public types in moved files live under `Dia::Observation::Log::` namespace | grep check; compile check |
| AC6 | `DIA_LOG_TRACE/DEBUG/INFO/WARNING/ERROR(channel, fmt, ...)` macros work identically — same arguments, same compile-out behaviour in Release | Existing `TestLogger.cpp` GoogleTest passes unchanged after include path update |
| AC7 | Every `.h`/`.cpp` file that previously included `<DiaLogger/...>` now includes `<DiaObservation/Log/...>` | grep returns zero `<DiaLogger/` matches across `Dia/` and `Cluiche/` |
| AC8 | Every reference to `Dia::Logger::` is replaced with `Dia::Observation::Log::` (excluding the existing DiaLogger system spec, which is documentation) | grep returns zero `Dia::Logger::` matches in `.h`/`.cpp` |
| AC9 | `dia.dia.observation.architecture.module.md` exists with YAML frontmatter (id, name, dependencies, public_api, responsibilities) | File present, YAML validates via `python Tools/dia_modules.py --validate` |
| AC10 | `Cluiche.sln` builds Debug+Release with no warnings introduced by this feature | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` succeeds |
| AC11 | All existing GoogleTests pass after the fold (no behaviour change in Part A) | `dia run googletest` returns success |
| AC12 | DiaLogger system spec status is `Superseded` with forwarding pointer to `diaobservation.md` | Read spec |
| AC13 | `module-registry.md` reflects the rename (DiaLogger removed, DiaObservation added with `Log/` subsystem listed) | Read registry |
| AC14 | `Logger` exposes a public `Stop()` method that signals the drain thread, drains pending entries synchronously, and joins | API check |
| AC15 | After `RegisterSink()` (or `RegisterThreadBuffer()`, whichever first), `Logger` owns a running `std::thread` named or identified as the drain thread | Unit test |
| AC16 | Drain thread wakes every ~1 ms via `std::this_thread::sleep_for(std::chrono::milliseconds(1))` and calls the internal flush | Code review + unit test (latency assertion) |
| AC17 | A unit test using a mock `ISink` records the calling thread's id and asserts it is **not** the test's main thread id | New `TestAsyncDrain.cpp` GoogleTest |
| AC18 | `Logger::Stop()` blocks until the drain thread has processed all pending entries; no log entries are lost on clean shutdown | Unit test: push N entries, call Stop, assert sink received exactly N |
| AC19 | `LoggerModule::DoUpdate` no longer calls `FlushBuffers` (or the call is a documented no-op) | Code review |
| AC20 | `AssertSinkBridge::DispatchImmediate` continues to bypass the drain thread and dispatch synchronously to sinks (asserts surface immediately on the calling thread) | Existing assert test still passes |
| AC21 | Producer-side log call cost (`Logger::Log`) is unchanged from current measurement (vsnprintf + thread-local ring write, no mutex) | Code review (no new locks on producer path) |
| AC22 | If `Logger::Log` is called before any sink registration (i.e. drain thread not yet started), entries land in the thread-local ring and are drained when the thread eventually starts | Unit test |

---

## Public API

The macro API and `ISink` interface are **preserved verbatim** from the existing DiaLogger spec — only namespace and include path change. New API additions are limited to:

```cpp
namespace Dia::Observation::Log {

    class Logger {
    public:
        static Logger& Instance();

        // Sink management — unchanged
        void RegisterSink(ISink* sink);
        void UnregisterSink(ISink* sink);

        // Thread buffer management — unchanged
        void RegisterThreadBuffer();
        void UnregisterThreadBuffer();

        // Direct log entry (called by DIA_LOG macro) — unchanged signature, unchanged producer behaviour
        void Log(LogLevel level, const Dia::Core::StringCRC& channel,
                 const char* fmt, ...);

        // EXISTING — but semantically a no-op for external callers in v1.
        // The drain thread calls the internal flush logic on its 1ms tick.
        // Left in the public API for backward source compatibility; calling it
        // does nothing harmful (returns immediately if drain thread is running).
        void FlushBuffers();

        // NEW — host module's DoStop calls this. Signals drain thread to exit,
        // drains remaining entries synchronously, joins the thread, then returns.
        // After Stop(), Logger::Log calls still write to thread-local rings but
        // those rings will not be drained until a future Start (not in v1).
        void Stop();

        // EXISTING — synchronous bypass for asserts, unchanged.
        void DispatchImmediate(const LogEntry& entry);

    private:
        Logger();
        ~Logger();

        // NEW — drain thread infrastructure
        void DrainLoop();
        std::thread       mDrainThread;
        std::atomic<bool> mDrainRunning;

        // ... existing members preserved (mThreadBuffers, mSinks, mRegistryMutex, etc.) ...
    };

}
```

`LogEntry` is **NOT modified in this feature**. The `timestampNs`, `threadId`, `scenarioStep` fields are added in feature #2 (DiaObservation Foundation) where the producer side is wired up to populate them. This feature treats `LogEntry` as a verbatim move.

`namespace Dia::Logger` ceases to exist after this feature lands. No alias is provided — the system spec answer was a hard switch.

---

## Implementation Notes

### File-move mechanics

- Use `git mv` for each file in `Dia/DiaLogger/` to its new location in `Dia/DiaObservation/Log/`. Preserves blame history.
- After move, edit each file to change the namespace declaration (`namespace Logger` → `namespace Observation { namespace Log`) and update internal includes.
- The `vcxproj` and `.vcxproj.filters` for `DiaObservation` are written from scratch — easier than editing the old `DiaLogger.vcxproj` because the new module has additional subdirectories ready for features 2–6.

### Caller rewrite

- Use `sed` (or equivalent) for the 155 caller files: `sed -i 's|<DiaLogger/|<DiaObservation/Log/|g; s|Dia::Logger::|Dia::Observation::Log::|g'`
- Spot-check ~10 files manually after the sweep to catch edge cases (multi-line includes, comments mentioning DiaLogger, etc.). Exclude `docs/` from the sweep — the historical DiaLogger system spec must keep its references.
- Update every `.vcxproj` that has DiaLogger as a dependent project: replace `<ProjectReference Include="..\DiaLogger\DiaLogger.vcxproj">` with the DiaObservation equivalent.

### Drain thread design

- `Logger::Logger()` constructor does NOT start the drain thread — too early in static-init order.
- `Logger::RegisterSink(sink)`: if `mDrainRunning == false`, transition to `true` and `mDrainThread = std::thread(&Logger::DrainLoop, this)`. Use `std::call_once` to ensure single start.
- `DrainLoop()` body:
  ```
  while (mDrainRunning.load(std::memory_order_acquire))
  {
      // Drain all thread buffers (existing FlushBuffers internals)
      InternalFlush();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // On exit: one final drain to catch entries pushed after the last sleep
  InternalFlush();
  ```
- `InternalFlush()` is the existing `FlushBuffers` body, renamed and made private.
- `Logger::Stop()`:
  ```
  if (!mDrainRunning.exchange(false))
      return;  // already stopped or never started
  if (mDrainThread.joinable())
      mDrainThread.join();
  // Sinks may now be unregistered safely.
  ```
- The existing `mRegistryMutex` continues to protect sink list and thread buffer registry — drain thread acquires it during `InternalFlush`, same as today's `FlushBuffers`. No new locks introduced on the producer path.

### `LoggerModule::DoUpdate` change

- Currently: `Dia::Logger::Logger::Instance().FlushBuffers();`
- After this feature: leave a comment explaining drain is async; call removed. `DoUpdate` may become empty (no-op) — that's fine.
- `LoggerModule::DoStop` MUST call `Logger::Stop()` before `UnregisterSink` to avoid use-after-free of sinks while the drain thread is still running.

### Edge case: `Logger::Log` before drain start

- A thread calls `RegisterThreadBuffer()` and then `Log()` BEFORE any `RegisterSink()` happens. The entries land in the ring; drain thread is not yet running. When sinks register and drain starts, those entries are picked up. AC22 covers this.

### Edge case: assert during drain

- `AssertSinkBridge` calls `Logger::DispatchImmediate(entry)` which acquires `mRegistryMutex` and dispatches synchronously to sinks on the asserting thread. The drain thread may be mid-flush holding the mutex — this serialises correctly. Assert thread blocks briefly, then proceeds. Existing behaviour preserved.

### Bypass guard for re-entrancy

- The existing `tFlushing` thread-local guard in `Logger::FlushBuffers` prevents recursive flush. Preserved in `InternalFlush` — a sink that itself logs (e.g. `JsonLineSink` that fails a write and logs the failure) will not recurse.

---

## Dependencies

### Required Modules
- **DiaCore** — `StringCRC`, `String1024`, `DynamicArrayC`, `Log::OutputLine` (for DebugOutputSink — preserved). Existing dependency, just from a new module location.

### Dependent Features (downstream in the system spec)
- **diaobservation-foundation** (feature #2) — adds `timestampNs`, `threadId`, `scenarioStep` to `LogEntry`; introduces `JsonLineSink` and `SessionManager`.
- **diaobservation-config** (feature #3) — wires `.diagame` `observation` block into `Logger` sink registration.
- All other DiaObservation features — depend on the module skeleton landing.

### Dependent Modules (caller-side rewrite scope)
- 155 files across `Dia/` and `Cluiche/` reference `DiaLogger` today. Every one is updated in this feature.
- Notably: `DiaAPI`, `DiaApplicationFlow`, `DiaAssetCatalogueEditor`, `DiaAssetRuntime`, `DiaDebugServer`, `DiaEditor`, `DiaWebSocket`, `DiaUICEF`, `DiaUIUltralight`, `DiaPython`, `DiaPipeline`, `DiaRig2D`, `DiaRigidBody2D`, `DiaSoftBody2D`, `DiaAnimation2D`, plus `CluicheTest`, `CluicheEditor`, and `Cluiche/Tests/GoogleTests/DiaLogger/` (which itself moves to `Cluiche/Tests/GoogleTests/DiaObservation/Log/`).

---

## Testing Strategy

### Existing tests (must continue passing — proves no behaviour change in Part A)

- `Cluiche/Tests/GoogleTests/DiaLogger/TestLogger.cpp` — moves to `Cluiche/Tests/GoogleTests/DiaObservation/Log/TestLogger.cpp`, includes updated, otherwise unchanged. All cases pass.

### New unit tests (`Cluiche/Tests/GoogleTests/DiaObservation/Log/TestAsyncDrain.cpp`)

1. **`DrainHappensOffMainThread`** — Register a mock sink that records the calling `std::this_thread::get_id()`. Push 100 entries from the main thread. Wait up to 50 ms. Assert the mock sink received entries from a thread id different from the test's main thread id. Covers AC17.
2. **`StopDrainsAllPendingEntries`** — Register a mock sink. Push 1000 entries rapidly from main thread. Immediately call `Logger::Stop()`. Assert the mock sink received exactly 1000 entries (no loss). Covers AC18.
3. **`StopJoinsDrainThread`** — After `Stop()`, assert `mDrainThread.joinable() == false` (or that a subsequent `mDrainThread.join()` is a no-op). Covers AC14.
4. **`AssertBypassesDrain`** — Register a mock sink that records calling thread id. Trigger an assert via `AssertSinkBridge`. Assert the mock sink received the assert entry on the asserting thread, not the drain thread. Covers AC20.
5. **`PreSinkLogsAreNotLost`** — `RegisterThreadBuffer()`, log 10 entries, then register a sink. Wait for drain. Assert the sink received all 10 entries. Covers AC22.
6. **`MainThreadFrameTimeIsNotInflatedByLogging`** — (Optional, performance-flavour) loop 1000 iterations, each iteration measure time around 100 `DIA_LOG_INFO` calls; assert per-iteration time below a generous bound (e.g. 1ms) — proves producer side stays cheap. Soft assertion; tagged as informational rather than blocking.

Tests 1–5 are required for AC sign-off. Test 6 is optional / informational.

### Build verification

- `dia pipeline --target diaobservation --config Debug` and `--config Release` both succeed.
- `dia run googletest` runs every existing test and the new ones; all pass.
- Spot-check `dia run cluichetest` boots cleanly to the dummy stage and shuts down without log loss.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | Channel IDs remain StringCRC (existing). No raw strings introduced. |
| PD-002 | Platform | ProcessingUnit/Phase/Module | Logger continues to be host-driven via `LoggerModule` (in `CluicheGameBaseline`). Drain thread is implementation-internal — does not introduce a new `Module`. |
| PD-003 | Platform | Component-based entities | Orthogonal — feature is engine infrastructure, no entity work. |
| PD-004 | Platform | No STL containers in public APIs | Public API uses `String1024`, `DynamicArrayC` (preserved). New internals use `<thread>`, `<atomic>`, `<chrono>` — these are NOT containers, allowed under PD-004. |
| PD-005 | Platform | x64 Windows only | `std::thread`, `std::atomic`, `std::chrono::steady_clock` all well-supported on x64 MSVC. |
| PD-006 | Platform | .vcxproj source of truth | `DiaObservation.vcxproj` and `.vcxproj.filters` written and registered in `Cluiche.sln`. `DiaLogger.vcxproj` removed. |
| PD-007 | Platform | C++20 required | `std::atomic<bool>::exchange`, `std::this_thread::sleep_for`, `std::source_location` all C++20-compatible. |
| PD-008 | Platform | Directory.Build.props owns OutDir | `DiaObservation.vcxproj` does NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. Inherits everything. |
| PD-009 | Platform | Generated output under Cluiche/out/ | No generated output added in this feature. |
| PD-010 | Platform | .diagame is project root | No config consumed in this feature (config lands in feature #3). |
| AD-001 | Dia App | Module system with YAML frontmatter | `dia.dia.observation.architecture.module.md` created; `dia.logger.architecture.module.md` removed. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Observation::Log::` after the fold. Old `Dia::Logger::` namespace ceases to exist. |
| AD-005 | Dia App | Component-based entities | Orthogonal. |
| SD-O01 | System | One module covering all four pillars | This feature creates the module skeleton with `Log/` populated and `Trace/`, `Metric/`, `Health/`, `Session/`, `Config/` reserved as empty subdirectories for features 2–6. |
| SD-O02 | System | DiaLogger folded into `Dia/DiaObservation/Log/`; `Dia/DiaLogger/` deleted | Direct execution of this decision. AC2, AC3, AC4 enforce it. |
| SD-O03 | System | Logger drain on dedicated thread, not main PU | Direct execution. AC15, AC16, AC17 enforce it. |
| SD-O04 | System | Two distinct sinks coexist (human + AI JSON-line) | This feature preserves `StdOutSink` and `DebugOutputSink`. `JsonLineSink` lands in feature #2. |
| SD-O05 | System | `schema_version: "1.0"` on every record | Not exercised in this feature — `LogEntry` is moved verbatim. Schema commitments land in feature #2. |
| SD-O06 | System | OTel wire-format alignment, no SDK adoption | Not exercised — no JSON serialization happens in this feature. |
| SD-O07 | System | Session = directory at `Cluiche/out/<App>/sessions/<id>/` | Not exercised — `SessionManager` lands in feature #2. |
| SD-O14 | System | Stable monotonic timestamp on every record | Not exercised — `LogEntry.timestampNs` lands in feature #2. |
| SD-O20 | System | Test utilities ship in `Dia/DiaObservation/Testing/` | Mock sink relocates here as part of this feature. (Today's mock sinks live in `Cluiche/Tests/GoogleTests/DiaLogger/Helpers/` — those move into `Dia/DiaObservation/Testing/`.) |
| SD-O21 | System | DiaCore is the only required dependency | Compliance: dependency on `DiaCore` only. No new dependencies introduced. |
| SD-O22 | System | DiaCore cannot include observation headers | Preserved — DiaCore continues using `Dia::Core::Log::OutputLine`. No DiaCore file gains an observation include. |
| SD-O23 | System | DiaLogger system spec marked Superseded | AC12 enforces — final flip from `Superseded (pending)` to `Superseded`. |

All Binding=Yes decisions either apply directly (executed by this feature) or are non-applicable in this feature's scope (deferred to features 2–6 with notes above). No conflicts.

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Fold mechanics | Should the include-path rewrite be done with `sed` or per-file manual edits? | `sed` for the bulk sweep, manual spot-check on ~10 files for edge cases (multi-line includes, comments mentioning DiaLogger). | Use `sed` for the bulk; spot-check 10 files manually. Exclude `docs/` so the historical DiaLogger spec stays intact. |
| 2 | Fold mechanics | Should we use `git mv` to preserve blame history, or `cp + rm` for a clean break? | `git mv` per file. Blame history matters for the logger code which has been stable for a long time. | `git mv` per file. |
| 3 | Drain thread | Where does the drain thread start — in `Logger::Logger()` constructor, or lazily on first `RegisterSink`? | Lazily on first `RegisterSink` via `std::call_once`. Avoids static-init order issues. Constructor runs too early (before `main`). | Lazy start on first `RegisterSink`. |
| 4 | Drain thread | What happens if `Logger::Log` is called from a thread that has not called `RegisterThreadBuffer`? | Same as today — entry silently dropped (programmer error). Drop is logged via `Dia::Core::Log::OutputLine` once per process to surface the bug. | Match existing behaviour: silent drop. Adding the warning is a follow-up if needed. |
| 5 | Drain thread | Does the drain thread need a name (for debugger visibility on Windows)? | Yes — set via `SetThreadDescription("DiaObservation::Log::Drain")` on Windows (PD-005). Cheap to add, big debugging payoff. | Yes — set thread name on Windows for debugger visibility. |
| 6 | Shutdown | What if `Logger::Stop()` is called twice? | Idempotent — second call is a no-op. `mDrainRunning.exchange(false)` returns false on second call; early-return. Documented in the API comment. | Idempotent; second call is a no-op. |
| 7 | Shutdown | What if a sink throws inside `OnLogEntry` during the final drain in `Stop()`? | Same as today — sinks must not throw. If one does, `std::terminate` fires (existing contract). Document in `ISink` comment. | Sinks must not throw; existing contract preserved. |
| 8 | Backward compat | Does anything outside the macro API depend on `Dia::Logger::*` directly? | Yes — `DebugServer::DebugServerLogSink` and `EditorConsoleSink` (in CluicheEditor) inherit from `Dia::Logger::ISink`. These get rewritten to `Dia::Observation::Log::ISink` in this feature. | All `ISink` implementers get their inheritance rewritten in this feature; they're in the 155-file count. |
| 9 | Test reorg | Where do the existing DiaLogger GoogleTests live after the move? | `Cluiche/Tests/GoogleTests/DiaObservation/Log/` — mirrors the source structure. `TestLogger.cpp` becomes `Cluiche/Tests/GoogleTests/DiaObservation/Log/TestLogger.cpp`; new `TestAsyncDrain.cpp` joins it. | Move tests to `Cluiche/Tests/GoogleTests/DiaObservation/Log/`. |
| 10 | Test helpers | The existing DiaLogger mock sink (if any) — does it live in DiaLogger or in tests? | Today: any test-side mock lives in `Cluiche/Tests/GoogleTests/DiaLogger/`. After this feature: per SD-O20, mocks live in `Dia/DiaObservation/Testing/MockSink.h`. Relocation is part of this feature. | Mock sink moves to `Dia/DiaObservation/Testing/MockSink.h`; tests include from there. |
| 11 | Drain perf | Is 1ms tick too aggressive at idle? At 1000 wakeups/sec the drain thread costs ~0.1% of one CPU core when idle. | Acceptable for v1 simplicity. If profiling shows it's an issue (it won't), switch to condition-variable wake in a follow-up. | Acceptable; revisit only if profiling shows it. |
| 12 | Drain perf | When the drain thread reaches `InternalFlush` and finds nothing in any ring, does it still do work? | It still acquires `mRegistryMutex` and walks every thread buffer. For ~4 threads with empty rings, this is microseconds. Cheap. | Acceptable; existing pattern. |
| 13 | Lifecycle | What if `Logger::Log` is called AFTER `Logger::Stop()`? | The entry lands in the thread-local ring; drain thread is gone, so nothing reads it. On process exit the ring is leaked (memory reclaimed by OS). Not a correctness problem; document the behaviour. | Entry lost; documented as "do not log after Stop". |
| 14 | Lifecycle | Can `Logger::Stop()` be followed by a new `RegisterSink` to restart the drain? | No — Stop() is one-shot for v1. If a use case emerges, add `Restart()` later. Throwing or asserting on a registration after Stop is the safe default. | One-shot Stop; assert in Debug, no-op in Release if RegisterSink follows Stop. |
| 15 | Verification | How do we prove "no behaviour change" in Part A before Part B begins? | Run the entire GoogleTest suite before the fold (record green), apply the fold, run again (must be green). If green, Part B begins. If red, fix the fold first. | Run full test suite as gate between Part A and Part B. |
| 16 | Verification | Should this feature ship as one PR or two (Part A: fold, Part B: drain)? | Two PRs — Part A is mechanical and reviewable in isolation; Part B is the interesting change and benefits from a focused review. Both PRs ship in the same feature. | Two PRs in the same feature. |
| 17 | Module doc | What does the YAML frontmatter for `dia.dia.observation.architecture.module.md` declare in this feature? | Module ID `dia.observation`. Subsystems: `Log` (populated), `Session/Trace/Metric/Health/Config` (declared but empty). Public API: enumerate the moved DiaLogger headers. Dependencies: `dia.core`. Each subsystem header listed. | YAML enumerates moved headers + declares empty subsystems for features 2–6. |
| 18 | Registry | Does `module-registry.md` need a row for each subsystem of DiaObservation? | One row for `DiaObservation` itself; subsystem-level entries are not the registry's pattern (look at how DiaCore is documented). | One row for DiaObservation; subsystem detail lives in the `dia.dia.observation.architecture.module.md` file. |
| 19 | Risk | Highest risk in this feature? | Caller-rewrite missing a file, build fails late. Mitigation: comprehensive grep before commit, full Debug+Release build before PR. Second-highest: drain-thread shutdown race losing entries on Stop. Mitigation: AC18 unit test covers it. | Confirmed risks; mitigations specified. |
| 20 | Rollback | If something breaks, can we roll back without losing other in-flight work? | Yes — both PRs are isolated. Part A revert restores DiaLogger; Part B revert restores synchronous drain. Neither is entangled with non-observation work. | Both PRs are individually revertible. |

All 20 review questions have answers. No outstanding items.

---

## Open Questions

None.

---

## Status

`Approved` — 2026-05-17. All 27 Binding Decisions marked Compliant; all 20 AI Review Questions answered; no open questions. Ready for implementation.

**Next:** create plan file `docs/specs/features/dia/diaobservation/skeleton-and-logger-fold.plan.md` when implementation begins.
