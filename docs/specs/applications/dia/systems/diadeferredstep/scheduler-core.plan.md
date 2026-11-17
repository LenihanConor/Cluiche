# Plan: Scheduler Core (DiaDeferredStep Feature 1)

**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/scheduler-core.md
**Status:** Not Started
**System plan:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.plan.md (row 1)

After **Task 1** the headers exist, so Feature 2 and Feature 3 can start in parallel with the rest of
this plan. Tasks 3–9 all edit `DeferredStepScheduler.h/.cpp` and are therefore strictly sequential —
do not dispatch them in parallel.

## Implementation Patterns

### Module skeleton (Task 0)
- `Dia/DiaDeferredStep/` as a flat folder with a `Testing/` subfolder, matching DiaCellHistory, not a
  `Docs/` subfolder (DiaSimTime's shape is the older one).
- Module doc `Dia/DiaDeferredStep/dia.diadeferredstep.architecture.module.md`, `dia.module.v1`
  frontmatter per @docs/reference/registry/module-metadata-schema.md. `dependent_modules` starts as
  `DiaCore` + `DiaObservation` only; Features 2–4 add DiaThreading, DiaSaveGame, DiaApplicationFlow,
  DiaStreams as they need them, each with a `dia check deps` run.
- `DiaDeferredStep.vcxproj` + `.filters` copied from `DiaCellHistory`'s shape (same Debug/Release x64
  pair, same `Directory.Build.props` inheritance — add no build settings, PD-008). Register with
  `dia check sln-sync`.
- `GoogleTests.vcxproj` gets a `DiaDeferredStep` project reference and a
  `Cluiche/Tests/GoogleTests/DiaDeferredStep/` filter.
- New files are always added with `dia docs vcxproj-add DiaDeferredStep "<File>" --filter <Filter>`
  (feedback_use_dia_docs_commands), from PowerShell with literal paths.

### Headers (Task 1)
- Namespace `Dia::DeferredStep` throughout (AD-003), one type per header, `#pragma once`.
- `StringCRC` constants are **not** `constexpr` — `StringCRC`'s `const char*` ctor is a runtime table
  lookup. Declare `static const Dia::Core::StringCRC kX;` in the header, define in the `.cpp`
  (the convention `DiaSimTimeModule`, `DiaRenderTime` and `AIBudgetModule` already follow).
- `LaunchToken` is a struct wrapping `uint32_t`, not a typedef, so a raw integer cannot be passed by
  accident. 0 is invalid. Encoding is the launcher's business; the scheduler only stores and compares.
- `RangeFn` is `std::function<void(uint32_t, uint32_t)>`, following DiaThreading's `JobFn` precedent
  (the explicit AD-002 carve-out in the system spec). No other STL in any public signature.
- `SaveGame::SaveContext` / `LoadContext` are **forward-declared only** in `IDeferredStep.h`; this
  feature does not link DiaSaveGame.

### Scheduler internals (Tasks 3–9)
- One fixed table: `DynamicArrayC<ChannelEntry, kMaxChannels>` with `kMaxChannels = 8`. Linear search
  by `StringCRC` — 8 entries, so no hashing.
- `ChannelEntry` holds: `channelId`, `IDeferredStep* step`, `StepState state`, `StepKey kickKey`,
  `StepKey applyKey`, `StepKey keyFloor`, `LaunchToken token`, `bool hasToken` (Feature 3 needs the
  flag; add it now so `Tick` never consults the launcher for a tokenless channel),
  `uint64_t kickSequence`, `StepKey completionSeenKey`, `bool completionSeen`, `ChannelStats stats`.
- **Kick order** is a monotonically increasing `mKickSequence` counter stamped onto the entry at
  `Kick`, *not* derived from keys — two channels can share a key (AC 14). `Tick`'s apply pass sorts
  the due set by `kickSequence`; with ≤ 8 entries that is an insertion sort into a stack array, no
  allocation.
- `Tick` is two passes over the table: (1) a **poll pass** over in-flight, not-yet-due channels
  calling `IsComplete` once each to record `completionSeenKey` for slack; (2) an **apply pass** over
  the due set in kick sequence order. Both passes are allocation-free.
- Timing uses `std::chrono::steady_clock` in the `.cpp` only, matching `Tracer.cpp` /
  `ProfileScopedZone.cpp`. Durations are stored as `uint64_t` micros.
- Non-monotonic `Tick` (AC 17) and the destructor's registered-channel check (AC 4) use `DIA_ASSERT`
  in Debug plus a log in both configurations — assert for the programmer, log for the session.
- No singleton, no file static other than the `StringCRC` constant definitions
  (feedback_no_singletons).

### Observation (Task 9)
- `DeferredStepObservation.h/.cpp` owns the one log channel `StringCRC` (`"DeferredStep"`) and the
  reason/zone name constants, so no other file hand-rolls a CRC.
- `DIA_LOG_WARNING` / `DIA_LOG_INFO` per AC 33. **Nothing is logged per frame** — the poll pass is
  silent.
- `DIA_TRACE_ZONE` + `DIA_PROFILE_SCOPE` around `Capture`, `Apply`, the stall `Wait` and `DrainAll`.
  Zone names carry the channel.
- Metrics are *not* registered here (Feature 4 does that); this feature only maintains
  `SchedulerStats` / `ChannelStats` so Feature 4 has something to read.

### Test helpers (Task 2)
- `Dia/DiaDeferredStep/Testing/` (feedback_testing_utilities) — header-only, so GoogleTests needs no
  extra link target.
- `ManualStepLauncher`: a fixed table of `{IDeferredStep*, LaunchInfo, bool completed, bool released}`
  plus a `DynamicArrayC<Call, 64>` call log. `Wait` on an uncompleted token runs
  `step->Run(mSerialExecutor)` inline, so the stall path needs no sleeps and no threads.
- `RecordingStep`: counts `Capture`/`Run`/`Apply` and stores `std::this_thread::get_id()` for each, so
  AC 6 and the "`Run` touched no SimPU state" rule are assertable. Its `SerializePending` /
  `DeserializePending` are stubs until Feature 3.
- `ScriptedKeyDriver`: takes a `StepKey` array and calls `Tick` once per entry.
- Test fixture naming: `DeferredStepSchedulerTest`, so `--filter=DeferredStep*` picks up every test
  in this system.

### Verification cadence
- Per task: `dia run googletest --filter="DeferredStep*" --shards 2` (feedback_scoped_gtest_runs).
- This plan's own close-out is Task 11; the full suite, Release and E2E are the **system** plan's
  Task 6, not here (feedback_verification_cadence).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Module skeleton: folder, `DiaDeferredStep.vcxproj(.filters)`, `dia check sln-sync`, module doc with `dia.module.v1` frontmatter, `GoogleTests` project reference + filter, empty `Testing/`. Record the baseline full-suite pass count for later comparison | A throwaway empty test compiles and runs under `--filter=DeferredStep*` | Done | haiku | Mechanical, TDD-exempt. `dia docs registry` at the end; Skeleton built; skeleton test, sln-sync, deps pass. Placeholder DiaDeferredStep.h/.cpp replaced in Task 1. Full-suite baseline deferred to system close-out |
| 1 | `DeferredStepTypes.h`, `IParallelExecutor.h`, `IDeferredStep.h`, `IStepLauncher.h`, `SerialExecutor.h/.cpp`, `DeferredStepObservation.h/.cpp` (constants only). RED → GREEN on `SerialExecutor` | AC 28: one range, `GetLastRangeCount() == 1`, `count == 0` calls `fn` zero times | Done | sonnet | **Unblocks F2 and F3.** Headers only elsewhere — no scheduler yet; RED (LNK2019) -> GREEN, 6 SerialExecutor tests pass; deps + precommit pass. Headers unblock F2/F3. Skeleton test removed. New files sit in vcxproj root (vcxproj-add lacks anchor on empty project) |
| 2 | `Testing/ManualStepLauncher.h`, `Testing/RecordingStep.h`, `Testing/ScriptedKeyDriver.h` | AC 29–32: launcher completes on demand, `Wait` runs inline, call log order, assert on unknown/double `Release`, `RecordingStep` counts + thread ids | Done | sonnet | Helpers are tested directly before anything depends on them; 15 helper tests pass; deps + precommit pass. Call log caps at 64 entries (silent) |
| 3 | RED → GREEN: `DeferredStepScheduler` construction, `Register`, `Unregister`, destructor drain, `GetState` / `GetChannelCount` | AC 1–4: duplicate id, 9th channel, unregister idle, unregister in flight (waits, discards, warns on a completed result), destructor asserts on a registered channel | Done | sonnet | Establishes `ChannelEntry` and the table. Tasks 4–9 build on this file; 9 tests, RED (missing cpp) -> GREEN; in-flight AC3/AC4 logic written, tests moved to Task 4 (needs Kick). DrainEntry resets state to kIdle: Task 8 DrainAll must not reuse it as-is |
| 4 | RED → GREEN: `Kick` and all five `KickResult` values | AC 5–11: capture-before-launch, `LaunchInfo` contents, double kick, backwards kick key, `applyKey < kickKey`, unknown channel, `applyKey == kickKey` accepted, launch-returns-already-complete | Done | sonnet | `kickSequence` stamped here; 12 tests, RED (no Kick) -> GREEN. Deferred: kKeyNotMonotonic test + AC11 already-complete token (need Tick, Task 5/7); Warning log assertions (Task 9) |
| 5 | RED → GREEN: `Tick` apply pass — due selection, kick order, exactly-once, key jumps, repeated and backwards `Tick` | AC 12–18: not applied early; applied once; 3 channels sharing an apply key apply in kick order; jump of 10; same key twice; backwards `Tick` warns and does nothing; empty table allocation-free | Done | sonnet | The insertion sort by `kickSequence` lands here; 12 tests RED(LNK2019)->GREEN; AC8 + AC11 tests added; backwards-Tick assert tested via g_pAssertFunc under DEBUG. Apply re-entrancy handled defensively, test in Task 10. Stall path is minimal (Wait then Apply), counters in Task 6 |
| 6 | RED → GREEN: the stall path — `Wait` then `Apply`, counters, one warning | AC 19–22: call order via the launcher's log; `stalls` / `lastStallMicros` / `totalStallMicros`; exactly one Warning; no stall when already complete | Done | sonnet | `ManualStepLauncher::Wait` running inline makes this deterministic; 6 tests + 1 extended, RED->GREEN. AC21 log assertions left to Task 9 (only log capture helper is in DiaCellLayers/Testing; Task 9 needs its own or a lifted one) |
| 7 | RED → GREEN: the poll pass — `completionSeenKey`, `lastSlackKeys`, `lastRunMicros` | AC 23–24: completion seen 3 keys early → slack 3; seen on the apply key → 0; a late step → slack 0 **and** a stall; `lastRunMicros` spans kick to first completion | Done | sonnet | One `IsComplete` call per in-flight channel per `Tick`, asserted from the call log; 9 tests RED->GREEN. Slack/run time written at observation. lastRunMicros tests use a 2 ms sleep (clock-dependent, generous bound) |
| 8 | RED → GREEN: `DrainAll` | AC 25–27: no-op when idle (no `Wait`, no log, counters unchanged); drains without applying or releasing; one Info with the count; drain-then-`Tick` applies in kick order with no stall | Not Started | sonnet | Feature 3 depends on "drain does not apply" — call it out in the test name |
| 9 | Observation wiring: the log channel and all AC 33 call sites, trace and profile zones, nothing per frame | AC 33: each Warning/Info fires once on its trigger; a 100-`Tick` idle run writes no records | Not Started | sonnet | Uses `Dia/DiaObservation/Testing/` log fixtures |
| 10 | Exhaustive test pass: gap audit of every AC against a test, every `KickResult`, every `StepState` transition and every early return in `Tick`; edge cases — `StepKey` at `UINT64_MAX`, `applyKey` overflow, `Register` after a `DrainAll`, `Unregister` inside the apply of another channel, 8 channels all due on one key; `/gen-tests` audit on `DeferredStepScheduler.h` | Gap list in notes; new tests green | Not Started | sonnet | No mutation check (feedback_no_mutation_checks) |
| 11 | Feature close-out: `dia run googletest --filter="DeferredStep*" --shards 2`; `dia check sanitizer --filter=DeferredStep*` (token and channel-table use-after-free, especially AC 3 and AC 4); `dia check deps`; `dia docs registry`; observation scan across the five pillars in notes; update the system spec features table | All ACs, quoted pass/fail | Not Started | sonnet | Full suite / Release / E2E are the system plan's Task 6 |
