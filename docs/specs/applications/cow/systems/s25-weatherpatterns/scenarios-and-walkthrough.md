# Feature Spec: S-2.5 Scenarios & Walkthrough (CoW-Shared)

**Parent:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md (Feature 4)
**Status:** Approved (2026-11-16)
**Contract it implements:** `ScenarioValidator`'s rule table in @docs/specs/applications/cow/systems/t2-worlddataeditor/scenario-editor.md (T-2 Feature 5); S-3's Feature 3 consumes the format, loader, registry and panel.
**Depends on:** S-2.5 Features 1 (stage, module, `LoadWorld`), 2 (`ICheck`, `IWeatherTrace`, `RecordedTrace`) and 3 (the run table the scenarios are tested against); A4 Feature 5 (typed `WeatherCommand`, the commands, the overlay and layer toggles) and Feature 6 (presets); DiaValueSchema command catalogue export; `SimQueryHostModule`; DiaCellLayers.
**Consumed by:** S-3 Feature 3 (its ten Controlled and four Living scenarios), T-2 Feature 5 (calls the same validator over the bridge).

## Summary

The shared guided-scenario system, built first for S-2.5 and reused by S-3: the `cow.scenario` file format and loader, the shared `ScenarioValidator`, a `CheckRegistry`, a scenario runner that loads a world, runs setup commands and shows the watch text, a **live check-line panel**, the **walkthrough** (steps that switch overlays, show text and pin a check), the three `cow.scenario.*` commands, and S-2.5's fourteen scenario files (nine tests plus five presets) with their vocabulary.

Scenarios are for **understanding**. The headless runner's plan (Feature 3) remains the verdict.

## Goals

1. A scenario does nothing the panel cannot: its setup is a list of command lines, and the command path is the same one the card uses.
2. One validator decides what is valid, whether the testbed's loader or the T-2 editor calls it, and with the same rules and the same line numbers.
3. A live check line shows the same result as the headless run for the same weeks (decided 2026-11-16: the live trace is the module's own full-precision ring, not DiaCellHistory).
4. A check that cannot be evaluated live says exactly why.
5. Loading a folder of scenarios never fails as a whole because one file is bad.
6. S-3 reuses everything here with no S-2.5 knowledge.

## Non-Goals

- Authoring scenarios (T-2 Feature 5).
- The verdict, report and baseline (Feature 3).
- Defining the checks (Feature 2) or the overlays (A4 Feature 5).
- A scripting language: setup is a flat list of command lines.
- S-3's ecology scenarios and checks (S-3 Feature 3).

## Design

### Placement

CoW-Shared (AD-002), two libraries:

- `Cluiche/CoW/Shared/Scenarios/`: `Scenario.h`, `ScenarioLoader.*`, `CommandLine.*`, `CheckRegistry.*`, `ScenarioRunner.*`, `LiveTraceRing.*`, `CheckLineModel.*` (pure, no modules).
- `Cluiche/CoW/Shared/WorldAssets/Core/ScenarioValidator.*` in `CoWWorldAssetsCore` (the T-2 contract's location). F-3 Feature 1 creates that library's project; if it is not built yet when this feature starts, this feature creates the project and F-3 adds to it (a first-task check).
- `Cluiche/CoW/Modules/ScenarioModule.*`: the SimModule, **stage-scoped**, hosting the runner, the panel and the three commands. S-2.5 and S-3 each declare their own instance.
- S-2.5 data: `Assets/CoW/Stages/S25WeatherPatterns/scenarios/` (`*.scenario.json`, `vocabulary.json`).

### File format

As the system spec and T-2: envelope `{"format":"cow.scenario","version":1}`, extension `.scenario.json`.

```json
{ "format": "cow.scenario", "version": 1,
  "id": "s25_rain_shadow", "world": "s25_coast_range",
  "setup":   [ "cow.weather.load_preset name=rain_shadow" ],
  "watch":   "The range blocks the prevailing wind. Rain falls on the windward slope; the leeward side stays dry.",
  "checks":  [ "s25.t2.shadow_present", "s25.t2.windward_gt_leeward" ],
  "walkthrough": [
      { "overlays": ["wind"],          "text": "Find the prevailing wind and the range across it." },
      { "overlays": ["precipitation"], "text": "Compare the two sides.", "check": "s25.t2.windward_gt_leeward" },
      { "overlays": ["ground_water"],  "text": "The shadow persists in ground water." } ] }
```

`id` must equal the file name without `.scenario.json`. `mode` (S-3) and `walkthrough` (S-2.5) are optional. Unknown keys are invalid (the repo-wide strict rule).

### Command lines

A setup entry is a string: a command name, then arguments as `key=value` or positional values, values with spaces in double quotes (T-2's grammar). `ParseCommandLine(const char*, CommandLine&, ParseError&)` tokenises it, and `RenderCommandLine` renders it back; the two are inverses for every valid line (the editor relies on that). Positional values bind to the command's declared argument order from the catalogue; the A4 commands that take an `action` are written named: `cow.weather.force action=storm cell=412 intensity=1.0 weeks=3`. The tokenised line becomes the JSON-object argument the sim-hosted command takes.

### `ScenarioValidator`

```cpp
Findings Validate(const Json::Value& scenario, const ICommandCatalogue&, const IScenarioVocabulary&, const IWorldIds&);
```

Pure, in `CoWWorldAssetsCore`. It implements the T-2 rule table exactly (envelope and `id`; `id` equals file name and is unique in the folder; `mode`; `world`; setup command unknown / argument name, missing required, type, range, enum value, too many positional; command missing from this stage's export; check not in vocabulary; duplicate check; no checks; walkthrough overlay not in vocabulary; walkthrough `check` not among the scenario's own `checks`; empty `watch`), each with a JSON path and, for text, a line number. Missing catalogue or vocabulary skips the rules that need it with **one** Warning. This feature adds **no rule** beyond the table; if one is needed it goes to T-2 first.

**Known limit.** The command catalogue cannot express "required when `action` is X". A4's consolidated commands (`cow.weather.force action=storm`) are validated for name, type and range of every argument present, but a missing `cell` for `action=storm` is caught by the command handler at run time, not by the validator. Closing that needs a `requiredWhen` field in the catalogue (a DiaValueSchema change, not proposed here; Open Design Question 3).

### `CheckRegistry`

```cpp
class CheckRegistry {
public:
    bool Register(const ICheck& check);                 // false on a duplicate id
    const ICheck* Find(StringCRC id) const;
    void  ListIds(DynamicArrayC<StringCRC>& out) const;
};
```

`S25WeatherChecks::RegisterAll` (Feature 2) fills it; S-3 registers its own. A scenario's `checks` are looked up here; the loader rejects an id that is not registered.

### Loader

`LoadScenarioFolder(path, validator, ...)` loads every `*.scenario.json`, validates each, and returns the valid ones plus a findings list. A file that fails (bad JSON, any Error finding) is **rejected with the file and line and the others still load**; Warnings load with the finding kept. The loader runs at stage start and on `cow.scenario.list {"reload":true}`.

### Live trace ring (decided 2026-11-16)

`LiveTraceRing` is an `IWeatherTrace` the module fills on each `WeatherUpdated` by copying the committed nine fields from `WeatherSnapshot::GetFields()` as **full float32**. Capacity 65 weeks (the longest single-run check, Test 5), oldest dropped first; at Small about 7 MB and at Large about 16 MB. Not saved. Cleared on world load, `cow.scenario.run`, and parameter changes that restart weather. Because it holds the same values the headless runner's `RecordedTrace` holds, a live result equals the headless result for the same weeks (acceptance criterion 9).

Checks run through Feature 2's `Evaluate`, so a check whose `CheckRequirements` are unmet shows **Pending** with the reason (`needs weeks 1..52, have 9`). A check that needs two runs (`t2.shadow_repeatable`, `t8.*`, `t9.determinism`) is flagged `headlessOnly` in its requirements and shows "headless only: run `cow.s25.run`" rather than a perpetual Pending.

### Scenario runner

A small state machine on SimPU: `Idle → LoadingWorld → SettingUp → Ready → Running`.

1. `cow.scenario.run {"id":...}` clears the ring, **pauses** the clock (so the watch text can be read; the user resumes), and requests the world through Feature 1's `LoadWorld` (skipped if it is already the current ready world, keeping its spin-up).
2. On `IsReady(revision)` it runs each setup line **in order**, through the sim-hosted command path (below). The first line that returns an error stops the scenario in `SetupFailed` with the line, the command and the error, and leaves the world as it was after the lines that succeeded.
3. It shows `watch`, enters `Ready`, and on the first resumed tick `Running`: after each `WeatherUpdated` it evaluates the scenario's checks against the ring and updates the panel.
4. `cow.scenario.run` for another id, a world change or stage exit stops the current one and clears the panel.

**Command path.** Setup lines are executed through the same sim-hosted dispatch the debug server uses, so a scenario cannot do something the console cannot. The first plan task confirms a local in-process call exists on `SimQueryHost` (it exposes `Has(name)`; no local `Execute` was found on 2026-11-16). **If none exists, a small additive `Execute(name, args)` is needed on `SimQueryHost` (a Dia change, AD-001: ask before building).** For the headless runner the same lines can be turned into A4's typed `WeatherCommand` and applied through the direct sink; a line for any non-`cow.weather.*` command in a headless context is reported unsupported.

### Check-line panel (CoW-Shared)

`CheckLineModel` (pure) holds, per scenario check: id, label (from the vocabulary), kind, state (Pass / Fail / MagnitudeMiss / Pending / InvalidWorld / HeadlessOnly), measured, threshold, detail. The panel card (`DIA_DEBUG`, via the shared debug card, **mockup first, plan Task 1**, extending `s25-card.html`) shows the scenario id and watch text, the lines, and the walkthrough controls. It posts commands only and reads a published model, not module state across threads. Live lines are labelled **understanding, not verdict**.

### Walkthrough

A scenario's `walkthrough` is an ordered step list. Each step: `overlays` (ids from the vocabulary), `text`, optional `check` (must be one of the scenario's own `checks`). **Showing a step** switches the named overlay or base layer on and the others off through the layer/overlay commands A4 Feature 5 and DiaCellLayers provide (no private path), shows the text, and pins the named check line at the top of the panel. `cow.scenario.walk {"action":"next"|"prev"}` moves through the steps, clamped at the ends (no wrap); it is refused with a reason when no scenario is running or it has no walkthrough. Leaving the scenario restores the overlays that were on before it started.

### Commands (3 names, all-or-nothing, shared with S-3)

Sim-hosted with mandatory help, category `scenario`:

- `cow.scenario.list` (query): `{"reload":false}`; lists ids with world, mode, check count, and the findings for any rejected file.
- `cow.scenario.run` (command): `{"id":"<id>"}` or `{"action":"stop"}`.
- `cow.scenario.walk` (command): `{"action":"next"|"prev"}`.

A forced failure registering any of the three leaves no names behind. (The budget is the per-PU sim-hosted cap, 192 in code today.)

### S-2.5's fourteen scenarios and the vocabulary

All use `s25_coast_range` unless noted. Checks are Feature 2's ids with the `s25.` prefix (`s25.t1.moisture_monotonic`, …). Setup lines use the A4 consolidated commands.

| Scenario id | Source | Setup | Checks |
|---|---|---|---|
| `s25_t1_coastal_rain` | Test 1 | none | `t1.*` (5) |
| `s25_t2_rain_shadow` | Test 2 | `cow.weather.load_preset name=rain_shadow` | `t2.shadow_present`, `t2.windward_gt_leeward`, `t2.shadow_repeatable` (headless only) |
| `s25_t3_storm_movement` | Test 3 | `cow.weather.force action=storm cell=<chosen> intensity=1 weeks=3` | `t3.*` (5) |
| `s25_t4_seasons` | Test 4 | none | `t4.*` (5) |
| `s25_t5_spring_flood` | Test 5 | none (natural 65 weeks) | `t5.*` (4) |
| `s25_t6_droughts` (`s25_dry_interior`) | Test 6 | `cow.weather.load_preset name=drought` | `t6.*` (3) |
| `s25_t7_variability` (`s25_dry_interior`) | Test 7 | none | `t7.*` (2) |
| `s25_t8_boundary` | Test 8 | `cow.weather.params action=set key=boundary.edge_inflow value=0` | `t8.*` (3; headless only) |
| `s25_t9_determinism` | Test 9 | none | `t9.determinism` (headless only) |
| `s25_p_coastal_storm`, `s25_p_rain_shadow`, `s25_p_winter_snow`, `s25_p_spring_melt`, `s25_p_drought` | the five presets | `cow.weather.load_preset name=<preset>` | the checks of the matching test (`t1.*`, `t2.*`, `t4.winter_wetter`+`t5.snow_builds_winter`, `t5.snow_falls_spring`+`t5.flood_follows_melt`, `t6.*`) |

`<chosen>` is the storm cell chosen by the runner's rule (Feature 3), resolved at scenario run time into the command line and shown in the panel. Each scenario has a `watch` text (one to three sentences) and a walkthrough of two to four steps. `vocabulary.json` (`cow.scenario_vocabulary` v1) lists the fourteen ids' checks, the overlays and layers a walkthrough may use, and the `modes` array empty. A GoogleTest fails when a registered check or overlay is missing from the vocabulary, or the vocabulary names one that is not registered.

**Agreement with the plan.** For each scenario that has a matching run in Feature 3's table (T2 → R5a, T3 → R4, T6 → R6, T8 → R3, the presets), a test asserts the scenario's setup lines parse to the same typed `WeatherCommand`s as the plan run's setup.

### Instrumentation

Channel `SCN`. Info on scenario loaded/rejected counts at stage start, run start, state transitions, walkthrough step; Warning on a validator Warning, a `headlessOnly` check shown, a failed `LoadWorld`; Error on a setup failure and a failed registration. `DIA_TRACE_ZONE` around load, validation and per-week evaluation. Health: Degraded if any scenario file was rejected. Metric: `scn.check_eval_ms` (per-week evaluation time, `MetricInfo`), because 34 checks over 65 weeks must stay a small fraction of the frame.

## Public API

As Placement; `ICheck` and `CheckRegistry` shape defined with Feature 2; `IScenarioCommandSink` (live dispatch) and `HeadlessCommandSink` implementations. New messages: none.

## Acceptance Criteria

### Command lines and validator
1. `ParseCommandLine` handles named, positional and quoted arguments and `RenderCommandLine` round-trips every valid line to identical text; a malformed line (unterminated quote, empty command, duplicate named argument) returns a `ParseError` with the offset.
2. `ScenarioValidator` produces **every** rule in the T-2 table with the right severity and JSON path and line; one test per rule, plus missing-catalogue and missing-vocabulary degradation (exactly one Warning each).
3. The validator never throws, accepts a valid scenario with no findings, and `Validate` on the same input gives the same findings in the same order.
4. A missing `cell` for `action=storm` is not a validator finding (the known limit) and is caught by the command handler at run time with a clear error.

### Loader and registry
5. A folder with one bad file loads the others, reports the bad one with file and line, and sets Degraded; Warnings load with their findings.
6. `Register` rejects a duplicate check id; the loader rejects a scenario naming an unregistered check.
7. The fourteen shipped scenarios all load with **no Errors**, and the vocabulary sync test passes.

### Live trace and check lines
8. The ring holds 65 weeks, drops the oldest, clears on world load and scenario start, and is not saved.
9. **Live equals headless:** over the same 13 weeks, every single-run check evaluated on the ring gives the same state and the same `measured` (bit-identical) as on the headless `RecordedTrace`.
10. A check with unmet requirements is `Pending` with the reason; a `headlessOnly` check is `HeadlessOnly` and never Pending.
11. Per-week evaluation of all registered checks over a full ring at Large stays within the reported budget (measured, reported; the metric exists).

### Runner and walkthrough
12. `cow.scenario.run` pauses the clock, loads the world (reusing the current ready world when it matches), runs the setup lines in order, shows the watch text, and enters `Running` on the first resumed tick.
13. A setup line that errors stops the scenario in `SetupFailed` naming the line, command and error; the lines before it have taken effect.
14. Running another scenario, changing world or leaving the stage stops the current one, clears the panel and restores the overlays that were on.
15. `cow.scenario.walk next/prev` moves through steps and clamps at both ends; each step turns on exactly its overlays, shows its text and pins its check; it is refused with a reason when idle or when the scenario has no walkthrough.
16. A walkthrough step's `check` that is not in the scenario's `checks` is a validator Error, so it cannot reach the runner.

### Commands
17. The three names register with non-empty help; a forced failure of any leaves **no** names behind; S-3's instance registers the same names in its own stage with no conflict.
18. Invalid arguments (unknown id, action, `walk` with no scenario) return an error naming the field and change nothing.

### Agreement and smoke
19. For each scenario with a matching plan run, its setup lines parse to the same typed commands as the plan's.
20. A DiaAutomation scenario enters the stage, runs `s25_t2_rain_shadow`, steps 13 weeks, and reads the live check lines: `t2.shadow_present` and `t2.windward_gt_leeward` are `Pass` or `Fail` (never Pending) and `t2.shadow_repeatable` is `HeadlessOnly`.

## Files Touched

New: the placement files; the 14 scenario files and `vocabulary.json`; tests `Tests/GoogleTests/CoW/Scenarios/{TestCommandLine,TestScenarioValidator,TestScenarioLoader,TestLiveTraceRing,TestScenarioRunner,TestScenarioVocabulary}.cpp`; the panel mockup update. Edited: `CoW.vcxproj(.filters)`, `GoogleTests.vcxproj(.filters)`, the `CoWWorldAssetsCore` project (created here if F-3 Feature 1 is not built), `S25WeatherModule` (hosts the instance). **Spec edits on approval:** the system spec's Scenarios section (the 3 command names, ring-based live checks, `HeadlessOnly`, the preset scenarios' checks); T-2 `scenario-editor.md` (note the catalogue's `requiredWhen` limit); F3's note that scenarios and plan runs are tested for agreement. **Possible Dia change, ask first (AD-001):** an additive `SimQueryHost::Execute(name, args)` if no local dispatch exists.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | The format, loader, registry, validator and panel are CoW-Shared because S-3 is the second consumer; the S-2.5 scenarios and vocabulary are feature-local |
| AD-001 | Ask permission before Dia edits | None built here; a possible `SimQueryHost::Execute` is flagged before any work |
| PD-004 | No STL in public APIs | Findings, command lines and the ring use Dia containers |
| Editor-actions rule | Every tool action is a command; the panel calls the same commands | Setup runs through the command path; the panel posts the three commands |
| T-2 VS-004 | One rule set, the shared validator | The validator is the single implementation; no rule is added here |
| HTML mockup rule | Mockup before any UI | Plan Task 1 extends `s25-card.html`; user sign-off before panel code |
| PU boundaries are streams | No cross-PU mutex reads | The panel reads a published model |

## Test Plan

Pure parts first and heavily (command-line grammar, the validator rule table, the loader, the ring and `CheckLineModel`), then the runner against a fake world source, a fake command sink and a scripted `WeatherUpdated` feed, then the live-equals-headless test on hand-built terrain through the real A4 module, then one DiaAutomation smoke. TDD order: command lines, validator, loader and registry, ring, check lines, runner, walkthrough, commands, shipped scenarios and vocabulary. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. The panel gets a one-screenshot taste check per the mockup.

## Open Design Questions

1. **Does a local command dispatch exist?** The runner's guarantee ("does nothing the panel can't") depends on executing setup through the same path the debug server uses. If `SimQueryHost` has no in-process execute, an additive Dia method is the cleanest answer and needs your sign-off before the first task; the fallback (call the module methods directly) breaks the guarantee and is not recommended.
2. **Pausing on `cow.scenario.run`.** Pausing so the watch text can be read helps a walkthrough but slows a "run it and look" use, and S-3 may want its scenarios to start running. A per-scenario `autostart` flag would need a T-2 rule. Default is pause; revisit with S-3 Feature 3.
3. **Conditional required arguments.** A4's consolidated commands push "required when action is X" into the handler, so the T-2 editor and the validator cannot flag a missing `cell` while a scenario is being written. A `requiredWhen` field in the DiaValueSchema command catalogue would fix it for every consolidated command in the project; it is a Dia-side change and is not assumed here.
4. **Fourteen scenarios is a content task, not just code.** Each needs a `watch` text and a two-to-four step walkthrough. The plan should treat authoring them as its own task with your review, since the wording is what a person reads.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **`needsComparison`.** Feature 2's `CheckRequirements.needsComparison` is what marks a check headless-only; the earlier `headlessOnly` name in this spec means that flag. A live context has no comparison trace, so those checks show `HeadlessOnly`.
2. **Mockup path** is `s25-weatherpatterns/mockups/s25-card.html` (Feature 1's amendment 1); this feature's Task 1 depends on Feature 1's Task 1 having created it.
3. **Overlay restore (criterion 14).** Leaving a scenario restores the **stage defaults** recorded in the stage config (base layer `weather.precipitation`, wind overlay off), not an earlier state, because no overlay-query command exists. If DiaCellLayers has one, plan Task 0 uses it and this item is dropped.
4. **Line numbers.** `LoadScenarioFolder` builds a `JsonLineMap` (token offset to line) while parsing and passes it to the validator as an optional `ILineMap`; with none, findings carry the JSON path only.
5. **`IWorldIds`.** The production implementation (`StageWorldIds`) lists the stage's `s25_worlds.json` ids, S-1's preset names and the `.wld.json` files S-1 can load. S-3 supplies its own.
6. **`RegisterAll` and check lifetime** follow Feature 2's amendment 6.
7. **Live value of headless-only scenarios.** `s25_t8_boundary` and `s25_t9_determinism` are mostly `HeadlessOnly` live; their `watch` text says so and names `cow.s25.run`. They exist for the walkthrough text, the setup lines and agreement with the plan runs.

## Status

**Status:** `Approved` (2026-11-16). Plan: [scenarios-and-walkthrough.plan.md](scenarios-and-walkthrough.plan.md) (written 2026-11-16, not started). **Plan Task 1 is the panel mockup.**
