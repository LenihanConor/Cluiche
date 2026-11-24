# Feature Spec: S-2.6 Stage & World

**Parent:** @docs/specs/applications/cow/systems/s26-riversfloods/s26-riversfloods.md (Feature 1)
**Status:** Approved (2026-11-24)
**Design source:** `DesignDocs/COW/Output Docs/GDD/10_PrototypeStrategy/S2.6_RiversFloods.md` §2, AD-01, AD-02, AD-09
**Depends on:** A5 Features 1–3 and 5 (`HydrologyModule`, the debug domain with its layers, overlay and `cow.hydro.*`, `ScriptedWaterInput` and `cow.hydro.preset` from Feature 6); S-1 Features 1 and 8, with amendment 8 (basin data) for real worlds; World Step Features 1–3 (order, Sync/Deferred switch); S-2.5 Feature 1's `s25_worlds.json` (curated worlds) and S-2.5 Feature 4's scenario host (Feature 4 of this system plugs in); this system's Feature 2 (prechecks, series, checks). Built: DiaCellLayers, DiaCellHistory, GameSpeedController, F-1, F-2. **Soft:** CoW-Shared `HistoryDriverModule` (S-3.5 Feature 1; step-back is gated on it), F-3's session hint. **Coupled mode** additionally needs A4 Features 1–3 and 8 (see Coupled tasks).
**Consumed by:** Features 2–5 (they attach to this stage), S-3.5's launch-outs.

## Summary

The S-2.6 stage: a `.diastage` and `.diaapp` that run A5 on a hand-chosen S-1 world with no ecology, succession or save module, the **Scripted** and **Coupled** modes, the world and size picker, the prechecks wired in, time controls with a year-step, the four presets, forced wet and dry years, the climate-cycle scrubber, the A5 tools hosted (layers, inspector, step-back), the live check lines, the readouts, and the F-1 menu entry. It contains no headless runner, no scenarios and no export: those are Features 3 to 5.

## Goals

1. Launch from F-1's debug menu into a stage that loads `s25_coast_range` at Small in Scripted mode and shows A5's water within seconds, with no ecology, succession or save module.
2. Each curated world carries the **prechecks** the tests need and says "invalid world" with the failing id and margin when S-1 has changed enough that a test no longer applies.
3. Scripted and Coupled are panel state; switching reloads the world and clears history (GDD AD-02).
4. One-week and **one-year** step controls make a flood or a drought observable.
5. Every action on the S26 card is a registered command; the card posts the same commands.
6. The stage honours F-3's session hint so S-3.5's "Open in S-2.6" continues with the same world (needs S-3.5 Feature 4 to list S-2.6; confirmed here).

## Non-Goals

- The headless runner, report and baseline recording (Feature 3), scenarios (Feature 4), history export (Feature 5), the checks themselves (Feature 2; this feature **calls** them for the live lines).
- Generating worlds (S-1), tuning A5 parameters, saving, water rendering (V-1).
- Presenting A3 conversions (Lake, Wetland, DRY_RIVERBED): counters only.

## Design

### Files and scaffolding

Created with `/new-cow-stage`:

```
Cluiche/Assets/CoW/Stages/S26RiversFloods/
    s26_rivers_floods.diastage
    misc/ApplicationFlow/s26_rivers_floods.diaapp
    s26_worlds.json                     envelope "cow.s26_worlds", version 1
Cluiche/CoW/Modules/S26RiversFloodsModule.h/.cpp     (S26 prefix, feature-local, AD-002)
Cluiche/CoW/Modules/S26/ScaledWaterInput.h/.cpp      (pure; forced years, see below)
```

Registered in F-1's debug menu. `dia validate manifest` accepts the stage.

### Module list (manifest order)

```
MessageBusHostModule, SimQueryHostModule, GameSpeedControllerModule, RandomStreamModule,
WorldGenerationModule     (config: world from s26_worlds.json default; preset + seed + size)
CellLayersModule, CellHistoryModule, HistoryDriverModule (soft)
DeferredStepModule
WorldStepModule           (order from the mode: Scripted ["hydro"]; Coupled ["weather","hydro"]; deferred false)
WeatherModule             (Coupled only; blocked on A4)
HydrologyModule           (config: parameters file, spinup_weeks, history block; fixed driver in Scripted mode)
S26RiversFloodsModule
```

No `EcologyModule`, `SuccessionModule` or `SaveLoadModule`. A5 without A3 reports no A3 counters consumer; that is expected and not a Degraded cause. A mode switch **tears down and rebuilds** the order (and adds or drops `WeatherModule`), then reloads the world; this is a module-set change, so the stage supports it by declaring both orders in the manifest and enabling one (confirmed in the first plan task against World Step Feature 2: if it cannot, the mode switch becomes "restart the stage with the other config", flagged).

### Worlds and prechecks

`s26_worlds.json`:

```json
{ "format": "cow.s26_worlds", "version": 1,
  "default": "s25_coast_range",
  "worlds": [
    { "id": "s25_coast_range",  "source": "s25_worlds.json",
      "tests": { "1":["long_river"], "2":[], "3":["dry_and_lake_basin"], "4":["cold_high_cells"],
                 "7":["river_crossing"], "8":["long_river"] } },
    { "id": "s25_dry_interior", "source": "s25_worlds.json",
      "tests": { "2":[], "3":["dry_and_lake_basin"], "6":["corridors_and_sites"] } } ] }
```

The world ids are **S-2.5's** (no new worlds, user decision 2026-11-17); preset, seed and size come from `s25_worlds.json`, so there is one place to re-pick a seed. Prechecks are Feature 2's pure predicates (`S26Prechecks`), run on `WorldGenerationComplete` over the `HydrologyTerrain` built by A5's `BuildHydrologyTerrain` and cached with the world revision. A failed precheck makes only the **tests that need it** `InvalidWorld`; the stage still runs and shows the failing id, measured value and margin. A world with no basin table (version 1 import) fails `dry_and_lake_basin` and says "lakes-only world: Test 3 needs a world with a basin table".

Verdict-eligible = `size == Small && curated`. Another size or a non-curated pick is "exploration: no verdict" and runs no prechecks.

### `S26RiversFloodsModule`

```cpp
class S26RiversFloodsModule : public Dia::ApplicationFlow::SimModule {
public:
    bool LoadWorld(Dia::Core::StringCRC world);          // curated id; sets S-1 inputs, waits for IsReady
    bool SetMode(S26Mode mode);                          // reloads the world, clears history and the series
    S26Mode GetMode() const;
    const S26WorldStatus& GetWorldStatus() const;        // id, preset, seed, size, revision, prechecks, verdict-eligible
    const DynamicArrayC<CheckResult>& GetLiveChecks() const;   // Feature 2's checks over the live series
    // Features 3-5 attach their pieces here (runner, scenarios, export).
};
```

- **World load** = set S-1's preset, seed and size through S-1's commands and wait for `IsReady(revision)` on both S-1 and `HydrologyModule` (SD-S1-010). The module never touches `GetCurrentWorld()` beyond reading terrain for the prechecks.
- **Series recording.** After each applied week (`HydrologyUpdated`), the module calls Feature 2's `SummariseWeek` into a live `S26Series` (unbounded to 5,200 weeks, about 3 MB at Small) and re-evaluates the checks whose requirements are now met. The series clears on world load, mode switch, preset load and parameter changes that restart the water.
- **Launch-out hint.** At `DoStart`, if F-3's reader exists (soft) and a hint is present, load that bundle's terrain instead of the default; absent or unreadable logs a Warning and uses the default. Until F-3 exists this is a seam with a fake reader.

### Modes

- **Scripted** (default): `HydrologyModule` runs on `ScriptedWaterInput` through `SetFixedInput`; no A4, no RNG. A preset is loaded with `cow.hydro.preset name=<n>` (the scripted phases play) and the stage is **paused at week 0** after it applies, so the setup is readable.
- **Coupled:** `WeatherModule` is in the order; `cow.hydro.preset name=<n> mode=coupled` applies the fills only and A4 supplies the weather. The card shows "weather: A4 live". **Blocked on A4 Features 1–3 and 8.**

### Time controls

- GameSpeedController's Normal/Fast/Fastest and pause, nothing new.
- **Step one week** = `cow.worldstep.step` (World Step Feature 2), then pause.
- **Step one year** = `cow.s26.year_step`: runs 52 applied weeks at Fastest and pauses, stopping early on cancel, a world reload or an Error. Progress shows in the card. Behaviours here (lakes, glaciers, reachability) are multi-year, so this is the control that makes S-2.6 usable.

### Forced wet and dry years (understanding tools, never verdicts, CH-S26-04)

`cow.s26.forced_year wet|dry` runs 52 weeks:
- **Scripted:** `ScaledWaterInput` wraps the current `ScriptedWaterInput` and scales its **rain** by 1.75 (wet, the middle of the preset's 1.5–2×) or 0.2 (dry); snow and glacier input are untouched. Feature-local and pure.
- **Coupled:** posts A4's map-wide WETTER or DRIER influence; A4's per-field weekly caps apply, and the card says "bounded by A4 caps".

### Climate-cycle scrubber (Coupled only)

`cow.s26.cycle` pins A4's `ice_age_40y` or `oscillation_4y` to a phase `u` in [0, 1) (or `run` to let it run), then re-runs from spin-up. Disabled in Scripted with the reason "Scripted mode has no A4". **Blocked on A4 Feature 8.**

### Tools hosted

| Tool | Wiring |
|---|---|
| Layer picker and legend | DiaCellLayers' picker over the 8 `hydro.*` layers plus A5's `hydro.river_state`, `hydro.basins`, `hydro.flow_dir`; flow arrows on top (A5's overlay) |
| Click-to-inspect | CellField click selects a cell; A5's detail rows (every `HydroCell` field, basin, counters, this week's per-pass deltas) |
| Presets | The four A5 presets via `cow.hydro.preset` |
| Step-back | `cells.history.view` and `cells.snapshot.restore`; disabled with "history driver not built" until `HistoryDriverModule` exists, and "no history yet" before the first sample. Needs only that a flood can be scrubbed from peak back to onset (Feature 5 proves it) |
| Mode and size | `cow.s26.mode`, `cow.worldgen.set_size` (via `cow.s26.world`) |

### The S26 panel card

An `IDebugDomain` card (`DIA_DEBUG`), accent from `DebugGroupAccents`, description ≤ 80 characters, showing: world id, preset, seed, size, revision and verdict-eligible or "exploration"; each precheck with value and margin (red when failed); mode and world step mode; week, year, season, speed; preset buttons and the Step week / Step year controls; Forced wet / dry; the cycle scrubber; **readouts**: water totals by store, the **balance-error gauge**, `cow.hydro.step_ms` for the size and the largest river-tree size, the five A3-facing counters with "would trigger" lines (104 / 52 / 26 / 260 weeks), and the live check lines (Feature 2) labelled **understanding, not verdict**; history status; the last session-hint source. It **posts commands only** and gets SimPU values through a published stats struct, not a cross-thread read.

**HTML mockup first (project rule):** `mockups/s26-card.html` extends S-2.5's `s25-card.html` with every control above, a failing-precheck state, an exploration-world state, a history-unavailable state, a Coupled-blocked state (A4 absent), and a flood mid-run. It is **plan Task 1**, signed off by the user before any card code (recorded in this spec once given, never re-asked).

### Commands (6 names, registered all-or-nothing)

Sim-hosted through `SimQueryHostModule` with mandatory help, category `s26`:

| Name | Kind | Arguments |
|---|---|---|
| `cow.s26.world` | command | `{"action":"load","world":"<id>"}`, `{"action":"size","size":"Small\|Standard\|Large"}` (thin alias for `cow.worldgen.set_size`) |
| `cow.s26.mode` | command | `{"mode":"scripted\|coupled"}`; Coupled refused with a reason when A4 is absent |
| `cow.s26.year_step` | command | `{"action":"start\|cancel"}` |
| `cow.s26.forced_year` | command | `{"kind":"wet\|dry"}` |
| `cow.s26.cycle` | command | `{"cycle":"ice_age_40y\|oscillation_4y","u":0.0..1.0}` or `{"action":"run"}`; Coupled only |
| `cow.s26.status` | query | world, prechecks with values, verdict-eligible, mode, week/year/season, history availability, hint source, live checks |

Features 3 and 4 add `cow.s26.run`, `cow.s26.report`, `cow.s26.baseline`, `cow.s26.livesmoke` and reuse `cow.scenario.*`. The budget is the per-PU sim-hosted cap, **192 in code** (`SimQueryHost::kMaxHandlers`). Registration follows Feature 3's `S26CommandRegistrar` so a forced failure of any name leaves none behind.

### Instrumentation

Logs on channel `S26`: Info on stage start, world load request and completion with revision and duration, precheck results, mode switch, year-step start and end, forced year, hint used; Warning on a failed precheck, an unreadable hint, a refused mode switch or cycle, Coupled refused; Error on a failed command registration. `DIA_TRACE_ZONE` around world load, prechecks and the per-week series summary. `DIA_PROFILE_SCOPE` around `SummariseWeek` and live evaluation (per-week work). Health (`IHealthReporter`): Degraded on a failed precheck; Failing never from this module. Metrics: `s26.world_ready_ms`, `s26.check_eval_ms` (per-week live evaluation) with `MetricInfo`.

## Public API

New: the files above; `S26WorldStatus`, `S26Mode`; `s26_worlds.json`; `mockups/s26-card.html`; tests under `Tests/GoogleTests/CoW/S26/`. Edited: F-1's menu data, `CoW.vcxproj(.filters)` via `dia docs vcxproj-add`. No Dia or Draoi files are added or edited.

## Acceptance Criteria

### Stage and manifest
1. `dia validate manifest` accepts the stage; the manifest lists the modules in the order above and contains no `EcologyModule`, `SuccessionModule` or `SaveLoadModule`.
2. The stage appears in F-1's debug menu and enters without error; exiting and re-entering gives a clean second visit (no leaked registrations, jobs or names).
3. In Scripted mode, health after start has no Failing cause, and the card states the input source ("scripted preset driver").

### Worlds and prechecks
4. The default world loads and A5 is `IsReady` for its revision with no manual step; the weekly series starts at week 0.
5. A failed precheck marks only the tests that need it `InvalidWorld`, shows the id, value and margin, sets Degraded and still runs.
6. A version-1 (lakes-only) world fails `dry_and_lake_basin` with the stated message and does not stop the stage.
7. `cow.s26.world load` of each curated id regenerates, re-initialises A5, re-runs the prechecks and updates the card; an unknown id returns an error listing valid ids and changes nothing.
8. Changing size to Standard or Large marks the world "exploration: no verdict" and re-runs prechecks; changing back to Small restores verdict-eligible. A non-curated pick runs no prechecks and is "exploration".

### Modes and time
9. The stage starts Scripted and paused at week 0 after a preset applies; `cow.s26.mode` to Coupled is refused with a reason while A4 is absent and changes nothing.
10. `cow.worldstep.step` advances exactly one week and pauses; refused with a reason before the world is ready.
11. `cow.s26.year_step` runs exactly 52 applied weeks and pauses; `cancel` stops it within one week; a world reload cancels it.
12. Switching mode reloads the world, clears the series and history, and the order reads `["hydro"]` or `["weather","hydro"]` as stated.
13. `ScaledWaterInput` scales only rain: with scale 1.0 it equals the wrapped driver bit for bit; with 0.2 rain is 0.2× and snow, glacier input and every other field are unchanged. A forced year lasts 52 weeks.
14. (Coupled, blocked on A4) forced years post the A4 influence and the card states "bounded by A4 caps"; `cow.s26.cycle` pins to `u` and re-runs from spin-up, and is refused in Scripted with the reason.

### Tools
15. A click selects a cell and shows A5's detail rows; clicking off the map clears it.
16. The layer picker lists the eight `hydro.*` layers and A5's three debug layers with their hotkeys; no hotkey clash is logged.
17. With `HistoryDriverModule` present, step-back scrubs the retained weeks and restores; without it the control is disabled with the reason.

### Live checks and readouts
18. The live check lines show `Pass`/`Fail`/`Pending`/`CoupledOnly` per Feature 2's states, are labelled "understanding, not verdict", and a check whose weeks have not run shows `Pending` with the reason.
19. The balance gauge, store totals, step_ms for the size, largest river-tree size and the five counters (with "would trigger" lines) come from the published stats struct and equal `cow.hydro.state` for the same week.

### Launch-out
20. A valid session hint loads that bundle's world; an absent hint uses the default; an unreadable one logs a Warning and uses the default (fake reader until F-3 exists).

### Commands and card
21. All names register with non-empty help; a forced failure of any leaves **no** names behind.
22. Every card control corresponds to a command (a test walks the control list against the registered commands); the card holds no direct calls into the module.
23. The card's description is ≤ 80 characters; its values come from a stats struct (no cross-thread reads).

### Smoke
24. A DiaAutomation smoke scenario enters the stage, waits for ready, loads `melt_pulse`, steps 4 weeks, year-steps once, switches to a size and back, and exits with no Error logs.
25. Scripted weekly step CPU at Small is reported against A5's budget (reported, not gated in Debug).

## Files Touched

As Public API. Also on approval: the system spec's Features table (this row's spec link). No Dia or Draoi files.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Stage, module, forced-year driver are feature-local (S26 prefix); only the scenario host and `CheckRegistry` are shared |
| AD-005 | One stage per testbed | One stage; modes and presets are panel state |
| AD-001 | Ask permission before Dia edits | None; all used Dia APIs are built |
| SD-S1-010 | World source hand-off | The module sets S-1's inputs and waits on `IsReady`; never generates itself |
| SD-S1-006 | Small map only for verdicts | Verdict-eligible flag, exploration label |
| PU boundaries are streams | No cross-PU mutex reads | The card reads a published stats struct |
| Editor-actions rule | Every tool action is a command | Criterion 22 |
| HTML mockup rule | Mockup before any UI | Plan Task 1, user sign-off |

## Test Plan

Pure parts first (`ScaledWaterInput`, world table loader, status model), then the module against fakes (fake S-1 world source, fake hydrology module, fake session-hint reader, fake World Step), then one DiaAutomation smoke on the real stage. TDD order: world table loader, `ScaledWaterInput`, module world loading, mode switch, year-step, live checks wiring, tools wiring, commands. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. The stage is verified for water content by Feature 3's report (cell data over pixels), not screenshots; one screenshot per feature remains a taste check.

## Open Design Questions

1. **Can World Step switch its order at runtime?** The mode switch changes the order and adds or drops `WeatherModule`. World Step Feature 2 may only support a launch-time order. If so, the switch becomes a stage restart with the other config (one extra step for the user), or a CoW-Shared World Step amendment. Decide when the first plan task has checked.
2. **`ScaledWaterInput` scales rain only.** A "dry year" with full snowmelt still feeds rivers, which may make `forced dry` look mild. If the first use shows that, scale snow melt too, or let the user pick; the verdict never depends on it (CH-S26-04).
3. **Year-step cost.** 52 weeks at Fastest depends on GameSpeedController's cap (S-2.5 saw about 12 s per 52 weeks). If that is too slow for a 20-year ice age by hand, a "skip" that runs the sim headlessly on the worker and swaps the result in would be a separate feature, not assumed here.

## Status

**Status:** `Approved` (2026-11-24). No plan. **Plan Task 1 is the HTML mockup.**
