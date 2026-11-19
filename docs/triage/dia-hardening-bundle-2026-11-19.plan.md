# Dia hardening bundle plan (2026-11-19)

**Status:** Not Started
**Scope:** 6 backlog Task rows from the 2026-11-19 loose-ends triage, fixed together, then moved to BACKLOG-HISTORY.md via `dia docs backlog move`: DiaCore `String<N>` audit, GoogleTests Release failures, AI Inspector UI, AssetRuntime Inspector UI type error, E2E scenario health pass, AutomationService checkpoint runner.
**Rules:** scoped `--filter` while iterating, `--shards >= 2`, one full `dia run googletest` at the end. No sanitizer, no mutation checks, no Release benchmarking. Commit after each task, staging only that task's files. Models are sonnet/haiku only (no opus without asking).
**Precedent:** [cleanup-bundle-2026-11-19.plan.md](cleanup-bundle-2026-11-19.plan.md) (same shape, all Done).

## Why this order (optimisation)

Three lanes, split by what each one contends for:

| Lane | Tasks | Contends for | Runs |
|---|---|---|---|
| **UI** | 1, 2 | node/vite only, no C++ build, no game exe | Parallel with Lane A from the start (separate subagent) |
| **A: core C++** | 3, 4, 5 | MSBuild outputs, vcxproj, Debug + Release builds | Serial, one build at a time |
| **B: automation** | 6, 7 | game exes, ports, SimPU timing | After Lane A is committed, so E2E measures a stable tree. Nothing else may run builds or games during it |

Ordering within lanes:
- **String<N> before Release failures.** The audit may change `String` behaviour that tests assert on, so the Release triage should see the final code.
- **Release-gating (mechanical, ~45 of 51) before the 6 real failures.** The gating shrinks the failure list to the ones that need thought.
- **E2E health before the checkpoint runner.** The runner's acceptance is a full E2E run, which needs a trustworthy baseline. Task 6 also adds the log capture that makes Task 7 debuggable.
- **UI lane first in wall-clock terms.** It is small and unblocks `dia pipeline` for editor targets.

## Sign-offs and checks first (before any dispatch)

| # | Item | Why |
|---|---|---|
| S1 | **AD-001: DiaCore `String<N>` change.** Ask whether DiaCore itself may change (e.g. make `operator=(const char*)` non-formatting) or only call sites move to a bounded `SetText`-style assign. | Task 3 step 3 branches on this. Default if not approved: call sites only. |
| S2 | **AD-001: DiaAutomation `CheckpointSnapshot` type, Sim-side runner module, Main-side serving.** | Task 7 adds new headers to a Dia project. |
| S3 | Working tree has uncommitted edits (`GridSearch.inl`, `grid-search-helpers.md`, `.claude/settings.local.json`). Do not touch or stage them. | Avoid mixing into bundle commits. |
| S4 | Kill any leftover `CluicheTest.exe` before Lane B; `dia test e2e` does not build, so run `dia run`/pipeline first. | Known gotchas (see memory: dia run / E2E gotchas). |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **AssetRuntime Inspector UI type error.** Fix `StateTransitionLog.test.tsx(288,22)` TS2774 (a function is tested instead of called: call it, or assert on the result). Switch the `pipeline.toml` entry for AssetRuntime back from `vite build` to `tsc && vite build` (match Economy and Blackboard). | `npm run build` in the AssetRuntime UI folder passes; `npx vitest run` for that file passes; `dia pipeline --target <editor>` deploys it | Not Started | haiku | Lane UI. If more `tsc` errors surface once the first is fixed, fix them here (small) or stop and report |
| 2 | ~~**AI Inspector UI.** Build `DiaAIInspector/UI/` (package.json, vite, tsconfig, `index.html`) modelled on the Economy/Blackboard inspector UIs. Panels for the four payloads the controllers already push: `ai_inspector.budget`, `.utility_ai`, `.rules`, `.htn`. Add its `ui_builds` and `files` entries to `pipeline.toml`; remove `aiinspector` from `KNOWN_MISSING_UI` in `Dia/DiaCLI/tests/test_plugin_ui_deploy.py`. UI calls DiaAPI/bridge commands only. | Vitest per panel against fixture payloads; `pytest Dia/DiaCLI/tests/test_plugin_ui_deploy.py`; `dia pipeline` deploys `dia://plugins/aiinspector/index.html` and it is non-blank | **Dropped 2026-11-19** | sonnet | User decision: AI Inspector is game-team specific, not a Dia deliverable; row removed from BACKLOG, `aiinspector` stays in `KNOWN_MISSING_UI`. Original note: **Visual gate:** the HTML mockup rule applies to editor features. Produce one mockup (4 tabs) first and **stop for user sign-off before building**. Read an existing inspector UI and the 4 controllers' payload shapes before dispatch |
| 3 | **`String<N>` audit.** (a) Grep `= .*c_str()` and `mX = cstr` assignments and `String<N>(cstr)` constructions on sized `String*` across Dia and Cluiche; list each with whether the source can contain `%`. (b) Add a failing test per class of bug (a `%` in a path/key survives round trip; no crash). (c) Convert unsafe call sites to the bounded `SetText`-style assign (DiaValueSchema already has one). (d) Per S1, either fix DiaCore's sized-string assign/constructor or leave it and record the decision in `dia.core.string.architecture.module.md`. (e) `DynamicArray` copy constructor: stop asserting on an empty source, with a test. | `--filter="*String*:*DynamicArray*:*ValueSchema*"` (TDD: RED shown first, per `.claude/skills/tdd.md`) | Not Started | sonnet | Lane A. Split commits: 3a DiaCore change (if signed off), 3b call-site conversions (grouped per module), 3c DynamicArray. Keep each module's conversion small enough to build-check scoped. Report the audit list in the task notes. Gaps follow the three-way rule |
| 4 | **Release gating (mechanical).** Run `dia run googletest --all` (Release) once to get the real list; capture failing names to a file. Gate the death/assert tests that cannot die in Release with `#ifdef _DEBUG` (or a shared `DIA_DEATH_TEST` helper if one already exists; do not invent a new one if it does): `Array.*`, ArrayC/DynamicArrayC/HashTable/LinkListC bounds, FixedDrawLayer, DiaRig2D/DiaIK2D boundary, CallbackRegistry, SquarePathGrid/FlowField boundary. | Re-run the same Release list: every gated test disappears from the failures, none newly skipped without a gate comment | Not Started | sonnet | Lane A. Debug runs of the same tests must still run and pass (gating must not hide them in Debug). Do not touch the 6 real-failure tests here |
| 5 | **Release real failures (6).** One hypothesis, one change each (`.claude/skills/debug.md`; three failed fixes on one root cause = stop and report): `DiaSensorTest.DamageSensor_PrunesDamageEvents_AfterWindow` and `Integration_DamagePruneWindow_EndToEnd`; `DiaRules_ActionRegistry.Register_Duplicate_OverwritesHandler`; `ThreadPool.LoadBalancing_DistributesWorkAcrossThreads`; `FileDiscovererScanTest.DiscoversFindsPngAndConfigFiles`; `SelectionInspectorPerfTest.AggregateAllCellsOverTwentyLayers` (2.06 ms vs 2 ms). | Each test run alone in Release, then in `--all` | Not Started | sonnet | Lane A. Classify each first: **real bug** (fix), **test assumption** (fix the test), **perf ceiling** (per quick-and-dirty rule: ballpark-OK means raise the ceiling and say so, no Release benchmarking). The thread-pool load-balance test may be timing-flaky: repeat 20 times before and after. If a fix needs a Dia engine behaviour change, flag it before editing |
| 6 | **E2E scenario health pass.** Step 1: make E2E failures self-explaining: capture the game's stdout and log path in the failure output (CLI/pytest harness change), so every later step is diagnosable. Step 2: make `dia test e2e` build first (or fail with a clear "stale exe" message). Step 3: fix `test_discover_stages` timing out the full run. Step 4: `geometry2d` first-entry pending-checkpoint timeout. Step 5: `mesh3d_render_system`, `scalarfield`, `entityspatial`, `assetruntime` failing alone, one at a time. Step 6: CellLayers stage re-entry flake (PU stall after a transition). Step 7: add a minimal CoW E2E pytest scenario (S-1 launch, replacing the throwaway script). Re-measure each flake over N=10 runs on a clean tree before and after. | Per scenario: pytest green 10/10; one full `dia test e2e` run at the end | Not Started | sonnet | Lane B, serial, no other builds/games running. Commit per step. Steps 4-6 each get their own root-cause note; if one resists three fixes, stop, report, and promote it as its own row rather than blocking the bundle. Read the session log, not the summary line |
| 7 | **AutomationService checkpoint runner.** Per the backlog design: (a) spec amendment + S2 sign-off; (b) `CheckpointSnapshot` type and a Sim-side runner module that evaluates Sim-owned checkpoints each frame and publishes results over a FrameStream snapshot (CellLayers state-relay shape); (c) Main-side `RunCheckpoint` serves the latest snapshot, with explicit missing and stale results; registration sites unchanged; (d) thread-identity test proving Sim-owned checkpoint lambdas execute on SimPU, not Main; (e) migrate the ~140 SimPU checkpoint registrations only if they need a registration-time tag (otherwise untouched). | TDD: thread-identity RED first; `--filter="*Automation*:*Checkpoint*"`; then one full `dia test e2e` | Not Started | sonnet | Lane B, after Task 6. Respect PU-boundary-streams rule (no mutex cross-PU reads). Note the ~1 frame of lag in the spec. Observation scan on the runner (silent stale/missing path needs a `DIA_LOG_*`; runner per-frame cost needs a `DIA_PROFILE_SCOPE`) |
| 8 | **Close-out.** One full `dia run googletest --shards 2` (Debug); one `--all` Release pass confirming Tasks 4-5 (the only Release run in the bundle); `dia check deps`; `dia validate manifest`; `dia docs precommit`; observation opportunity scan on touched files (High implemented as `chore(<Module>): observation instrumentation`, Low noted here). | Full suite pass; Release list empty or only items promoted as new rows | Not Started | haiku | Report pass/fail and failing names only |
| 9 | **Backlog.** `dia docs backlog move` for the 6 rows; add any promoted rows (each with a deliverable); update the Dia hardening line in BACKLOG.md. Commit, staging hunks only (S3). | `dia docs precommit` | Not Started | haiku | Gap handling below |

## Execution

```
t0  ┬─ Lane UI (subagent, parallel):  1 ─► 2 (mockup gate, then build)
    └─ Lane A (serial):               3 ─► 4 ─► 5
                                       │
t1  (Lane A committed) ────────────────┴─► Lane B (serial, exclusive):  6 ─► 7
t2  8 ─► 9
```

- Dispatch Task 1 and the Task 2 mockup alongside Task 3. Everything inside a lane is sequential because every C++ task touches vcxproj or shared build output.
- Each dispatch inlines its context (the row text, file paths, constraints), per `.claude/skills/dispatch.md`. Two-stage check after each DONE: spec compliance, then code quality. Fresh quoted test output required (`.claude/skills/verify.md`).
- Estimated size: Lane UI about 1 day (mostly Task 2), Lane A about 1.5 days (Task 3 audit breadth is the variable), Lane B about 2-3 days (Task 6 is open-ended, Task 7 is 1-2 days).

## Risks

| Risk | Mitigation |
|---|---|
| Task 3 audit is wider than expected (hundreds of `= c_str()` sites) | Fix only sites that can receive user/path text; list the rest in the notes; if S1 allows a DiaCore fix, that closes the class and call-site conversion becomes optional |
| Task 6 turns into a swamp | Time-box per step; a scenario that resists three fixes becomes its own backlog row |
| Release failure fixes touch behaviour | Classify first; engine behaviour changes are flagged, not made silently |
| Task 2 UI scope creep | Mockup sign-off is the gate; the four panels only render pushed payloads |

## Gap handling

Anything found during a task ends as: fix now (small), promote to a backlog row with a deliverable, or drop. No Loose Ends rows.
