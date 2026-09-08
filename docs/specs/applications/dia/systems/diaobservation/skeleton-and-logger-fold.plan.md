# Implementation Plan: DiaObservation Skeleton + DiaLogger Fold + Async Drain

**Spec:** [skeleton-and-logger-fold.md](skeleton-and-logger-fold.md)
**System Spec:** [diaobservation.md](diaobservation.md)
**Created:** 2026-05-18

---

## Session Notes

### Spec Decisions Summary

This feature is a two-part landing. **Part A** is purely mechanical: move `Dia/DiaLogger/` into `Dia/DiaObservation/Log/`, hard-switch namespace `Dia::Logger::` → `Dia::Observation::Log::`, update all ~122 callers, rewire vcxprojs/sln, delete the old module. No behaviour change — existing tests must pass unchanged. **Part B** adds a dedicated drain thread to `Logger`: lazy start on first `RegisterSink` (via `std::call_once`), 1 ms sleep loop, `Stop()` method for `LoggerModule::DoStop`. `FlushBuffers()` body becomes `InternalFlush()` (private); external `FlushBuffers()` becomes a no-op. `DispatchImmediate` stays synchronous. `LoggerModule::DoUpdate` stops calling `FlushBuffers`.

**Key constraints (PD-004, AD-003):** No STL containers in public API; namespace is `Dia::Observation::Log::`. `LogEntry` is NOT modified — that lands in Feature #2. `DiaObservation.vcxproj` must NOT override OutDir/IntDir/toolchain (PD-008). New module GUID: `{5E6F7A8B-C9D0-E1F2-3456-789ABCDE0123}`. DiaLogger GUID being removed: `{7A8B9C0D-E1F2-3456-7890-ABCDEF012345}`.

**Current Logger.cpp quirk:** `RegisterSink`/`UnregisterSink` do NOT currently hold `mRegistryMutex` — Part B must add that protection to avoid a race with the drain thread.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `DiaObservation.vcxproj` + `.vcxproj.filters` | Compiles empty lib | Done | haiku | StaticLibrary; DiaCore reference only; include dirs `./;./../;`; GUID `{5E6F7A8B-C9D0-E1F2-3456-789ABCDE0123}`; subdirs Log/ Session/ Trace/ Metric/ Health/ Config/ Testing/ as empty placeholders in filters |
| 2 | Create `dia.dia.observation.architecture.module.md` | File present | Done | haiku | id `dia.observation`; deps `dia.core`; public_api = all Log/ headers; subsystems Session/Trace/Metric/Health/Config declared but empty; status active |
| 3 | `git mv` all 18 DiaLogger source files to `DiaObservation/Log/` | Build (pre-namespace edit) | Done | haiku | Files: Logger.h/.cpp, LogEntry.h, ISink.h/.cpp, LogLevel.h/.cpp, ThreadLogBuffer.h/.cpp, StdOutSink.h/.cpp, DebugOutputSink.h/.cpp, AssertSinkBridge.h/.cpp, DiaLog.h. Use git mv to preserve blame. After move, DiaLogger dir has only vcxproj/filters left. |
| 4 | Update namespace in all 18 moved files | Compile moved files | Done | sonnet | In each .h/.cpp: `namespace Logger` → `namespace Observation { namespace Log`; close with `} // namespace Log\n} // namespace Observation`. Internal includes: `"DiaLogger/Foo.h"` → `"DiaObservation/Log/Foo.h"`. Update DiaObservation.vcxproj ClCompile/ClInclude entries. |
| 5 | Bulk-rewrite all 122 caller files: includes + namespace refs | `grep -r '<DiaLogger/' --include="*.h" --include="*.cpp" Dia/ Cluiche/` returns zero | Done | sonnet | `sed -i 's|<DiaLogger/|<DiaObservation/Log/|g; s|Dia::Logger::|Dia::Observation::Log::|g'` across all .h/.cpp under Dia/ and Cluiche/ **excluding** docs/ and Dia/DiaLogger/ itself. Also fix ISink subclasses: `DebugServerLogSink`, `EditorConsoleSink`, `DiaVisualDebuggerConsole`. Spot-check 10 files manually post-sweep. |
| 6 | Replace DiaLogger project reference with DiaObservation in all dependent .vcxproj files | Build | Done | haiku | Old GUID: `{7A8B9C0D-E1F2-3456-7890-ABCDEF012345}`. New GUID: `{5E6F7A8B-C9D0-E1F2-3456-789ABCDE0123}`. Path: `..\DiaLogger\DiaLogger.vcxproj` → `..\DiaObservation\DiaObservation.vcxproj`. Find affected .vcxproj files with grep. |
| 7 | Move DiaLogger test files to `GoogleTests/DiaObservation/Log/`; update GoogleTests.vcxproj + .vcxproj.filters | Build | Done | haiku | `git mv` TestLogger.cpp, TestAssertSinkBridge.cpp, TestISink.cpp, TestLogLevel.cpp, TestThreadLogBuffer.cpp. Update includes in each test file. Update GoogleTests.vcxproj ClCompile paths. |
| 8 | Update `Cluiche.sln`: remove DiaLogger project, add DiaObservation project | Solution builds | Done | haiku | Remove the `{7A8B9C0D-E1F2-3456-7890-ABCDEF012345}` project block + config platforms block. Add DiaObservation block (mirroring DiaLogger's placement under Library folder). Update any dependency sections that reference DiaLogger GUID. |
| 9 | Delete `Dia/DiaLogger/` from disk | `ls Dia/DiaLogger` → no such dir | Done | haiku | At this point only DiaLogger.vcxproj + .vcxproj.filters remain in the dir. Delete the directory. |
| 10 | **Part A verification gate** — `dia pipeline --target googletest` green + `grep -r '<DiaLogger/' returns zero + `grep -r 'Dia::Logger::' --include="*.h" --include="*.cpp" Dia/ Cluiche/` returns zero | All existing logger tests pass | Done | sonnet | Run full googletest suite. Fix any missed callers before proceeding to Part B. AC1–AC13 verified here. |
| 11 | Add async drain to `Logger` | Unit tests in task 12 | Done | sonnet | Add `std::once_flag mDrainOnce`, `std::thread mDrainThread`, `std::atomic<bool> mDrainRunning{false}` to Logger. Rename `FlushBuffers` body → private `InternalFlush()`. `FlushBuffers()` becomes no-op (external compat). Add mutex guard to `RegisterSink`/`UnregisterSink` (currently unguarded — race with drain thread). `RegisterSink` calls `std::call_once(mDrainOnce, [this]{ mDrainRunning=true; mDrainThread=std::thread(&Logger::DrainLoop,this); })`. Add `DrainLoop()` (1 ms sleep loop calling InternalFlush). Add `Stop()`: exchange mDrainRunning false, join thread. Set thread name `SetThreadDescription(L"DiaObservation::Log::Drain")` on Windows. AC14–AC16, AC21. |
| 12 | Write `TestAsyncDrain.cpp` in `GoogleTests/DiaObservation/Log/`; add to vcxproj | `dia run googletest --filter="AsyncDrain*"` — 5 tests pass | Done | sonnet | Tests: DrainHappensOffMainThread, StopDrainsAllPendingEntries, StopJoinsDrainThread, AssertBypassesDrain, PreSinkLogsAreNotLost. Use a `CaptureSink` that records calling thread::id. AC17, AC18, AC20, AC22. |
| 13 | Update `LoggerModule`: `DoUpdate` no-op; `DoStop` calls `Stop()` before `UnregisterSink` | Build + cluichetest boots | Done | haiku | AC19. DoStop order: `Stop()` → `UnregisterSink` → `UnregisterThreadBuffer` → `delete sinks`. |
| 14 | **Part B verification gate** — `dia run googletest --filter="Logger*"` + `dia run googletest --filter="AsyncDrain*"` all pass; `dia pipeline --target cluichetest` green | AC14–AC22 verified | Done | sonnet | Confirm no race conditions under repeated runs. |
| 15 | Update `module-registry.md` | File updated | Done | haiku | Remove DiaLogger row; add DiaObservation row with Log/ subsystem. Update total count. |
| 16 | Flip DiaLogger system spec to `Superseded` | File updated | Done | haiku | `docs/specs/systems/dia/dialogger.md` — status `Superseded`, add forwarding pointer to diaobservation.md. AC12. |
| 17 | **Final integration gate** — `dia pipeline --target googletest` + `dia pipeline --target cluichetest` + `dia pipeline --target cluicheeditor` all green in Debug | All ACs verified | Done | sonnet | `grep DiaLogger Cluiche/Cluiche.sln` → 0 matches. AC4, AC7, AC8, AC10, AC11. |

---

## Dependency Graph

```
Task 1 (vcxproj) ──► Task 2 (module doc)         [parallel]
                  └──► Task 3 (git mv files)
                              │
                              ▼
                        Task 4 (namespace edit in moved files)
                              │
                              ▼
                        Task 5 (bulk caller rewrite)
                              │
                        Task 6 (vcxproj ref swap)  [parallel with 5]
                              │
                        Task 7 (test file move)     [parallel with 5]
                              │
                              ▼
                        Task 8 (sln update)
                              │
                              ▼
                        Task 9 (delete DiaLogger/)
                              │
                              ▼
                       Task 10 (Part A gate) ──────────────────────────────┐
                              │                                             │
                              ▼                                             ▼
                       Task 11 (drain thread)                     Task 15 (registry)
                              │                                    Task 16 (spec flip)
                              ▼
                       Task 12 (TestAsyncDrain)
                              │
                              ▼
                       Task 13 (LoggerModule update)
                              │
                              ▼
                       Task 14 (Part B gate)
                              │
                              ▼
                       Task 17 (final gate)
```

---

## Parallelization Opportunities

- **Tasks 1 and 2** can run in parallel (different files)
- **Tasks 5, 6, 7** can run in parallel after Task 4 (touch different file sets)
- **Tasks 15 and 16** can run in parallel after Task 10 (docs only)

---

## Notes

- The bulk `sed` in Task 5 must exclude `docs/` (historical DiaLogger system spec keeps its references intentionally) and must exclude already-moved files inside `DiaObservation/Log/` (they already have the new namespace after Task 4).
- `RegisterSink`/`UnregisterSink` currently lack mutex protection — this is safe today because they're always called from the main thread before drain starts. Part B breaks that assumption: the drain thread calls `InternalFlush` (holding mRegistryMutex) concurrently with potential `UnregisterSink` calls. Adding the mutex to Register/Unregister is mandatory.
- MockSink for TestAsyncDrain: inline `CaptureSink` struct (same pattern as existing TestLogger.cpp), recording `std::this_thread::get_id()` per call. No separate MockSink.h needed unless Task 12 requires shared use — it doesn't.
- DiaObservation.vcxproj will NOT add subdirectory `.cpp` entries for Session/Trace/Metric/Health/Config yet — those dirs exist as empty placeholders in the filters file only, added when each feature ships.
