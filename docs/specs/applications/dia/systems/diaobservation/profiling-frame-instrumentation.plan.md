# Plan: Profiling Frame Instrumentation — Amendment 2026-11-09

**Spec:** @docs/specs/applications/dia/systems/diaobservation/profiling-frame-instrumentation.md (section "Amendment 2026-11-09")
**Status:** Not Started
**Scope:** the amendment only (A1-A5). The original 14 tasks of the feature are already built (`Dia/DiaObservation/Profile/` exists).
**Order:** this plan first, then `perf-live-control.plan.md`, then `perf-inspector.plan.md`.

## Sign-offs needed before Task 1 (AD-001)

| Change | Where | Status |
|---|---|---|
| `ScopeRecord` field + `ScopedZone` subject overload | DiaObservation | Given 2026-11-12 |
| `Module::Update` / `ProcessingUnit::Update` call sites, PU timing hookup, removal of `pu.over_budget` | DiaApplicationFlow | Given 2026-11-12 |
| `observation.perf` config block, `--perf` | DiaObservation config, DiaCLI | Given 2026-11-12 |

Ask once for all three before dispatch. The existing Profile code is built and Approved, so these are amendments to built modules.

## Design questions (resolved in the spec 2026-11-10)

Questions 1, 2, 4 and 5 below are now decided in the spec's amendment (A1-A3, AA10-AA12). The table keeps the reasoning for reference.

| # | Question | Proposal |
|---|---|---|
| 1 | **Blocking. The spec's "grows by 4 bytes" is wrong.** `StringCRC` carries a 64-char string plus the CRC (about 68 bytes), so a `StringCRC subject` adds about 68 bytes to `ScopeRecord` (about 112 bytes today). This grows each thread's 512-entry closed ring and `ProfileFileSink`'s 60 x 512 frame ring (about 3.4 MB today, about 5.5 MB after). It also copies 64 chars per scope on the hot path | **Decided (spec A1):** `uint32_t` CRC value + `Profiler::RegisterSubjectName`; unknown values written as hex |
| 2 | Per-module gauges when modules come and go per stage | **Decided (spec A2):** register on start, unregister on stop |
| 3 | Trip on one module's share of the frame | Deferred by the spec, not in this plan |
| 4 | **Blocking. SimPU has no single "frame".** `ProcessingUnit::Update` on Sim runs `RunForwardPass` 0 to N times per call (fixed-step accumulator, catch-up cap). The spec's per-module "ms per frame", the 128-frame ring and the trip budget don't say whether a frame is one `Update()` call or one fixed step. A catch-up `Update()` of 3 steps legitimately takes about 3x budget and would trip on `trip_factor` 2.0 | **Decided (spec A2/A3):** one entry per fixed step, budget per step, no entry for 0 steps, plus a `pu.<unit>.catchup_updates` counter |
| 5 | The current warning skips tick 1 (module start costs) and allows 7% jitter. The spec's `TripDetector` mentions neither | **Decided (spec A3):** skip the first entry and entries with a starting module; drop the 7% |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Get the sign-offs (spec questions are already resolved). Then record `sizeof(ScopeRecord)`, `sizeof(ProfileFileSink)` and the existing Profile test names on the current tree | - | Not Started | haiku | No code. Gate for every later task. Known locations: `ProcessingUnit.cpp` ~50 gauge registration, ~213 `pu.update`, ~217 `tickStart` (`high_resolution_clock`), ~301 `pu.over_budget`; `Module.cpp` ~253 `module.update` inside `FrameTick` `kActive`; thread locals in `Profiler.cpp` ~20-36 |
| 1 | **A1a.** RED then GREEN: `ScopeRecord.subject` (`uint32_t` CRC value, 0 = unset); `ScopedZone` subject overload; new parallel `tOpenSubject[kOpenStackCapacity]` thread local so a nested section inherits the subject; `GetOpenScopesSnapshot` carries it | AA2, AA12, part of AA1 | Done | sonnet | Dia/DiaObservation/Profile. Includes `Profiler::RegisterSubjectName` and hex fallback. TDD per `.claude/skills/tdd.md`. Spec task A1; 27 Profiler tests pass (--shards 1; ProfileFixture filename collision under shards>=2); sizeof(ScopeRecord) 112->120; RED proven |
| 2 | **A1b.** `profile.jsonl` emits `"subject"` (omitted when 0); `profile-final.json` too. `Module::Update` and `ProcessingUnit::Update` pass instance ids (CRC stored at construction, no string building per frame) | AA1 (two test modules in one frame are distinguishable) | Done | sonnet | Touches `Module.cpp`, `ProcessingUnit.cpp`; ProfileFixture may need a subject helper. Spec task A1; 475 scoped tests pass, deps OK, RED proven; subject name table (256, process-wide) falls back to hex once ZZ_ table-full test has run |
| 3 | **A2a.** `UnitTimingTable` (pure, DiaCore-only): per module last ms, EWMA, ring of 128 entries (entry = fixed step on Sim, per open question 4), unit tick ms; p50/p95 accessors; API takes ms values, no clock inside | AA3 table math (the "5 ms module reads 5 ms +/- 1" part needs a real clock: tested in Task 4) | Done | sonnet | `Dia/DiaObservation/Profile/UnitTimingTable.h/.cpp` (SD-PERF-3). Fixed arrays, no STL in public API. Module capacity must match `ProcessingUnit`'s module array cap. Spec task A2; UnitTimingTable (kMaxModules 64, ring 128, EWMA 0.2); 10 tests pass, RED proven (LNK2019), deps OK |
| 4 | **A2b.** `ProcessingUnit` owns a table and fills it from `RunForwardPass` (one clock read between modules, so N+1 reads per pass, not 2N); reuse the existing `tickStart`/`tickEnd` for unit tick ms and switch them to `steady_clock`; per-module EWMA gauges `pu.<unit>.module.<id>.ms` registered on module start / removed on stop (name built once, off the hot path); `perf.self_ms` gauge; Sim step count and `pu.<unit>.catchup_updates` | AA3 real-clock part (sleeping test module), AA4 (gauges update with profiling OFF), AA8 (benchmark, reported not gating), AA10 | Done | sonnet | Always compiled in, both configs (SD-PERF-1). Open question 2 resolved here. Check whether `MetricRegistry` supports unregister; if not, that is a Dia change to raise. Spec task A2; PU hookup, gauges (register-once, zero on stop; no MetricRegistry unregister, user decision), catchup_updates, perf.self_ms. 5 timing tests pass; AA8 758 ns/module/pass (config unchecked). InputProfileTest flakes under shards, passes alone |
| 5 | **A3a.** `TripDetector` (pure): consecutive-frames and single-frame-factor triggers, re-arm, min interval, suppressed count; injectable clock | AA5 (10 frames = exactly one trip; 1,000 frames = at most one per `minIntervalMs`, next record states suppressed count) | Done | sonnet | Mutation check applies: pure threshold logic (see `feedback_mutation_checks_scope`). Includes the first-entry and starting-module skips (AA11). Spec task A3; TripDetector pure, 16 tests pass, RED proven, deps OK. Mutation check dropped (feedback_no_mutation_checks 2026-11-12) |
| 6 | **A3b.** Trip record builder + `trips.jsonl` writer: unit, frame range, budget, peak, top 5 modules by mean ms (with sections when profiling on), worst frame breakdown, last 60 frames, suppressed count; one `pu.trip` Warning line per record | AA6 (descending order, 60 frames), drain hand-off | Done | sonnet | Session directory from `SessionManager`; states build configuration. The detector runs on the unit's own thread (Sim, Render): hand the record to the observation drain, never `fwrite` on the PU thread. Without a session (tests, no `SessionManager::Start`) only the counter and log line happen. Spec task A3; TripRecord + TripReporter (own writer thread, 8-slot queue, started by SessionManager); 16 tests, RED proven. Sections hook left empty. TripReporter is a singleton (debt, flagged) |
| 7 | **A3c.** Wire into `ProcessingUnit.cpp`: remove the per-frame `pu.over_budget` Warning, add counter `pu.<unit>.over_budget_frames`, feed the detector; check `LogPushBatcher.h` / `DebugPanelPageModule.cpp` references to `pu.over_budget` and update | AA7 (1,000 frames over budget: at most 1 `pu.trip` per 5 s, zero `pu.over_budget`) | Done | sonnet | Integration test with a sleeping test module and an injected clock (1,000 real over-budget frames make a slow test). The other `over_budget` mentions are comments only (`LogPushBatcher.h:12`, `DebugPanelPageModule.cpp:395`): update them. Spec task A3; TripDetector wired in PU; reporter injected via Application/ObservationModule; pu.over_budget removed; 4 PUTrip tests pass. NO RED run (tests written after code). Trip measure = forward-pass sum, not whole Update() |
| 8 | **A4a.** `ObservationConfig` `observation.perf { trip_frames, trip_factor, rearm_frames, min_interval_ms, budget_scale }` + `--perf` (profile enabled with ApplicationFlow category, trips on) | AA9 (part) | Not Started | haiku | Additive, defaults per spec; amends Feature 3 files. Spec task A4 |
| 9 | **A4b.** `perf-summary.json` at exit: per unit, per module p50/p95/max, trip counts | AA9 (every module of every unit present) | Not Started | sonnet | Written from `UnitTimingTable`. Modules that stop before exit (stage changes) would be missing: keep a per-module summary row past stop. Spec task A4 |
| 10 | **A4c.** DiaCLI: `dia run` and `dia launch` take `--perf[=seconds]` and forward `--perf`; with a duration, the game exits after N seconds | CLI unit test + one live run on `cluichetest` | Not Started | sonnet | Launch code is `Dia/DiaCLI/dia_cli/cli/launch.py` (`launch_target`). Exit path: an in-game timer on `--perf=N` (needs no debug server) rather than `dia.app.quit` over the wire. Never call the exe directly. Spec task A4 |
| 11 | **Gap audit and edge tests** (explicit task): ring wrap, subject 0, depth 64, zero modules, budget_scale 0 and negative, trip with fewer than 60 frames, unit with no budget, disk-full on `trips.jsonl`. Mutation check on `TripDetector` thresholds only | All AA | Not Started | sonnet | `feedback_exhaustive_test_pass`. Also: Sim `Update()` with many catch-up steps hitting the cap, subject name table full, same subject registered twice |
| 12 | **A5.** Update `dia.dia.observation.architecture.module.md` (new types, `observation.perf`), `dia docs registry`, flip the spec amendment tasks, record the measured `sizeof(ScopeRecord)` before and after in the spec's open question 1 | - | Not Started | haiku | `dia docs` commands, not manual edits where one exists. Spec task A5 |
| 13 | **Verification.** Scoped `--filter="*Profile*:*Trip*:*UnitTiming*:*ProcessingUnit*"` with `--shards 2` while iterating (done per task); one full `dia run googletest --shards 2` here, plus `dia check deps`, `dia check sanitizer`, a Release build of GoogleTests and `dia pipeline --target cluichetest`, then a real `dia run cluichetest --perf=20` smoke | AA1-AA12 | Not Started | sonnet | Cheap checks per task; the heavy gates run once, here (`feedback_verification_cadence`). Kill leftover `CluicheTest.exe` first |

## Commit rhythm

One commit per task, plan updated in the same commit (`dia docs plan <path> <#> --status Done --notes ...`; status values are the CLI canonical ones: Not Started, In Progress, Blocked, Done).

## Observation scan (at task 13)

This feature is the instrumentation. Scan its own new code for logs on trip-writer I/O failures, a gauge for dropped trips, and an `IHealthReporter` for `trips.jsonl` write failure.

## Notes

- Sections inside a module inherit the subject through the open stack, so no call-site edits are needed in other modules.
- Build order dependency: Tasks 3-4 are independent of 1-2 and can run in parallel only if they touch no shared header. Both touch `ProcessingUnit.cpp` (2, 4, 7), so keep them serial.
- **After this plan: smoke it on S-1** (`dia run cow --perf=30` on the S-1 stage, card open) before starting live control. If the trip records answer the S-1 question, re-decide how urgent the other two plans are.
- Baseline comparisons against a worktree are not possible (`feedback_dia_cli_worktrees`); take the `sizeof` and timings in Task 0 on the current tree.
