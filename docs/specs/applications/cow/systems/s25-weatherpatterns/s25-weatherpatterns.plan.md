# Plan: S-2.5 Weather Patterns (system)

**Spec:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md
**Status:** Not Started

System-level tracker. Each row is a feature spec with its own plan; the detailed tasks live there. The three weather systems share one cross-system order: see `a4-weather.plan.md` and `worldstep.plan.md`.

## Build order and parallelism

```
F2 weather checks ─────────────┐   (pure; needs A4 F1, and A4 F5/F6 helpers for distance, storms, droughts)
                               ├──► F3 headless runner ──► (first real verdict on the curated worlds)
F1 stage & world ──────────────┤   (needs A4 F1-F6, World Step F1-F3, F2)
   (mockup first)              ├──► F4 scenarios & walkthrough
                               └──► F5 time-series graphs
```

- **F2 can start early** (pure code over scripted traces) once A4 F1 exists; its distance, storm and drought parts wait for A4 F5/F6's pure helpers.
- **F1 starts with the HTML mockup** (plan Task 1, user sign-off; file `s25-weatherpatterns/mockups/s25-card.html`, amendment 3) and needs A4 F1 to F6 and World Step F1 to F3 built. The world scout is a user taste gate.
- **F3 follows F1 and F2** and A4 F2/F3/F5/F6. Its last task is the proof: the first real run on the curated worlds. A Hard fail there is a finding about the definition **or** A4, not a bug to fix silently.
- **F4 and F5 follow F1.** F4 starts with a mockup extension and a spike on local command dispatch. F5 starts with a spike on a standalone `CellHistoryRecorder`.
- **First full verdict: F2 + F3 on the curated worlds**, which needs no rendering.

## Soft prerequisites outside this system

| Needed | Gates | Owner |
|---|---|---|
| `HistoryDriverModule` (CoW-Shared) | F1 step-back and timeline, F4's live use of history, F5 recording and exports | S-3.5 Feature 1 (build it early if S-2.5's history side is wanted soon) |
| F-3 session hint reader | F1 launch-out | F-3 Feature 2 (seam with a fake until then) |
| S-1 region layer flagged `history.group: true` | F5 region curves | S-1 schema one-liner (ask first) |
| T-1 Feature 7 (Inspector graphs) | Seeing the curves on screen | Proposed, not specced |

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| Additive `SimQueryHost::Execute(name, args)` if no local dispatch exists | DiaDebugServer | **Given 2026-11-16** (only if the F4 spike finds no local dispatch; the spike still runs first) |
| `IDeferredStep::ValidatePending` | DiaDeferredStep (World Step F3) | Given 2026-11-16 |
| None for F1, F2, F3, F5 | — | All used Dia APIs are built |

## Cross-feature rules

- **Verification cadence** (feedback_verification_cadence): scoped `dia run googletest --filter=... --shards 2` per task. The full suite, `SLOW_S25Verdict`, the live-vs-runner agreement test and the DiaAutomation smokes run once, in the system close-out.
- **TDD** for all new public API, RED proven. **No mutation checks.** **Gap audit** closes every feature plan. **Commit after each task**, plan row updated in the same commit.
- **Cell data over pixels** (feedback_cell_data_over_pixels): the verdict is the checks and the report. One screenshot per feature stays a taste check for the user, not a gate.
- **UI rule**: mockup first, user sign-off, then card code (F1 Task 1, F4 Task 1). One mockup file, `s25-weatherpatterns/mockups/s25-card.html`; F4 extends the file F1 creates.
- **Superseding amendments (2026-11-16):** each spec's `Gap-pass amendments` section supersedes contradicting text; the system spec's four are: Feature 3's command names (`cow.s25.run` with `action`, `cow.s25.baseline`) replace the dotted forms, `CheckResult.measured`/`threshold` are `double` with `state` and `reopenSection`, mockups live in `s25-weatherpatterns/mockups/`, and `CheckInputs`/`WorldInfo`/`needsComparison` are Feature 2's.
- **Command registration**: `S25WeatherModule` owns one `S25CommandRegistrar` for all six `cow.s25.*` names (F1 task 17 creates it with two names; F3 task 16 adds four; rolled back together). F4's three `cow.scenario.*` names are registered by `ScenarioModule` and roll back separately.
- **Performance is report-only** in S-2.5 (deviation D-3.1); no task gates on a timing.
- **Model column**: haiku and sonnet only; ask first if opus seems needed (feedback_confirm_opus).
- **Engine boundary**: CoW only, `S25` prefix for feature-local code; shared scenarios code is CoW-Shared. Any Dia edit is flagged and asked about.

## Tasks

Row number = feature number. Recommended build order: **2, 1, 3, 4, 5** (see the diagram above).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Feature 1 — Stage & world.** @stage-and-world.plan.md | Its final task | Not Started | — | Task 1 is the mockup (user sign-off). World scout (seeds 1..200, 60 s, top 5 by smallest margin; user pick fills `s25_worlds.json`) is a user gate. Creates `S25CommandRegistrar`. History parts, including 13-week retention, wait for `HistoryDriverModule` |
| 2 | **Feature 2 — Weather checks.** @weather-checks.plan.md | Its final task | Not Started | — | Pure logic; start early. `Evaluate(CheckInputs)` with `WorldInfo` and `needsComparison`; registry-driven test forces a pass and a fail trace per check |
| 3 | **Feature 3 — Headless runner & report.** @headless-runner.plan.md | Its final task | Not Started | — | Final task is the first real verdict run. Includes `SLOW_S25Verdict` (Pass needs no recorded baseline; `baseline.unrecorded[]` is informational). Adds four names to `S25CommandRegistrar`; P2 runs no checks |
| 4 | **Feature 4 — Scenarios & walkthrough.** @scenarios-and-walkthrough.plan.md | Its final task | Not Started | — | CoW-Shared (S-3 reuses it). Mockup first; local-dispatch spike gated on user sign-off if a Dia edit is needed. 14 scenarios are a content task with your review |
| 5 | **Feature 5 — Time-series graphs.** @time-series-graphs.plan.md | Its final task | Not Started | — | No in-game graphs. Recorder-standalone spike first; drops the headless export if it needs a Dia change |
| 6 | **System close-out.** Full `dia run googletest --shards 2`; `SLOW_S25Verdict` on both curated worlds; `dia check sanitizer`; Release build; the DiaAutomation smokes for the stage, a scenario and the verdict run; `dia check deps`; `dia docs registry`; observation scan across all five pillars reported in notes | All of the above | Not Started | haiku | Run and report only. Then `dia docs spec-done` per feature and for the system. Record the first-run baseline decisions (which Default magnitudes were frozen, any `MagnitudeMisses` sent to the user) |
| 7 | **GDD fold-back list.** Deviations introduced by these feature specs: performance report-only (D-3.1), A4 CPU budgets in place of 16 ms (D-3.2), Test 5 on the natural 65 weeks (D-3.3), Test 7 thresholds (0.5%, r ≥ 0.9), Test 3 timed after release, Test 8's upwind-edge definition, the 34 named checks, synchronous-default with a live deferred switch, no in-game graphs. **Do not edit the GDD** (feedback_gdd_edits_via_gdd_session) | — | Not Started | haiku | Docs only. Send the text to the CoW/GDD session |

## Open items carried from the specs

| Source | Item | When to decide |
|---|---|---|
| F1 ODQ 1 | Build `HistoryDriverModule` before S-2.5 rather than with S-3.5? | Scheduling |
| F1 ODQ 2 | Verdicts at all three sizes would need six curated worlds | Only if wanted |
| F2 ODQ 1 | Hard-check definitions are first guesses (upwind-edge band, ±60° / 4-of-6, 0.01, 0.05); the crest definition is fixed by F2 amendment 3 (2026-11-16) | F3's first real run |
| F2 ODQ 2 | Test 5 needs a 65-week run, separate from the 52-week performance run | F3 plan (already separate) |
| F3 ODQ 1 | If Test 7 fails by design, `SLOW_S25Verdict` fails every run; a known-failures list hides regressions of that check | After the first real run |
| F3 ODQ 2 | Does S-1's blocking generator agree with the stepped one on the same seed? | F3 criterion 12 |
| F4 ODQ 1 | Local command dispatch on `SimQueryHost`; also whether DiaCellLayers has an overlay-query (else overlay restore = stage defaults, F4 amendment 3) | F4 spike (needs your sign-off if a Dia edit) |
| F4 ODQ 3 | `requiredWhen` in the command catalogue for consolidated commands | Raise with T-2 if wanted |
| F5 ODQ 1 | Nobody can see the curves on screen until T-1 F7 exists | Scheduling |

Items previously open that the 2026-11-16 gap-pass amendments resolved: the mockup path (F1 amendment 1), scout budget (F1 amendment 2), two-trace checks and `WorldInfo` (F2 amendments 1-2), first-run `Pass` (F2 amendment 5, F3 amendment 1), cross-feature command registration (F3 amendment 2), command names (F3 amendment 6). Each is recorded as 'Resolved by amendment N (2026-11-16)' in the owning feature plan.

## Open-items review (2026-11-16)

All accepted by the user on 2026-11-16. These supersede the matching "Open items" lines in the feature plans.

| # | Decision |
|---|---|
| 1 | **Build `HistoryDriverModule` early** (S-3.5 F1, scheduled between World Step F3 and S-2.5 F1) so the history-gated tasks in F1, F4 and F5 are not left on fakes |
| 2 | `SLOW_S25Verdict` ships **strict**, with no known-failures list. If Test 7 fails by design, the first real run decides between adding a forcing, accepting periodic weather, or dropping the test (a GDD decision) |
| 4 | T-1 Feature 7 (the Inspector graph panel, proposed) is to be specced **after S-2.5 F3**, since F3 produces the data. Not blocking: verdicts are numeric |
| 6 | Verdicts at Small only; one Large performance run |
| 7 | `cow.scenario.run` pauses the clock by default; revisit with S-3 F3 |
| 8 | `requiredWhen` in the command catalogue is deferred; raise with T-2 only if authors hit run-time missing-argument errors |
| 14 | A check with all-NaN data: Hard checks `Fail` with "non-finite data", Default checks `MagnitudeMiss`; `Pending` and `InvalidWorld` carry an empty `reopenSection` |
| 16 | `cow.s25.status` fields: world, prechecks with values, step mode, day/week/season, history availability, hint source, last performance values |
| 18 | The DiaAutomation scenario location and format follow F-2's pattern, found in `scenarios-and-walkthrough` Task 0 |
| C | Settled by running: hard-check definitions (S-2.5 F3 proof run), S-1 blocking vs stepped generator agreement (F3 AC 12) |
