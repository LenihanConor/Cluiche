# Plan: DiaDeferredStep (system)

**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md
**Status:** Done

System-level tracker. Each row is a feature spec with its own plan; the detailed tasks live there.
A feature is Done only when its own plan's final task is Done.

## Build order and parallelism

```
F1 scheduler core ──┬──► F3 save / load ──┐
                    │                      ├──► F4 module + observability ──► F5 attribution + summaries
F2 capped executor ─┴──────────────────────┘
```

- **F1 and F2 are independent** (F2 only needs F1's headers, which land in F1 Task 1) and can run in
  parallel once F1 Task 1 is Done. F3 needs F1's scheduler but not F2 — it tests against the manual
  launcher.
- **F4 needs F1–F3** because it owns and wires all three.
- **F5 Part B's last task is the only externally blocked work** (perf-inspector Task 8,
  `JobTimingTable`). F5 Part A and the summary builder are not blocked.

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| New module `DiaDeferredStep` (`IParallelExecutor` into Dia) | Dia | Given 2026-11-12 |
| `JobSystem::TrySubmit(JobFn, const JobTag&)` overload | DiaThreading | Given 2026-11-13 |
| `Dia::Observation::Profile::StepSummaryRecord` | DiaObservation | Given 2026-11-13 |

No further sign-off is needed — every other new file is inside `Dia/DiaDeferredStep/`, plus one
CluicheTest stage and one `.diaapp` entry.

## Cross-feature rules

- **Verification cadence** (feedback_verification_cadence): each feature plan runs scoped
  `dia run googletest --filter=... --shards 2` per task (narrow filter on the suite under work) and closes with a feature-wide scoped run. Sanitizer: on request only (user, 2026-11-17). Long tests are tagged `SLOW_` and run once in the feature close-out and once in Task 6. Perf is Debug ballpark only; a bad ballpark queues a perf pass. The **full** suite, one Release build and cross-feature E2E run once, in Task 6 of this plan — not per feature. This system lands first and the CoW stack depends on it, so it keeps its own full-suite and Release build (2026-11-17, speed-ups).
- **TDD** (`.claude/skills/tdd.md`) applies to every feature: all five are new public API. RED proven
  with quoted output before GREEN. Mechanical tasks (vcxproj, module doc, registry) are exempt.
- **No mutation checks** (feedback_no_mutation_checks, 2026-11-12).
- **Gap audit** closes every feature plan (feedback_exhaustive_test_pass).
- **Commit after each task** in the feature plans, with the plan row updated in the same commit.
- **Model column**: haiku and sonnet only. No task here is assigned opus; if one of the flagged
  tasks (F2 Task 3, F2 Task 4) turns out to need it, ask first (feedback_confirm_opus).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Feature 1 — Scheduler core.** @scheduler-core.plan.md | Its Task 11 | Done | — | Tasks 0–11. Pure logic, no threads. Unblocks F2 and F3 after its Task 1; Feature 1 Done 2026-11-17: 61+60 scoped tests, ASan clean (121). Full suite/Release/E2E still owed at system Task 6 |
| 2 | **Feature 2 — Capped executor.** @capped-executor.plan.md | Its Task 9 | Not Started | — | Tasks 0–9. The only real concurrency in the system; F2 Tasks 3–4 are the hardest work in it |
| 3 | **Feature 3 — Save / load.** @save-load.plan.md | Its Task 8 | Done | — | Tasks 0–8. Can run in parallel with F2 — different files, no shared headers beyond F1's; Feature 3 Done: 87 scoped tests green; ODQ1 size: A4 Large pending 0.92 MB numbered keys / 0.2 MB hex string |
| 4 | **Feature 4 — Module + observability.** @module-and-observability.plan.md | Its Task 11 | Done | — | Tasks 0–11, including the CluicheTest `DeferredStep` stage and its E2E |
| 5 | **Feature 5 — Job attribution + step summaries.** @job-attribution.plan.md | Its Task 8 | Done | — | Tasks 1–8. Done 2026-11-18; JobTimingTable wired via the existing hook (summaries do not yet reach the published chunk: backlog); everything before it is not |
| 6 | **System close-out.** Full `dia run googletest --shards 2`; the `SLOW_` tests once (AC 10 starvation test and any other `SLOW_`); **one Release build** of GoogleTests and CluicheTest covering the Release checks moved from feature plans (save-load AC 26 double attach/detach no-op, job-attribution AC 23 summaries on vs off soak of 1000 steps, reported for information only, and any Release-only asserts); no sanitizer (on request only, user, 2026-11-17); the cross-feature E2E (stage at k = 7 and k = 1, with a save/load mid-step); `dia check deps`; `dia check spec-sync`; `dia docs registry`; observation scan across all five pillars reported in notes; `dia docs spec-done` on the system spec + each feature spec; BACKLOG row updated | Everything, quoted | Done | sonnet | Kill leftover `CluicheTest.exe` first (feedback_dia_run_gotchas). The E2E harness does not build — build before running; Close-out 2026-11-18. Debug full 8096 pass/0 fail (only prior StreamWriteBenchmark failures fixed). 'dia run googletest --all' DEFAULTS TO RELEASE: 59-125 failures there are Release-only (death tests cannot die, plus ThreadPool.LoadBalancing, DiaSensorTest, DiaRules_ActionRegistry, FileDiscoverer, RegionRepair, SelectionInspector perf) none in DiaDeferredStep/DiaObservation/ApplicationFlow code from this work (backlog). Release GoogleTests 51 failures same class; all DeferredStep Release checks pass incl. save-load AC26 attach/detach no-op, backwards-key Release branch, soak on=50-55 off=47-55 us/step. Release CluicheTest link FIXED (ResolveUnitForThread was _DEBUG-only since 60751074). E2E stage k=7 passed, k=1 stalls=20 health=degraded passed. Save/load mid-step E2E NOT built: no SaveManager in CluicheTest (4-5 files); covered by 25+ GoogleTests (TestDeferredStepRoundTrip/Save/SaveEdge). deps OK; spec-sync no missing symbols for DeferredStep. |
| 7 | **Hand off the CoW follow-ups.** Write the backlog rows / spec amendment notes for: A4+A1+A3 as one `world` channel, the game clock implementing `IStepKeySource`, retiring `JobSystemPlaceholderConsumerModule`, S-3.5's spike check becoming a SimPU-frame gate, and choosing k per speed preset. Also the two gaps this system found: `SaveContext` array-element + `uint64_t` writers, and `JobObserver` dropping worker-submitted records | — | Done | haiku | Docs only. No CoW code in this system (engine/application boundary); CoW rows already in BACKLOG (919c51cb); added SaveContext writer gap row; JobObserver + chunk gaps in F5 follow-ups row |

## Open items carried from the specs

These are recorded so they are not lost; none blocks a task.

| Source | Item | When to decide |
|---|---|---|
| System ODQ 1 | Pending-result serialization cost once A1's working copy is sized | F3 Task 7, with real sizes |
| System ODQ 2 | One `world` channel or several (the Century step makes one week's `Run` very long) | CoW's call, after F4's stage gives real numbers |
| System ODQ 3 | Choosing k, possibly per speed preset | CoW, task 7 above |
| F2 ODQ 1 | `rangesPerWorker` scaled by M or by worker count | Measure with CoW's real step |
| F2 ODQ 3 | Whether a purely-waiting coordinator should release its slot | After cap-utilisation measurements |
| F4 ODQ 1 | Nothing enforces the two manifest ordering rules — wants a `dia validate manifest` rule | Backlog row in task 7 |
| F4 ODQ 2 | Health window in seconds vs keys | Revisit if CoW at Slow speed makes 60 s meaningless |
| F5 ODQ 1 | `JobObserver` drops worker-submitted job records | Backlog row in task 7; does not block F5 |
