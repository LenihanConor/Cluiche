# Feature Spec: T-1 S-2.5 Checks Panel

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md (Feature 9, amendment 2026-11-16)
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/time-series-and-events.md (Feature 2), @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md (Feature 6)
**Reads:** @docs/specs/applications/cow/systems/s25-weatherpatterns/headless-runner.md (S-2.5 Feature 3, `cow.s25_report` v1), @docs/specs/applications/cow/systems/s25-weatherpatterns/weather-checks.md (S-2.5 Feature 2, check model and `baseline.json`), @docs/specs/applications/cow/systems/s25-weatherpatterns/time-series-graphs.md (S-2.5 Feature 5, history file per run)

**Gameplay Domains:** debug

## Summary

A T-1 view for S-2.5's weather verdict. It opens the S-2.5 report files and shows:
- the overall verdict and the rule that produced it;
- each curated world's prechecks and checks, with the measured value against its threshold or ±10% baseline band;
- informational blocks: performance, edge-case coverage, live smoke and baseline state;
- each check's trend across archived reports;
- a link from every check to the history file of the run that fed it, which opens in T-1's time series.

While an S-2.5 stage is running, it shows run progress read-only and opens the new report when it lands. It never starts runs and never records baselines.

## Problem

S-2.5's verdict is a JSON file. A failing or missed check is read by hand: find the check, find its threshold, work out which run fed it, find that run's history file, open it. Nothing shows whether a check is getting worse across commits. S-2.5 has no in-game graphs, so T-1 is where this belongs.

## Goals

1. **Verdict at a glance:** the verdict, its precedence reason and the counts per state. `MagnitudeMisses` is shown as "needs your decision", not as a failure (S-2.5: it goes to the user).
2. **Every check explained:** test, id, kind (Hard or Default), state, measured, threshold, `detail`; `reopenSection` on a Hard fail; for Default checks with a baseline, the ±10% band drawn around the frozen value.
3. **One click to the curve:** each check lists its runs (from the report), and opening one loads `history_<run>_<world>.json` into the time series as Source A (or B in compare).
4. **Trend:** each check's measured value and state across archived reports for the same world, seed and size, with git revision labels.
5. **Context blocks:** performance (P1/P2 against budgets, build, cores, worker cap), edge-case coverage (EC-01 to EC-20), live smoke, baseline state (recorded vs missed, frozen values).
6. **Live, read-only:** progress from `cow.s25.report {"what":"status"}` while a run is active; the new report opens when it completes.
7. **No control:** the start-run and record-baseline commands are shown as copyable console text, never sent.

## Non-Goals

- Starting, cancelling or configuring runs (`cow.s25.run`, `cow.s25.livesmoke`). That is sim control, which the Inspector tier rules out.
- Recording or editing baselines (`cow.s25.baseline`, `baseline.json`). That is an authored, committed file.
- Recomputing any check. The report is the source of truth.
- Other testbeds' reports (S-3, S-3.5 R1-R6). The model should make adding them cheap, but they are out of scope here.

## Architecture

```
model/s25Report.ts   (pure TS)
  parseReport(json) -> S25Report | { error: 'format' | 'version' | 'malformed', reason }
  verdictReason(report) -> text          from the S-2.5 precedence rule
  groupChecks(report) -> per test, ordered by test then id
  bandFor(check, baseline) -> { lo, hi } | null       Default checks only, ±10% relative
  runLinks(check, report, dir) -> [{ runId, path, exists }]
model/s25Trend.ts    (pure TS)
  trendKey(report) = world.id + seed + size
  splitByRevision(series)               a world.revision change starts a new segment
  trend(reports, checkId) -> points { utc, gitRevision, measured, state }
model/s25Live.ts     (pure TS)
  parseStatus(reply) -> { running, phase, runId, week, totalWeeks, fraction, elapsedMs } | idle
store/   s25 { reports, selected, archive, live }
views/   VerdictHeader, WorldTabs, PrecheckList, ChecksTable, CheckBand, TrendSparkline,
         PerformanceBlock, EdgeCaseTable, LiveSmokeBlock, BaselineBlock, LiveProgress, CopyCommand
bridge (C++ CoWWorldSimInspectorPlugin)
  listS25Reports() -> latest + archive entries      restricted to out/CoW/s25/
  readS25File(relPath) -> text                      same restriction
```

- This is a **report source**, separate from Feature 1's history `Source`. The two file types are not forced into one model. A check's run link opens a history `Source` through Feature 1's existing `openHistoryFile`.
- Views call only `model/` and the store (UI/business separation rule).

### Files read

| File | Envelope | Use |
|---|---|---|
| `out/CoW/s25/report_summary.json` | `cow.s25_report` v1 | Overall verdict |
| `out/CoW/s25/report_<world>_<seed>.json` | `cow.s25_report` v1 | Per-world detail |
| `out/CoW/s25/archive/report_<world>_<seed>_<utc>_<gitrev>.json` | `cow.s25_report` v1 | Trend (**S-2.5 amendment B**) |
| `out/CoW/s25/history_<run>_<world>.json` | `dia.cell_history` | Check curve (S-2.5 Feature 5) |
| `out/CoW/s25/livesmoke.json` | `cow.s25_livesmoke` v1 (**S-2.5 amendment C**) | Live smoke block |
| `Stages/S25WeatherPatterns/baseline.json` (deployed copy) | `cow.s25_baseline` v1 | Frozen values and bands |

- An unknown `format` or `version` is refused with the reason. Unknown keys are ignored.
- A report without `checks[].runs` (written before amendment A) still opens. Its run links are disabled with "report predates run links".

### Requested S-2.5 amendments (S-2.5 Feature 3, CoW-side, no AD-001)

- **A. Runs per check.** Each `checks[]` entry gains `runs: ["R1", ...]`, the run ids that fed it (from the run table: T1 → R1, T2 → R5a + R5b, T3 → R4, and so on). Additive; the report stays version 1.
- **B. Report archive.** On every completed (not cancelled) run, S-2.5 also writes `out/CoW/s25/archive/report_<world>_<seed>_<utcCompact>_<gitrev7>.json` (atomic, same rule as the main report) and keeps the newest 50 per world and seed. The `SLOW_` test writes there too.
- **C. Live smoke envelope.** `livesmoke.json` gets the envelope `cow.s25_livesmoke` version 1 (it has none today).

### Verdict and checks

- The verdict header states the precedence result in words, for example "MagnitudeMisses: 2 Default checks outside ±10% of baseline. Goes to you: adjust the threshold or reopen A4."
- **Colours:** `Pass` green; `Fail` red; `MagnitudeMiss` amber; `Pending` grey; `InvalidWorld` purple ("re-pick the seed"). The verdict uses the same colours. Performance never changes the verdict colour (S-2.5 decision).
- Checks are grouped by test (T1-T9) inside each world tab. A test with any Hard fail starts expanded.
- **Band:** a Default check with a baseline entry shows `[0.9 × value, 1.1 × value]` (for a negative value, the bounds are ordered), with the measured value marked. Without a baseline, it shows the GDD threshold and "no baseline yet".
- `thresholdsOnly` baseline entries show the threshold only, with no band.

### Trend

- Archive reports are grouped by `trendKey`. A trend is drawn only within one key. A `world.revision` change starts a new segment with a marker ("world changed"), so values are not compared across different terrains.
- Per check: a sparkline of `measured` (state-coloured points), and a strip of states for checks without a magnitude. Hovering a point shows utc, git revision, value and state; clicking it opens that archived report.

### Live

- Only when connected and the stage registers `cow.s25.report` (the panel checks the registered names). Otherwise it shows "no S-2.5 stage running".
- While `running`, poll `{"what":"status"}` once a second and show phase, run id, week of total and elapsed. On idle after a run, fetch `{"what":"latest"}` and open the reported file.
- `cow.s25.report` is added to Feature 6's bridge allow-list as a **query**. No command is added.

### Copyable commands

The panel shows `cow.s25.run {"action":"start","world":"both"}` and `cow.s25.baseline {"action":"record"}` as text with a Copy button and a note "run in the game console". Nothing is sent.

### Logging and observation

Channel `[EDITOR] T1`. Logged: reports opened (path, verdict), refused files (reason), archive scan (count, keys), live run start and finish seen, run-link opens, and missing history files.

## Acceptance Criteria

1. `report_summary.json` and each `report_<world>_<seed>.json` open. A wrong `format`, an unknown `version` or malformed JSON is refused with the reason, and unknown keys are ignored.
2. The verdict header shows the verdict, the precedence reason in words and counts per state. `MagnitudeMisses` reads "needs your decision" in amber, not as a failure.
3. Performance `Over` never changes the verdict display. The performance block shows P1 and P2 against their budgets as `Within`, `Over` or `NotMeasured (Debug)`, with build, cores and worker cap.
4. Each world tab lists its prechecks (state, value, margin), and an invalid world says "re-pick the seed".
5. Checks are grouped by test. Each shows kind, state, measured, threshold and detail, and a Hard fail shows its `reopenSection`.
6. A Default check with a baseline shows the ±10% band around the frozen value with the measured value marked. One without a baseline shows the threshold and "no baseline yet". A `thresholdsOnly` entry shows no band.
7. A check lists its runs from `checks[].runs`. Opening one loads `history_<run>_<world>.json` as Source A (or B). A missing file is shown disabled with its path. A report without `runs` disables links with "report predates run links".
8. The trend for a check uses only archive reports with the same world, seed and size, splits at a `world.revision` change, and opens an archived report on click.
9. The edge-case table lists all 20 entries, covered (with run ids) or untested (with the reason).
10. The live smoke block shows max step against the Small budget, wall time and stalls from a `cow.s25_livesmoke` v1 file.
11. The baseline block shows recorded, missed and unrecorded Default checks (the report's `baseline.recorded[]`, `missed[]` and, per S-2.5 gap-pass amendment 1, `unrecorded[]`) and the frozen values from `baseline.json`. An unrecorded check is judged against the GDD default and does not block `Pass`.
12. With a connected S-2.5 stage, live progress updates at least once a second during a run, and the new report opens when the run completes. Without the stage, the panel says so and shows no error.
13. No `cow.s25.run`, `cow.s25.livesmoke` or `cow.s25.baseline` call can be sent. The bridge allow-list holds only the `cow.s25.report` query, and the copyable text is never sent.
14. The bridge reads files only under `out/CoW/s25/` and the deployed baseline. Any other path, including `..`, is refused.

## Test Strategy

- **Vitest (model/):**
  - parse all states and verdicts; refusals; a report without `runs`
  - precedence reason text for each verdict
  - band edges: negative values, `thresholdsOnly`, no baseline
  - run-link resolution, including a missing file
  - trend grouping and revision splits
  - live status parsing
- **React Testing Library:** each verdict from fixtures; check table states and colours; band render; trend sparkline; the four context blocks; live progress; copy-only commands.
- **GoogleTests (C++):** `listS25Reports` and `readS25File` path restriction (accepts the folder, refuses `..`, absolute and other folders); archive listing order.
- **E2E:** a stub stage registering `cow.s25.report` returns running status, then idle plus a latest path, and the panel opens that fixture report. Clicking a check's run opens its fixture history file in the time series.
- **Fixtures** are written to the real S-2.5 shapes, one report per verdict. When S-2.5 Feature 3 is built, its `SLOW_` output replaces them as a contract test.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code in `Cluiche/CoW/Editor/WorldSimInspector/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Tool tiers (Inspector) and T-1 non-goal "Running the simulation" | Inspector reads, never controls | Read-only `cow.s25.report` query; runs and baselines are copyable text only (AC 13) |
| T-1 Feature 6 AC 11 | Only listed names cross the bridge | Allow-list amendment: add `cow.s25.report` (query) |
| Editor actions via DiaAPI | Actions are DiaAPI commands | The only live call is the existing sim-hosted `cow.s25.report` |
| UI/business separation | Typed model first | `s25Report.ts`, `s25Trend.ts`, `s25Live.ts` |
| S-2.5 Feature 3: performance never changes the verdict | Report-only perf | Performance block is separate and never colours the verdict (AC 3) |
| S-2.5 Feature 3 / GDD AD-06: MagnitudeMisses go to the user | Not an automatic fail | Shown amber as "needs your decision" (AC 2) |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| T-1 Features 1, 2, 6 | Shell, history source and time series, bridge and allow-list | Approved, not built |
| S-2.5 Feature 2 (check model, `baseline.json`) | Check fields, bands | Approved, not built |
| S-2.5 Feature 3 (`cow.s25_report` v1, `cow.s25.report`) | Reports and live status | Approved, not built; **amendments A, B, C applied 2026-11-16** |
| S-2.5 Feature 5 (history file per run) | Check curves | Approved, not built |

Built against fixtures first; the contract test switches to real output when S-2.5 Feature 3 is built.

**Amendments when Approved:**
- **S-2.5 `headless-runner.md`:** A (`checks[].runs`), B (archive, newest 50 per world and seed), C (`cow.s25_livesmoke` v1 envelope).
- **T-1 Feature 6:** allow-list gains `cow.s25.report` (query).

## Open Design Questions

1. **Which layers does a check's curve show?** Opening `history_R4_s25_coast_range.json` loads the run, but the report doesn't say that T3 is about `weather.pressure` and wind. v1 opens the file with the built-in weather view (Feature 8). A `layers[]` per check would focus it. That's a small further S-2.5 amendment, worth adding if the generic view proves unhelpful.
2. **Archive size and CI.** 50 reports per world and seed is a guess. Reports are small (a few KB), so the limit is about clutter, not disk. If CI runs S-2.5, the archive lives on the CI machine. Whether CI publishes its archive somewhere T-1 can open is out of scope here.
3. **Generalising to other testbeds.** S-3 and S-3.5 will produce similar verdict reports. If they share a common envelope (verdict, checks with kind, state, measured, threshold and runs), this panel becomes a generic "testbed checks" view. Decide when S-3's report is specced, not now.

## Status

`Approved` (2026-11-16). S-2.5 amendments A-C applied to `headless-runner.md` and the Feature 6 allow-list amended the same day. No plan. The views are drawn in the new world-sim mockup (owed, see the parent amendment) before UI work.
