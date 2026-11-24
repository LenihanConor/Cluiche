# Feature Spec: S-2.6 Scenarios & Walkthrough

**Parent:** @docs/specs/applications/cow/systems/s26-riversfloods/s26-riversfloods.md (Feature 4)
**Status:** Approved (2026-11-24)
**Contract it consumes:** @docs/specs/applications/cow/systems/s25-weatherpatterns/scenarios-and-walkthrough.md (S-2.5 Feature 4: `cow.scenario` format, `ScenarioValidator`, `CheckRegistry`, `CheckLineModel`, `ScenarioModule`, the three `cow.scenario.*` commands). **S-2.6 builds none of the shared parts.**
**Depends on:** S-2.5 Feature 4 built; S-2.6 Features 1 (stage, module, live series), 2 (checks, `RegisterAll`) and 3 (the run table the scenarios are tested against); A5 Feature 5 (`cow.hydro.*` commands, layers and overlay) and Feature 6 (presets). Coupled scenarios need A4 Features 1–3 and 8.
**Consumed by:** nobody downstream; S-3 has its own scenarios on the same shared format.

## Summary

S-2.6's guided scenarios: **one scenario per test (ten) plus the four A5 presets as guided setups**, the `s26.*` check and overlay registration over the CoW-Shared format, a `vocabulary.json` with its GoogleTest, and the walkthrough text. Scenarios are for **understanding**; Feature 3's plan remains the verdict.

## Goals

1. A scenario does nothing the panel cannot: its setup is a list of command lines through the shared command path.
2. Every shipped scenario loads with no validator Errors, and the vocabulary matches the registered checks and overlays exactly.
3. A scenario's setup lines mean the same as the plan run it mirrors (a test proves it).
4. A check that cannot be evaluated live says why: `Pending` (weeks not yet run), `CoupledOnly` (Scripted mode), or "needs a comparison run".
5. The scenario's `watch` and walkthrough text is **content with a human reviewer**: you review the wording (it is its own plan task).

## Non-Goals

- The format, loader, validator, registry, runner, check-line panel and the three `cow.scenario.*` commands (S-2.5 Feature 4). S-2.6 adds **no validator rule**; if one is needed it goes to T-2 first.
- The checks (Feature 2), the overlays (A5 Feature 5), the verdict (Feature 3).
- Authoring tools (T-2 Feature 5).

## Design

### Placement

Feature-local data and registration (AD-002): `Assets/CoW/Stages/S26RiversFloods/scenarios/` (`*.scenario.json`, `vocabulary.json`), and `S26ScenarioRegistration.h/.cpp` in `Cluiche/CoW/Modules/S26/` which registers the `s26.*` checks with the shared `CheckRegistry` (Feature 2's `RegisterAll`) and supplies S-2.6's overlay vocabulary, `IWorldIds` (`s26_worlds.json` ids, S-1 presets, `.wld.json` files) and the command catalogue. `S26RiversFloodsModule` declares its own `ScenarioModule` instance (stage-scoped, as S-2.5 and S-3 do).

### Live checks over the series

The shared runner evaluates a scenario's checks after each applied week. S-2.5 fed `IWeatherTrace` from a 65-week field ring; **S-2.6's checks read Feature 2's per-week series** (`S26Series`, built by the module's `SummariseWeek` after every week and never dropped within a run), so the live lines and Feature 3's headless lines read the same numbers. Requirements decide the state: unmet weeks → `Pending` with `needs weeks 0..572, have 60`; a Coupled check in Scripted mode → `CoupledOnly`; a check needing a comparison run (`t10.ranges_identical`, `t10.goldens_match`) → shown as "headless only: run `cow.s26.run`" (the shared `needsComparison` flag). The scenario `Ready → Running` flow, pause-on-run, `SetupFailed`, and overlay restore follow S-2.5 Feature 4 unchanged (overlay restore uses the stage defaults: base layer `hydro.river_flow`, flow arrows on).

### Scenario format

As S-2.5: envelope `{"format":"cow.scenario","version":1}`, `id` equals the file name, `world`, `setup` (command lines), `watch`, `checks`, optional `walkthrough`. S-2.6 sets `mode` (optional shared key reserved for S-3) to nothing; the **mode is chosen by the setup lines** (`cow.s26.mode mode=coupled`).

```json
{ "format": "cow.scenario", "version": 1,
  "id": "s26_t1_floods_travel", "world": "s25_coast_range",
  "setup":   [ "cow.s26.mode mode=scripted", "cow.hydro.preset name=melt_pulse" ],
  "watch":   "A heavy mountain snowpack melts over a few weeks. The flood leaves the head early and reaches the mouth later, smaller and wider.",
  "checks":  [ "s26.t1.flood_not_early", "s26.t1.attenuation", "s26.t1.flood_appears_and_clears", "s26.t1.sea_balance" ],
  "walkthrough": [
      { "overlays": ["hydro.river_flow"], "text": "Find the head of the longest river and its mouth.", "check": "s26.t1.flood_not_early" },
      { "overlays": ["hydro.flooded"],    "text": "Step weeks and watch the flood move down the river.", "check": "s26.t1.flood_appears_and_clears" } ] }
```

### The scenarios

All use `s25_coast_range` unless noted; default magnitudes are shown as the **informational** lines they already are (not repeated here).

| Scenario id | Test | Setup | Checks (prefix `s26.`) |
|---|---|---|---|
| `s26_t1_floods_travel` | 1 | `cow.hydro.preset name=melt_pulse` | `t1.flood_not_early`, `t1.attenuation`, `t1.flood_appears_and_clears`, `t1.sea_balance`, `t1.lag_weeks` |
| `s26_t2_rivers_shrink` | 2 | Scripted: a normal year then `cow.s26.forced_year kind=dry` (rain 0.2×) then a rain year | `t2.dry_year_falls`, `t2.recovers_with_rain`, `t2.flow_survives`, `t2.surviving_fraction` |
| `s26_t2_dry_decade` (`s25_dry_interior`) | 2 | `cow.hydro.preset name=wet_then_dry` | `t2.low_flow_counter_climbs` |
| `s26_t2_frozen_weeks` | 2 | the cold-winter setup (the plan's H6) | `t2.frozen_weeks_neutral` |
| `s26_t3_lakes_breathe` | 3 | `cow.hydro.preset name=wet_then_dry` | `t3.*` (5 hard, 2 default) |
| `s26_t4_ice_age` | 4 | `cow.hydro.preset name=ice_age` | `t4.*` (7 hard, 3 default) |
| `s26_t5_water_balances` | 5 | `cow.hydro.preset name=<each>` (four variants as walkthrough steps) | `t5.weekly_balance`, `t5.cumulative_balance` |
| `s26_t6_nothing_cut_off` (`s25_dry_interior`) | 6 | the `ReachCycleScript` setup (headless is the verdict; live runs the same script) | `t6.no_protected_deep`, `t6.validator_unchanged`, `t6.lakes_only_safe` |
| `s26_t7_flash_flood` | 7 | `cow.hydro.preset name=flash_flood_fords` | `t7.*` (4) |
| `s26_t8_weather_drives_water` | 8 (Coupled) | `cow.s26.mode mode=coupled`, `cow.s26.cycle cycle=oscillation_4y action=run`, `cow.weather.load_preset name=spring_melt` | `t8.*` (3, `CoupledOnly` in Scripted) |
| `s26_t9_a3_counters` | 9 | `cow.hydro.preset name=wet_then_dry` (the counters' sources) | `t9.*` (5) |
| `s26_t10_determinism` | 10 | `cow.hydro.preset name=melt_pulse` | `t10.ranges_identical`, `t10.goldens_match` (headless only live) |
| `s26_p_melt_pulse`, `s26_p_wet_then_dry`, `s26_p_ice_age`, `s26_p_flash_flood_fords` | the four presets | `cow.hydro.preset name=<preset>` | the matching test's checks |

Fourteen files. `s26_t8_weather_drives_water` and the Coupled steps load and validate now but their checks show `CoupledOnly` until A4 exists (the file is valid either way). `<chosen>` values (the crossing, the long river) are resolved by Feature 2's `S26WorldInfo` at run time and shown in the panel.

Each has a `watch` text (one to three sentences) and a walkthrough of two to four steps (overlays from the vocabulary, text, optional pinned check). `vocabulary.json` (`cow.scenario_vocabulary` v1) lists the checks, the overlays and layers a walkthrough may use (the eight `hydro.*` layers, `hydro.river_state`, `hydro.basins`, `hydro.flow_dir`, flow arrows) and `modes` empty. A GoogleTest fails when a registered check or overlay is missing from the vocabulary, or the vocabulary names one that is not registered.

**Agreement with the plan.** For each scenario with a matching plan run in Feature 3's table (T1 → H1, T2 → H2/H3/H4/H6, T3 → H4, T4 → H5, T7 → H7, T9 → H4/H5, T10 → D runs, the presets), a test asserts the scenario's setup lines parse to the same typed `HydrologyCommand`s the plan applies.

### Instrumentation

Uses the shared `SCN` channel and `scn.check_eval_ms`; S-2.6 adds an Info log of scenario counts at stage start and a Warning when a scenario's world fails a precheck for one of its checks (the line shows `InvalidWorld`).

## Public API

New: `S26ScenarioRegistration.h/.cpp`; the 14 scenario files and `vocabulary.json`; S-2.6's `IWorldIds` and command-catalogue providers. No new command names (the shared three, `cow.scenario.list|run|walk`). Edited: `S26RiversFloodsModule` (hosts the `ScenarioModule`), vcxproj.

## Acceptance Criteria

### Loading and vocabulary
1. The fourteen shipped scenarios all load with **no Errors** through the shared loader and validator, against S-2.6's command catalogue and vocabulary.
2. The vocabulary sync test passes: every registered `s26.*` check and every overlay appears in `vocabulary.json`, and every vocabulary entry is registered.
3. Every walkthrough step's `check` is among its scenario's own `checks`, and every overlay id exists in the vocabulary.
4. A scenario naming an unregistered check, an unknown preset or an unknown world is rejected by the validator with the file and line, and the others still load.

### Live checks
5. Over the same weeks, every single-run check evaluated through a scenario gives the same state and the same `measured` (bit-identical) as Feature 3's headless series (live equals headless).
6. A check whose weeks have not run is `Pending` with the reason; in Scripted mode a Coupled check is `CoupledOnly` and never `Pending`; a comparison-run check shows "headless only".
7. Per-week live evaluation of all scenario checks over a 5,200-week series stays within the reported budget (measured, reported; `scn.check_eval_ms` exists).

### Agreement
8. For each scenario with a matching plan run, its setup lines parse to the same typed commands as the plan's (criterion above names the table).
9. A forced-dry scenario's `cow.s26.forced_year` setup produces the same rain schedule as plan run H2 (the `ScaledWaterInput` scale and week count are equal).

### Runner and walkthrough (on the real stage)
10. `cow.scenario.run id=s26_t7_flash_flood` pauses, loads the world (reusing the current one when it matches), applies `flash_flood_fords`, shows the watch text, and enters `Running` on the first resumed tick.
11. A setup line that errors stops the scenario in `SetupFailed` naming the line and error; lines before it have taken effect.
12. `cow.scenario.walk next/prev` moves through steps and clamps; each step turns on exactly its overlays, shows its text and pins its check.
13. Leaving a scenario restores the stage default overlays.

### Smoke
14. A DiaAutomation scenario enters the stage, runs `s26_t7_flash_flood`, steps through the 22 weeks, and reads the live lines: `t7.flood_occurs`, `t7.crossing_never_deep`, `t7.crossing_reached` and `t7.floods_clear` are `Pass` or `Fail` (never `Pending`) at week 22.

## Files Touched

New: the placement files; tests `Tests/GoogleTests/CoW/S26/{TestS26Scenarios,TestS26Vocabulary,TestS26ScenarioAgreement}.cpp`. Edited: `CoW.vcxproj(.filters)`, `GoogleTests.vcxproj(.filters)`, `S26RiversFloodsModule`. **Spec edits on approval:** the system spec's Scenarios section (14 scenarios; series-based live checks). No Dia or Draoi files.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Format, validator, registry and panel reused from CoW-Shared; scenarios and vocabulary feature-local |
| AD-001 | Ask permission before Dia edits | None. If S-2.5 Feature 4's `SimQueryHost::Execute` question is resolved with a Dia change, S-2.6 inherits that decision |
| T-2 VS-004 | One rule set, the shared validator | No rule is added here |
| Editor-actions rule | Every tool action is a command | Setup runs through the command path |
| GDD AD-02 | Two modes, one stage | Mode is set by a setup line (`cow.s26.mode`) |

## Test Plan

Pure parts first: load and validate all fourteen files, vocabulary sync, setup-agreement tests (no engine). Then the live-equals-headless test through the real hydrology module on hand-built terrain, then one DiaAutomation smoke. TDD order: scenario loading, vocabulary sync, agreement, live equality, smoke. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. One screenshot per walkthrough as a taste check.

## Open Design Questions

1. **Scenarios whose verdict is headless** (`s26_t6_nothing_cut_off`, `s26_t10_determinism`) show mostly "headless only" lines live, because R-Reach is 5,200 weeks and determinism needs comparison runs. They exist for the setup, the watch text and the agreement test. If that is too hollow, drop them and keep only the headless entries; say so.
2. **Coupled scenarios ship before A4 can run them.** They validate and show `CoupledOnly`. If you would rather not ship files you cannot use, delay `s26_t8_weather_drives_water` until A4 is built.
3. **Fourteen scenarios is a content task.** The wording is what a person reads, so authoring is its own plan task with your review (S-2.5 Feature 4's note).

## Status

**Status:** `Approved` (2026-11-24). No plan. Blocked on S-2.5 Feature 4 being built.
