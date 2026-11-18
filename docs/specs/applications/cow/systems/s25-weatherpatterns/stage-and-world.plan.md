# Plan: S-2.5 Stage & World (S-2.5 Feature 1)

> **Re-plan required (2026-11-18, world-sim review):** tasks below were written before the amendment in stage-and-world.md and are stale where they touch A4 command names, health levels. Re-plan before /implement.

**Spec:** @docs/specs/applications/cow/systems/s25-weatherpatterns/stage-and-world.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.plan.md (row 1)

**Start conditions.** Needs A4 Features 1-6 built (the module, the debug surface, `ComputeWaterDistance`,
`PrevailingDirection`) and World Step Features 1-3 built, plus S-1 Features 1 and 8, DiaCellLayers,
DiaCellHistory, GameSpeedController, F-1 and DiaAutomation (all built). **Soft:** CoW-Shared
`HistoryDriverModule` (S-3.5 Feature 1) and F-3's session hint reader; every task that needs them is
written against a fake first and the real integration rows are marked blocked.

**Mockup gate.** Task 1 (the HTML mockup) is first and the **user sign-off on it is a hard gate before
any card code** (tasks 16 and 19). Non-card tasks (table loader, prechecks, module, commands) may proceed
while the mockup is being reviewed, because they have no UI. Do not start task 16 or 19 until the user has
signed off the mockup in this conversation.

**Cross-plan dependencies.**
- Task 10 edits `WorldStepModule` (World Step Feature 2's code, CoW-Shared). It is owned by that
  feature's code; coordinate with the World Step Feature 2 plan so the two do not both edit
  `WorldStepModule.cpp` in parallel.
- Tasks 14 and 26 depend on S-3.5 Feature 1 `HistoryDriverModule` for their real halves.
- Task 9's real half depends on F-3 Feature 2's session hint reader.
- Features 2-5 attach their pieces to `S25WeatherModule` after this feature; the hand-off is the
  `S25WeatherModule` public header only.

No Dia files are added or edited (AD-001: no sign-off needed). The three tasks that would be tempting
Dia edits (card widgets, scout, history) all stay CoW feature-local.

## Implementation Patterns

### Files and layout (all tasks)
```
Cluiche/Assets/CoW/Stages/S25WeatherPatterns/
    s25_weather_patterns.diastage
    misc/ApplicationFlow/s25_weather_patterns.diaapp
    s25_worlds.json                          (envelope cow.s25_worlds v1)
Cluiche/CoW/Modules/S25WeatherModule.h/.cpp
Cluiche/CoW/Modules/S25WorldScout.h/.cpp     (test-time utility, not shipped; see task 21)
Cluiche/CoW/Modules/S25CommandRegistrar.h/.cpp (owned by S25WeatherModule; collects every cow.s25.* name; task 17)
Cluiche/CoW/Shared/Weather/S25/S25WorldTable.h/.cpp        (pure: loader + data types)
Cluiche/CoW/Shared/Weather/S25/S25WorldPrechecks.h/.cpp    (pure)
Cluiche/CoW/Shared/Weather/S25/S25WorldStatus.h            (S25WorldStatus, S25StepMode)
Cluiche/CoW/Shared/Weather/S25/S25CardModel.h/.cpp         (pure: control list + stats struct)
Cluiche/CoW/Modules/S25Card.h/.cpp                         (IDebugDomain, UI only)
Cluiche/Tests/GoogleTests/CoW/S25/TestS25{WorldTable,Prechecks,Module,StepMode,Tools,Commands,Card}.cpp
docs/specs/applications/cow/systems/s25-weatherpatterns/mockups/s25-card.html
```
The mockup path is fixed by amendment 1 (2026-11-16): `s25-weatherpatterns/mockups/s25-card.html`, a `mockups` folder inside the
system's spec folder (as S-3.5 does), not `docs/mockups/`. All new files are
registered with `dia docs vcxproj-add CoW "<path>" --filter <filter>` (idempotent; run it per task, not
at the end). Use `/new-cow-stage` once for the stage scaffold (task 2); do not hand-build the manifest.

### Conventions
- Namespace `CoW::S25`, S25 prefix on every type (AD-002). Members `m`-prefixed, constants `k`-prefixed.
- Public APIs use Dia containers and `String*`; jsoncpp only at the load edge (PD-004).
- Loaders are all-or-nothing: parse into a local, commit on success, return a typed error naming the
  field. Unknown keys are invalid (same convention as the baseline store in Feature 2).
- Every external collaborator is a small interface so the module is testable on SimPU-less fakes:
  `IS25WorldSource` (set preset/seed/size, `IsReady(revision)`, terrain read), `IS25SessionHintReader`,
  `IS25WorldStep` (`SetDeferred`, `Step`, `IsIdle`). Real adapters wrap `WorldGenerationModule`,
  F-3's reader and `WorldStepModule` through `ModuleRef`; no singletons, no `GetStatic`.
- Cross-PU rule: the card never reads module state across threads. `S25CardStats` is a plain struct
  published by the module on SimPU through the existing debug-domain stats path (same as A4's card).

### World table and prechecks (tasks 3-6)
- `S25WorldTable::Load(const Json::Value&, S25WorldTableError&)`; entries
  `{id, preset, seed, size, DynamicArrayC<StringCRC> prechecks, thresholds}`. Threshold defaults are
  the spec's (`minOceanCells` 200, `minCoastCells` 40, `minRidgeCells` 6, `minLowlandCells` 20,
  `minInteriorCells` 30); they are data in the file, not constants in the code.
- `struct S25PrecheckResult { StringCRC id; bool passed; double measured; double required; double margin; }`
  with `margin = measured - required` (negative on failure). One free function per precheck, signature
  `S25PrecheckResult CheckOceanPresent(const WeatherTerrain&, const S25PrecheckThresholds&)`; a
  `RunPrechecks(terrain, ids, thresholds, DynamicArrayC<S25PrecheckResult>&)` driver.
- Distances come from A4 Feature 6's `ComputeWaterDistance`; wind direction from `PrevailingDirection`
  (A4 Feature 5 exports). No new distance code here (Feature 2 adds the `ComputeDistance` overload
  later; this feature uses only the A4 function).
- `ridge_crosses_wind` is the one non-trivial precheck: enumerate ridge-crest cells (elevation >= 0.6),
  find windward/leeward pairs at least 3 steps apart along `PrevailingDirection(latitude)`, require a
  crest of at least `minRidgeCells`. The "parallel ridge" negative case (AC 6) is the first test.
- Hand-built terrain helpers live in `S25/Testing/S25TestTerrain.h` (builders: `OceanStrip`, `Ridge(angle)`,
  `Lowlands`, `Interior`), reused by Feature 2 and 3 tests if they are the same shape; if Feature 2's
  `ScriptedTrace` terrain builder exists first, reuse it instead of duplicating.
- Results are cached keyed by world revision; re-run on `WorldGenerationComplete`.

### S25WeatherModule (tasks 7-13)
- `class S25WeatherModule : public Dia::ApplicationFlow::SimModule` exactly as in the spec:
  `LoadWorld(StringCRC)`, `GetWorldStatus()`, `GetStepMode()`. `DoStart` resolves collaborators via
  `ModuleRef`, loads the table, picks the default or hinted world, registers commands.
- World load: set S-1 generation inputs through the `IS25WorldSource` seam, then wait for
  `IsReady(revision)` on later ticks (non-blocking state machine: `Idle -> Requested -> Ready/Failed`).
  Never touch `GetCurrentWorld()` except to copy terrain for prechecks (SD-S1-010).
- `S25WorldStatus { id, preset, seed, size, revision, prechecks[], verdictEligible, curated }`;
  `verdictEligible = curated && size == Small && all prechecks passed`.
  Failed precheck: status carries "invalid world: re-pick the seed", health Degraded, run continues.
- Size change goes through `cow.worldgen.set_size`; the module re-runs prechecks after A4 re-spins. Readiness after the change is `WeatherModule::IsReady(revision)` for the new revision (A4 Feature 3); no other readiness API is assumed.
- Session hint at `DoStart`: reader returns `{found, valid, bundlePath}`; absent -> default,
  unreadable or invalid -> one Warning and default.
- Step mode: `S25StepMode { Sync, Deferred }` mirrors `WorldStepModule` through the `IS25WorldStep` seam.
  Mode switches call `SetDeferred(bool)` and report the refusal reason verbatim on the card.

### World Step amendment (task 10)
`WorldStepModule::SetDeferred(bool)` returns `bool` with a reason out-param, refuses while a step is in
flight or a week is pending, registers the `world` channel on demand when switching to Deferred and
unregisters when switching to Sync. `cow.worldstep.step` gains `{"action":"kick"|"set_mode","mode":"sync|deferred"}`.
State at the switch is unchanged. The spec text lives in `worldstep/module-and-cadence.md` (edited by
that feature's close-out, not here).

### Tools wiring (tasks 14-15)
- Step-back: the card model computes the button state from `{driverPresent, historyAvailable}` with the
  two spec reasons ("history driver not built", "no history yet"). Real action posts
  `cells.history.view` / `cells.snapshot.restore`; tests use a `FakeHistoryDriver` that records the
  posted commands and serves scripted retained weeks.
- Click-to-inspect and layer picker are DiaCellLayers shared widgets hosted on the card; this feature
  tests only the wiring (selection state, command names posted, hotkey table has no clash for A4's
  seven layers), not the widgets.
- Boundary switch posts `cow.weather.params {"action":"set","key":"boundary.edge_inflow","value":0|1}`.

### Commands (task 17)
`cow.s25.world` (command) and `cow.s25.status` (query), sim-hosted via `SimQueryHostModule`,
category `s25`, mandatory help, JSON-object arguments. Registration goes through `S25CommandRegistrar`
(amendment 5; Feature 3 amendment 2), which `S25WeatherModule` owns and which collects every `cow.s25.*` name
(this feature's two and Feature 3's four) and rolls all of them back together if any registration fails; on
any failure unregister what succeeded and log an Error. Feature 4's three `cow.scenario.*` names belong to
`ScenarioModule` and roll back separately. Unknown world or size returns an error listing the valid names and changes nothing.

### Card (tasks 16, 19, 20)
`S25CardModel` (pure) owns the ordered control list `{controlId, label, commandName, enabledReason}`
and the `S25CardStats` struct; `S25Card : IDebugDomain` renders it per the signed-off mockup, accent
from `DebugGroupAccents`, description <= 80 characters, and posts commands only (UI/logic separation
rule). AC 23's test walks `S25CardModel`'s control list against the registered command names.

### Instrumentation (task 18)
Channel `S25`: logs and `DIA_TRACE_ZONE` per the spec; `IHealthReporter` Degraded on failed precheck;
metric `s25.world_ready_ms` with `MetricInfo`, written on SimPU.

### Verification cadence
- Per task: narrow `dia run googletest --filter="S25Prechecks*" --shards 2` style filters on the suite under work; the feature-wide `--filter="S25*"` runs only in task 29 (2026-11-17, speed-ups).
- Full suite runs once, in the Weather-stack close-out (worldstep.plan.md row 6). No mutation checks. Sanitizer: on request only (user, 2026-11-17). Perf is Debug ballpark only (2026-11-17, speed-ups). Long tests (the real-world scout run) are tagged `SLOW_`; `SLOW_*` is skipped by default.
- Never call `CluicheTest.exe`/`GoogleTests.exe` directly: `dia run` / `dia launch` only. Kill leftover
  `CluicheTest.exe` before E2E (feedback_dia_run_gotchas).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Write `docs/specs/applications/cow/systems/s25-weatherpatterns/mockups/s25-card.html` (create the `mockups` folder): the S25 card with every control (world id/preset/seed/size/revision, verdict-eligible vs exploration, prechecks with value+margin, step mode Sync/Deferred switch, day/week/season/speed, Step-one-week, baseline note, history status, hint source), the A4 layer picker, timeline and overlay toggles around the CellField view, plus three states: failing precheck, exploration world, history unavailable | Opens in a browser; every AC 23 control is visible; the three states are shown | Not Started | sonnet | **GATE: user sign-off in conversation required before tasks 16 and 19 (any card code).** Project rule: mockup before any editor/UI feature. Do not proceed to card code on assumed approval |
| 2 | Scaffold the stage with `/new-cow-stage`: `.diastage`, `.diaapp`, empty `s25_worlds.json` envelope, shell `S25WeatherModule`, module list in the spec's manifest order (no Ecology/Succession/SaveLoad); `dia docs vcxproj-add` for each new file; `dia validate manifest` | AC 1 (manifest validates, order, absent modules) | Not Started | sonnet | Mechanical scaffold, TDD-exempt except a manifest-order test written first. `HistoryDriverModule` listed as soft (absent until S-3.5 F1). Config: `WorldStepModule` order `["weather"]`, `apply_after_days 7`, `deferred false`; `WeatherModule` history block exactly as in the spec (<= 255 chars) |
| 3 | RED -> GREEN: `S25WorldTable` loader (`cow.s25_worlds` v1), thresholds with defaults, all-or-nothing, unknown key / wrong envelope / empty worlds / duplicate id / unknown precheck id rejected | AC 9 (table half), AC 11 (curated flag); loader rejection cases | Not Started | sonnet | Pure. Preset names and seeds in the shipped file stay placeholders until task 23. Tests use inline JSON, not the shipped file |
| 4 | RED -> GREEN: `S25PrecheckResult`, `RunPrechecks`, and `ocean_present`, `coast_band`, `interior_far_from_water` on hand-built terrain (pass terrain and lacking terrain each; measured value and margin) | AC 5 (three of five prechecks) | Not Started | sonnet | Uses A4 `ComputeWaterDistance`. Establish `S25/Testing/S25TestTerrain.h` builders here |
| 5 | RED -> GREEN: `ridge_crosses_wind` and `lowlands_downstream`, including the ridge parallel to the prevailing wind (fail), a ridge across it (pass), crest below `minRidgeCells`, and lowlands only counted downwind of the ridge | AC 5 (remaining two), AC 6 | Not Started | sonnet | The cross-wind condition is the point; write the parallel-ridge test first. Uses `PrevailingDirection` from A4 Feature 5 |
| 6 | RED -> GREEN: precheck cache keyed by world revision (recompute only on a new revision) and the "invalid world" status text carrying the failing id | AC 7 (status/Degraded flag half) | Not Started | sonnet | Pure `S25PrecheckCache`. Health wiring is task 18 |
| 7 | RED -> GREEN: `S25WeatherModule` world load against `FakeWorldSource`: default world loads and A4-side readiness (`IsReady(revision)`) reached with no manual step, `LoadWorld(id)` regenerates and re-runs prechecks, unknown id errors listing valid ids and changes nothing, non-curated pick runs no prechecks and is "exploration", failed generation leaves the previous world and logs | AC 4, AC 9, AC 11, AC 7 (still runs) | Not Started | sonnet | Define `IS25WorldSource` and its real adapter over `WorldGenerationModule` (`IsReady(revision)`, `WorldGenerationComplete`). Real-adapter integration is exercised by the task 24 smoke |
| 8 | RED -> GREEN: size picker and verdict-eligible flag: Standard/Large -> "exploration: no verdict" and prechecks re-run, back to Small restores the flag; invalid size errors | AC 10 | Not Started | sonnet | Routes through `cow.worldgen.set_size` via the world-source seam; A4 re-spin is A4's own (Feature 3) and is faked here. Readiness after the change is `IsReady(revision)` for the new revision (Feature 3 amendment 7); the fake source asserts the new revision is the one awaited |
| 9 | RED -> GREEN: session hint seam with `FakeSessionHintReader`: valid hint loads the bundle's world instead of default, absent -> default, unreadable/invalid -> one Warning and default; card shows hint source | AC 21 | Not Started | sonnet | **Blocked for the real reader until F-3 Feature 2 (session hint) is built**; the fake-reader tests are not blocked. Only the interface and fake ship now. Bundle load itself (`world_bundle`) is also F-3; fake reader returns a terrain-only stub |
| 10 | RED -> GREEN: **World Step Feature 2 amendment** - `WorldStepModule::SetDeferred(bool, reason&)`, idle-only rule, on-demand channel register/unregister, `cow.worldstep.step` `action` argument (`kick`, `set_mode`); tests belong to the WorldStep suite (`WorldStep*`) | AC 14 (module half): refused while in flight / pending week with a reason; allowed idle; unchanged committed state; switch back | Not Started | sonnet | **Cross-plan: owned by World Step Feature 2's code (CoW-Shared, no Dia change, AD-001 N/A).** Coordinate with that feature's plan; if the World Step plan has not built `WorldStepModule` yet this task is blocked. Record in that plan's notes too. Spec text for `module-and-cadence.md` is updated by its owner (task 30 lists it) |
| 11 | RED -> GREEN: mode switch in `S25WeatherModule` against `FakeWorldStep`: Sync default, Sync<->Deferred through `SetDeferred`, refusal reason surfaced, step-one-week label "advance 7 days" in Deferred, neither switch changes committed state | AC 12 (mode default), AC 14, AC 16 | Not Started | sonnet | `IS25WorldStep` seam; real adapter over `WorldStepModule` from task 10 |
| 12 | RED -> GREEN: step-one-week control: Sync = `cow.worldstep.step` then pause (GameSpeedController command), refused with a reason before the world is ready, Deferred = advance 7 days; speed/pause go only through GameSpeedController commands | AC 13, AC 12 (week advances once per game week) | Not Started | sonnet | Uses the real `GameSpeedControllerModule` calendar via a fake day source; the real-module path is covered by task 13 |
| 13 | RED -> GREEN: **mode-equivalence test**: the same 52 weeks in Sync and in Deferred produce identical committed fields after the same number of applied weeks (real `WorldStepModule` + `WeatherModule` on hand-built terrain, synchronous launcher) | AC 15 | Not Started | sonnet | Stage-level oracle. Needs World Step F1-F3 and A4 F4 module built; skip-with-reason (not failing) if the deferred path is not yet integrated, and say so in notes |
| 14 | RED -> GREEN: step-back/timeline gating against `FakeHistoryDriver`: disabled with "history driver not built" when absent, "no history yet" before first sample, enabled and posting `cells.history.view` / `cells.snapshot.restore` with a scripted driver; 13-week retention of the three tracked layers at Small under the 16 MB budget | AC 19 | Not Started | sonnet | **Real-driver integration blocked until S-3.5 Feature 1 `HistoryDriverModule` exists** (Open Design Question 1: consider building the driver first). Per amendment 3, AC 19 (13-week retention) needs `CellHistoryModule` **and** `HistoryDriverModule` (the driver does the sampling), so the real retention test is gated with the step-back work; only the fake-driver path ships now |
| 15 | RED -> GREEN: tools wiring: click selects a cell and the detail rows (A4 rows plus biome and elevation) are requested, off-map click clears; boundary switch posts `cow.weather.params` and the change is effective on the next committed week only; layer picker lists A4's seven layers with hotkeys and logs no clash | AC 17, AC 18, AC 20 | Not Started | sonnet | Wiring tests only (command names, selection state, hotkey table); the widgets are DiaCellLayers'. Use `Snow = K` as the anchor hotkey case |
| 16 | RED -> GREEN: `S25CardModel`: ordered control list mapped to command names, `S25CardStats` published struct (world, prechecks, mode, day/week/season/speed, baseline note, history status, hint source), description <= 80 chars, baseline note "table baseline (no ecology), expected" | AC 23 (model half), AC 24, AC 3 (baseline source text) | Not Started | sonnet | Pure logic, no rendering. **Gated by Task 1 sign-off** (it fixes which controls exist): do not start until the mockup is approved. Walk the control list against the registered command names in the test |
| 17 | RED -> GREEN: `S25CommandRegistrar` (owned by `S25WeatherModule`, collects every `cow.s25.*` name, all-or-nothing) and commands `cow.s25.world` (load, size) and `cow.s25.status`, sim-hosted with help, forced failure of the second leaves none, unknown world/size error lists valid names; the registrar takes a name list so Feature 3 adds its four and the six roll back together | AC 22, AC 9 (command path) | Not Started | sonnet | Defines the registrar that Feature 3 task 16 extends (amendment 5); build it on A4's registration pattern. Category `s25`. Feature 4's `cow.scenario.*` names are not in it. Features 3/4 add names later; budget cap is 192 (`SimQueryHost::kMaxHandlers`), not 64 |
| 18 | Observation instrumentation for the module: `S25` channel logs, `DIA_TRACE_ZONE` around world load and prechecks, `IHealthReporter` (Degraded on failed precheck, never Unhealthy), metric `s25.world_ready_ms` with `MetricInfo` | AC 3 (Degraded for baseline only, no Unhealthy cause), AC 7 (Degraded) | Not Started | sonnet | RED the health assertions first (Degraded on forced failing precheck; none otherwise); logs and trace are untested plumbing |
| 19 | Implement `S25Card : IDebugDomain` to the signed-off mockup: renders `S25CardModel`, posts commands only, accent from `DebugGroupAccents`; `dia docs vcxproj-add` | AC 23 (no direct module calls), AC 24 | Not Started | sonnet | **Gated by the Task 1 sign-off.** UI only; no business logic. Include a test that the card file includes no module header besides the stats struct |
| 20 | F-1 debug menu entry for the stage; exit and re-enter gives a clean second visit (no leaked registrations, jobs or names) | AC 2 | Not Started | sonnet | Edits F-1 menu data (CoW, not Dia). Leak check: command names, `world` channel registration, job count before/after |
| 21 | Author the world scout `S25WorldScout` (test-time utility, `*Scout*` filter, not shipped), budget fixed by amendment 2: seeds 1..200 per S-1 preset (the preset list is S-1's), 60 s budget per world, candidates ranked by smallest precheck margin, the top 5 printed with every precheck value; RED -> GREEN against a stub world source | Scout returns only passing candidates sorted by minimum margin, at most 5, each with all precheck values; seeds beyond 200 and worlds over 60 s are not tried; deterministic output | Not Started | sonnet | Seed range, preset source and budget are constants from the amendment, not filter arguments. Pure CoW; no Dia change. The unit tests against the stub world source stay in the fast suite; the real S-1 run is tagged `SLOW_S25WorldScout` (all presets x 200 seeds) (2026-11-17, speed-ups) |
| 22 | Run the scout on S-1 for both curated worlds (`dia run googletest --filter="*Scout*" --shards 2`) and present the top 5 per world with every precheck value to the user | Candidate list for each world has at least one passing seed | Not Started | haiku | **REVIEW GATE: the user picks one preset+seed per world (taste check).** The user's pick fills the `<S-1 preset>` placeholders in `s25_worlds.json` (task 23). Report only the table; no test-output dump. This run is the feature's once-run of `SLOW_S25WorldScout`; filter `*Scout*` also matches the `SLOW_` name (2026-11-17, speed-ups) |
| 23 | Record the user's picks in `s25_worlds.json` (replacing the `<S-1 preset>` placeholders) and add the regression test that the chosen preset/seed pass all their prechecks on a fresh generation | AC 8 | Not Started | sonnet | Do not pick on the user's behalf. Picks are Small-only (Open Design Question 2) |
| 24 | DiaAutomation smoke scenario: enter the stage, wait for ready, step 4 weeks, switch to Deferred and back, exit; no Error logs (Degraded expected); first real exercise of real adapters | AC 25, AC 2, AC 4 | Not Started | sonnet | Follow F-2's automation pattern. Kill leftover `CluicheTest.exe` first. E2E does not build: build via `dia run`/`dia pipeline` before |
| 25 | Report weekly-tick CPU in Sync at Small against A4's 4.2 ms budget (`cow.weather.step_ms`) and the `s25.world_ready_ms` metric, to the log and `cow.s25.status` (amendment 4; same destination as Feature 3's `performance` block and INFO log) | AC 26 (reported, not gated in Debug) | Not Started | haiku | DiaCLI run-and-report; label `NotMeasured (Debug)` honestly. Debug ballpark only; no Release run (2026-11-17, speed-ups) |
| 26 | Real-integration pass for blocked halves: when S-3.5 F1 `HistoryDriverModule` and F-3 session hint exist, swap the fakes for the real adapters and rerun tasks 9, 14 | AC 19 (driver present), AC 21 (real reader) | Not Started | sonnet | **Blocked until S-3.5 F1 and F-3 F2 exist**; if still blocked at feature close, mark the feature Done-with-deferrals and list them in the system plan |
| 27 | Exhaustive test pass: gap audit of every AC (1-26) against tests; edge pass (empty/duplicate worlds, size toggled mid-load, load while a load is in flight, hint present but world id unknown, mode switch while paused, card with no world); AC 8 re-run | Gap list in notes; every AC mapped to a test or an explicit deferral | Not Started | sonnet | No mutation checks. Scoped filter `S25*` |
| 28 | Observation Opportunity Scan over all touched files across the five pillars (logs, traces, profiling, metrics, health) | Suggestions listed in notes | Not Started | sonnet | Suggestions do not block Done |
| 29 | Feature verification run: `dia run googletest --filter="S25*" --shards 2`, `dia check deps`, `dia validate manifest`, `dia check sln-sync` | All green, failures named | Not Started | sonnet | Full suite moved to the Weather-stack close-out (2026-11-17, speed-ups); report pass/fail and failing names only |
| 30 | Close-out: set plan rows Done with `dia docs plan docs/specs/applications/cow/systems/s25-weatherpatterns/stage-and-world.plan.md <#> --status Done`; `dia docs spec-done docs/specs/applications/cow/systems/s25-weatherpatterns/stage-and-world.md`; update system plan row 1; `dia docs registry` if a module doc was added | Spec status Done | Not Started | haiku | **Do not run until all prior rows are Done.** Commit per task as usual; commit message trailer per project attribution |

## Open items

Spec silences and uncertain points. Resolved items are kept for traceability.
- Mockup location: **Resolved by amendment 1 (2026-11-16)**: `s25-weatherpatterns/mockups/s25-card.html`.
- Scout preset list, seeds per preset, time budget, ranking: **Resolved by amendment 2 (2026-11-16)** (seeds 1..200, 60 s, top 5 by smallest margin; the user's pick fills the placeholders).
- How `S25WeatherModule` learns A4's re-spin is complete: **Resolved by amendment 7 of Feature 3 (2026-11-16)**: `WeatherModule::IsReady(revision)`.
- Where AC 26 CPU is reported: **Resolved by amendment 4 (2026-11-16)**: log and `cow.s25.status`.
- Whether AC 19 needs `HistoryDriverModule`: **Resolved by amendment 3 (2026-11-16)**: it needs both `CellHistoryModule` and the driver.
- Command registration helper ownership: **Resolved by amendment 5 (2026-11-16)**: `S25CommandRegistrar`.
- `cow.s25.status` field names beyond the spec's list are not defined (still open; the CPU-report values from amendment 4 are added to it).
- Open Design Question 1 (build `HistoryDriverModule` before S-2.5) is unresolved and drives whether tasks 14/26 stay blocked.
- **Review 2026-11-16:** `cow.s25.status` fields are fixed (system plan, decision 16); `HistoryDriverModule` is scheduled early (decision 1).
