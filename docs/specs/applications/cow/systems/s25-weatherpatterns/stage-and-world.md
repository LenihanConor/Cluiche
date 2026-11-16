# Feature Spec: S-2.5 Stage & World

**Parent:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md (Feature 1)
**Status:** Approved (2026-11-16)
**Depends on:** A4 Features 1–6 built (the module, the debug surface, `ComputeWaterDistance` from Feature 6, `PrevailingDirection` from Feature 5's exports); World Step Features 1–3 and its `SetDeferred` amendment below; S-1 Features 1 and 8 (built); DiaCellLayers, DiaCellHistory, GameSpeedController, F-1 (debug menu and debug host), DiaAutomation (all built). **Soft:** CoW-Shared `HistoryDriverModule` (S-3.5 Feature 1; the history, timeline and step-back parts are gated on it) and F-3's session hint (Feature 2 of F-3; the launch-out part is gated on it).
**Consumed by:** S-2.5 Features 2–5 (the checks, runner, scenarios and graphs all attach to this stage), S-3.5's launch-outs.

## Summary

The S-2.5 stage: a `.diastage` and `.diaapp` that run A4 **alone** on a hand-chosen S-1 world, a minimal `S25WeatherModule` and its panel card, the two curated worlds with prechecks, a world and size picker, time controls including a single-week step, the A4 tools wired in (layer picker, timeline, step-back, click-to-inspect, boundary switch), and the F-1 menu entry. It contains no checks, no headless runner, no scenarios and no graphs: those are Features 2 to 5, which plug into the module this feature creates.

## Goals

1. Launch from F-1's debug menu into a stage that loads `s25_coast_range` at Small and shows A4's weather within seconds, with no ecology, succession or save module.
2. The two curated worlds are named data (preset + seed + size), each with a **precheck** that says "invalid world" when S-1 has changed enough that the world lost the features its tests need.
3. The stage runs the world step **synchronously** by default and can be switched to the **deferred** path live (decided 2026-11-16), so S-2.5 exercises both, with identical state per applied week.
4. A single "step one week" control makes one week observable at a time.
5. Every action on the S25 card is a registered command; the card posts the same commands.
6. The stage honours F-3's session hint so S-3.5's "Open in S-2.5" continues with the same world.

## Non-Goals

- The nine checks, check lines, baseline and report (Features 2 and 3), scenarios and walkthrough (Feature 4), time-series graphs (Feature 5).
- Generating worlds. S-1 owns generation, size and presets; this feature calls S-1's commands.
- Tuning A4's parameters or writing them back.
- Saving. No `SaveLoadModule` in this stage.
- A4's debug domain, layers, overlay and `cow.weather.*` (A4 Feature 5). This stage hosts them.
- Player-facing weather visuals (V-1).

## Design

### Files and scaffolding

Created with `/new-cow-stage`:

```
Cluiche/Assets/CoW/Stages/S25WeatherPatterns/
    s25_weather_patterns.diastage
    misc/ApplicationFlow/s25_weather_patterns.diaapp
    s25_worlds.json                      envelope "cow.s25_worlds", version 1 (curated world table)
Cluiche/CoW/Modules/S25WeatherModule.h/.cpp        (S25 prefix, feature-local, AD-002)
Cluiche/CoW/Shared/Weather/S25/S25WorldPrechecks.h/.cpp   (pure)
```

Registered in F-1's debug menu. `dia validate manifest` accepts the stage and module list.

### Module list (manifest order)

```
MessageBusHostModule, SimQueryHostModule, GameSpeedControllerModule, RandomStreamModule,
WorldGenerationModule            (config: preset + seed + size from s25_worlds.json default)
CellLayersModule, CellHistoryModule, HistoryDriverModule (soft; absent until S-3.5 F1)
DeferredStepModule
WorldStepModule                  (config: order ["weather"], apply_after_days 7, deferred false)
WeatherModule                    (config: parameters file, spinup_weeks 52, history block, wind_overlay)
S25WeatherModule
```

`DeferredStepModule` is present even though the stage starts synchronous, because the live switch needs it. No `EcologyModule`, `SuccessionModule` or `SaveLoadModule`. A4 therefore uses its table baseline and reports **Degraded**; that is expected here, the card says "table baseline (no ecology), expected", and a Degraded health never fails a smoke run.

`WeatherModule`'s `history` block (read by `CellHistoryModule`, capped at 255 characters) is `{"bands":["Week"],"track":["weather.precipitation","weather.temperature","weather.ground_water"],"cellBudgetMB":16}`. The system spec's `history_weeks: 13` no longer exists (A4 removed it); 13 weeks of step-back is what the 16 MB budget gives at the tracked layers, verified by a test rather than assumed.

### Worlds

`s25_worlds.json`:

```json
{ "format": "cow.s25_worlds", "version": 1,
  "default": "s25_coast_range",
  "worlds": [
    { "id": "s25_coast_range",  "preset": "<S-1 preset>", "seed": 0, "size": "Small",
      "prechecks": ["ocean_present","coast_band","ridge_crosses_wind","lowlands_downstream"] },
    { "id": "s25_dry_interior", "preset": "<S-1 preset>", "seed": 0, "size": "Small",
      "prechecks": ["interior_far_from_water"] } ] }
```

The preset names and seeds are filled in by the **world scout** task: a small pure tool (`S25WorldScout`, a test-time utility run through GoogleTests with `--filter=*Scout*` or a `dia` command, not shipped) that generates candidate seeds per S-1 preset, runs the prechecks and prints the passing ones with their margins. The user picks one per world from that list (a taste check) and the choice is recorded in this file. Picks are per **Small**; only Small has a verdict (system spec). Loading a curated world at another size via the size picker marks it **"exploration: no verdict"** on the card.

**Prechecks** (`S25WorldPrechecks`, pure, over a `WeatherTerrain`; thresholds are data in `s25_worlds.json`):

| Id | Passes when |
|---|---|
| `ocean_present` | at least `minOceanCells` (default 200) ocean cells |
| `coast_band` | at least `minCoastCells` (default 40) land cells within 2 hex steps of water |
| `ridge_crosses_wind` | there exists a windward/leeward cell pair across a ridge with elevation ≥ 0.6, the pair lying along the prevailing wind for their latitude (`PrevailingDirection`), at least 3 steps apart, with a ridge-crest of at least `minRidgeCells` (default 6) |
| `lowlands_downstream` | at least `minLowlandCells` (default 20) cells below elevation 0.4 within 8 steps downwind of the ridge |
| `interior_far_from_water` | at least `minInteriorCells` (default 30) cells at distance ≥ 8 from any water cell |

Distance comes from A4 Feature 6's `ComputeWaterDistance`. Each precheck returns pass/fail plus its **measured value and margin**, so a world that barely passes is visible. Prechecks run on `WorldGenerationComplete` and are cached with the world revision.

### `S25WeatherModule`

A SimModule that owns the stage-level state and nothing else yet:

```cpp
class S25WeatherModule : public Dia::ApplicationFlow::SimModule {
public:
    bool LoadWorld(Dia::Core::StringCRC world);          // curated id; resolves to S-1 preset/seed/size and requests generation
    const WorldStatus& GetWorldStatus() const;           // id, preset, seed, size, revision, prechecks, verdict-eligible flag
    S25StepMode GetStepMode() const;                     // Sync | Deferred
    // Features 2-5 attach their pieces here (checks, runner, scenarios, graphs).
};
```

- **World load** = set S-1's generation inputs (preset, seed, size) through S-1's commands and wait for `IsReady(revision)` / `WorldGenerationComplete`, per the S-1 hand-off (SD-S1-010). The module never touches `GetCurrentWorld()` beyond reading terrain for the prechecks.
- **Size picker** calls `cow.worldgen.set_size <Small|Standard|Large>`; A4 re-spins itself (A4 Feature 3), the prechecks re-run, and the verdict-eligible flag is `size == Small && curated`.
- **World picker** lists the curated worlds, other S-1 presets, and `.wld.json` files S-1 can load; a non-curated pick is "exploration: no verdict" and runs no prechecks. `world_bundle` (terrain only) joins when F-3 is built.
- **Launch-out hint.** At `DoStart`, if F-3's session hint reader exists (soft) and a hint is present, load that bundle's terrain instead of the configured default; absent hint or an unreadable one logs a Warning and falls back to the default. Until F-3 exists this is a seam with a fake reader in tests.

### Step mode and time controls

- **Default is synchronous** (`deferred: false` on `WorldStepModule`): weather runs inline in the due tick. GameSpeedController supplies the existing speed presets and pause; nothing new is invented.
- **Step one week** = `cow.worldstep.step` (World Step Feature 2) in synchronous mode: one inline step, then the stage pauses. In deferred mode the control is "advance 7 days" (the apply lands at +k), labelled as such.
- **Live mode switch** (decided 2026-11-16): the card toggles Sync ↔ Deferred. This needs a CoW-Shared amendment to World Step Feature 2: `WorldStepModule::SetDeferred(bool)`, allowed **only when no step is in flight and no pending week exists**, otherwise refused with a reason; and `cow.worldstep.step` gains `{"action":"kick"|"set_mode","mode":"sync"|"deferred"}`. Switching to deferred registers the channel on demand; switching to sync unregisters it. State at the switch is unchanged. S-2.5's **mode-equivalence test** runs the same weeks in each mode and asserts equal committed state per applied week (World Step's oracle, now also at stage level).
- Pause and speed changes use GameSpeedController's own commands; the card shows the current speed and day.

### A4 tools wired to the stage

| Tool | Wiring |
|---|---|
| Layer picker and timeline | DiaCellLayers' shared picker and DiaCellHistory timeline hosted on the S25 card; hotkeys come from the layer schema (Snow = K) |
| Overlay toggles | A4's wind overlay switch through the panel (no new command) |
| Step-back | The button calls `cells.history.view` and `cells.snapshot.restore` (DiaCellHistory). Disabled with the reason "history driver not built" until `HistoryDriverModule` exists, and with "no history yet" before the first sample |
| Click-to-inspect | A click on the CellField view selects the cell (DiaCellLayers selection); A4's detail rows plus biome and elevation appear |
| Boundary switch | `cow.weather.params {"action":"set","key":"boundary.edge_inflow","value":<0|1>}`, effective next week |

### The S25 panel card

An `IDebugDomain` card (`DIA_DEBUG`), accent from `DebugGroupAccents`, description ≤ 80 characters, showing: world id, preset, seed, size, revision; verdict-eligible or "exploration"; each precheck with value and margin (red when failed, "invalid world: re-pick the seed"); step mode and the Sync/Deferred switch; day, week, season, speed; the Step-one-week control; A4's baseline note; history status; the last session hint source. It **posts commands only** and gets SimPU values through a published stats struct, not a cross-thread read.

**HTML mockup first (project rule):** `mockups/s25-card.html` shows the S25 card with every control above, the A4 layer picker, timeline and overlay toggles around the CellField view, and one failing-precheck state, one exploration-world state, and one history-unavailable state. It is **plan Task 1**, signed off by the user before any card code. The card's layout is whatever the mockup fixes.

### Commands (2 names, registered all-or-nothing)

Sim-hosted through `SimQueryHostModule` with mandatory help, category `s25`:

- `cow.s25.world` (command): `{"action":"load","world":"<id>"}` and `{"action":"size","size":"Small|Standard|Large"}` (a thin alias for `cow.worldgen.set_size` so the card has one entry point). Unknown world or size returns an error listing valid names.
- `cow.s25.status` (query): world, prechecks with values, verdict-eligible flag, step mode, day/week/season, history availability, hint source.

Features 3 and 4 add their own names (`cow.s25.run` and friends, `cow.scenario.*`); the budget is the per-PU sim-hosted cap, **192 in code today** (`SimQueryHost::kMaxHandlers`). The 64 quoted in earlier A4 and World Step text is stale (checked 2026-11-16).

### Instrumentation

Logs on channel `S25`: Info on stage start, world load request and completion with revision and duration, precheck results, mode switch, hint used; Warning on a failed precheck, an unreadable hint, a refused mode switch; Error on a failed command registration. `DIA_TRACE_ZONE` around world load and prechecks. Health (`IHealthReporter`): Degraded on a failed precheck (the stage still runs); none otherwise. Metric: `s25.world_ready_ms` (time from load request to ready) with `MetricInfo`.

## Public API

New: the files above; `S25WorldStatus`, `S25StepMode`; `s25_worlds.json`; `mockups/s25-card.html`; tests under `Tests/GoogleTests/CoW/S25/`. Edited: F-1's menu data, `CoW.vcxproj(.filters)` and the CoW project filters via `dia docs vcxproj-add`. **World Step Feature 2** gains `SetDeferred` and the `cow.worldstep.step` `action` argument (CoW-Shared, no Dia change).

## Acceptance Criteria

### Stage and manifest
1. `dia validate manifest` accepts the stage; the manifest lists the modules in the order above and contains no `EcologyModule`, `SuccessionModule` or `SaveLoadModule`.
2. The stage appears in F-1's debug menu and enters without error; exiting and re-entering gives a clean second visit (no leaked registrations, jobs or names).
3. Health after start is Degraded for the table baseline only; no Unhealthy cause is present, and the card states the baseline source.

### Worlds and prechecks
4. The default world loads and A4 is `IsReady` for its revision with no manual step.
5. Each precheck passes on a hand-built terrain that has the feature and fails (naming itself) on one that lacks it; each returns its measured value and margin.
6. `ridge_crosses_wind` fails when the ridge exists but lies parallel to the prevailing wind (the cross-wind condition is the point).
7. A world whose precheck fails shows "invalid world" with the failing id, sets Degraded, and still runs.
8. The world scout finds at least one passing seed per curated world for **each** of the two worlds, and the chosen preset/seed pass all their prechecks on a fresh generation (a regression test against S-1 changes).
9. `cow.s25.world load` of each curated id regenerates, re-spins A4, re-runs prechecks and updates the card; an unknown id returns an error listing the valid ids and changes nothing.
10. Changing size to Standard or Large marks the world "exploration: no verdict", re-runs prechecks, and changing back to Small restores the verdict-eligible flag.
11. A non-curated world pick runs no prechecks and is marked "exploration".

### Step mode and time
12. The stage starts in Sync; weather applies in the due tick and `GetSnapshot().GetWeek()` advances once per game week.
13. `cow.worldstep.step` in Sync advances exactly one week and pauses; refused with a reason before the world is ready.
14. Switching to Deferred succeeds when idle and is refused with a reason while a step is in flight or a week is pending; switching back works; neither switch changes committed state.
15. **Mode equivalence:** the same 52 weeks run in Sync and in Deferred give identical committed fields after the same number of applied weeks (stage-level oracle).
16. In Deferred mode the step control reads "advance 7 days" and the apply lands at +k.

### Tools
17. A click on the view selects a cell and shows A4's detail rows plus biome and elevation; selecting off the map clears it.
18. The boundary switch changes `boundary.edge_inflow` and takes effect on the next committed week, not before.
19. With `HistoryDriverModule` present, step-back scrubs the retained weeks and restores; with it absent the control is disabled with the reason. The 16 MB history budget retains at least 13 weeks of the three tracked layers at Small.
20. The layer picker lists A4's seven layers with their hotkeys; no hotkey clash is logged.

### Launch-out
21. A present, valid session hint loads that bundle's world instead of the default; an absent hint uses the default; an unreadable or invalid hint logs a Warning and uses the default (a fake reader until F-3 exists).

### Commands and card
22. Both names register with non-empty help; a forced failure of the second leaves **no** names behind.
23. Every card control corresponds to a command (a test walks the control list against the registered commands); the card holds no direct calls into the module.
24. The card's description is ≤ 80 characters; its values come from a stats struct (no cross-thread reads).

### Smoke
25. A DiaAutomation smoke scenario enters the stage, waits for ready, steps 4 weeks, switches to Deferred and back, and exits with no Error logs. (A Degraded health is expected.)
26. Sync-mode weekly tick CPU at Small is reported against A4's 4.2 ms Small budget (reported, not gated in Debug).

## Files Touched

As Public API. Also edited on approval: the S-2.5 system spec (stale items: A4 `history_weeks: 13`, `cow.weather.step_back`, the 16-name list, the "weekly tick ≤ 16 ms" line, the A4 hosting description: replaced by pointers to A4's 7 names, the tracked-layers config, A4's CPU budgets and the step-mode decision), World Step Feature 2 (`SetDeferred`, `cow.worldstep.step` `action`). No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| AD-002 | Four-tier dependency model | Stage, module and prechecks are feature-local (S25 prefix); nothing here is shared except through Feature 4 later |
| AD-005 | One stage per testbed | One stage, `S25WeatherPatterns` |
| AD-001 | Ask permission before Dia edits | None; all used Dia APIs are built |
| SD-S1-010 | World source hand-off | The module sets S-1's preset/seed/size and waits on `IsReady`/`WorldGenerationComplete`; never generates itself |
| PU boundaries are streams | No cross-PU mutex reads | The card reads a published stats struct |
| Editor-actions rule | Every tool action is a command | Criterion 23 |
| HTML mockup rule | Mockup before any UI | Plan Task 1, user sign-off |

## Test Plan

Pure parts first (prechecks, world table loader, status model), then the module against fakes (fake S-1 world source, fake session-hint reader, fake `WorldStepModule` for the mode switch), then one DiaAutomation smoke on the real stage. The scout runs as a test-time utility and its output is reviewed by the user. TDD order: world table loader, prechecks, module world loading, mode switch, tools wiring, commands. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. The stage is verified for weather content by Feature 3's report (cell data over pixels), not by screenshots; one screenshot per feature remains a taste check.

## Open Design Questions

1. **`HistoryDriverModule` is a hard prerequisite for step-back, the timeline and Feature 5's graphs but belongs to S-3.5 Feature 1.** This feature is built so everything else works without it, and step-back shows a disabled control. If that is too much of the experience to be missing for long, the driver (a small part of S-3.5 Feature 1, CoW-Shared) should be built **before** S-2.5 rather than with S-3.5. Decide when scheduling.
2. **Is Small-only the right verdict size for the curated worlds?** At Standard and Large the same preset + seed generates different geography, so the two curated picks are not valid there. The system spec accepts this (one Large headless run for performance only). If you want verdicts at all three sizes, the scout must pick six worlds and the report grows; that is a feature, not a tweak.
3. **Does the live Sync ↔ Deferred switch earn its complexity?** It adds a CoW-Shared amendment (`SetDeferred`) and its idle-only rule. The alternative, a launch-time choice, would need a second stage entry or a config override. The switch was chosen because the deferred smoke is the main reason to watch weather in S-2.5; revisit if `SetDeferred` proves awkward in World Step.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Mockup path.** `s25-weatherpatterns/mockups/s25-card.html` (a `mockups` folder inside the system's spec folder, as S-3.5 does), not `docs/mockups/`.
2. **World scout.** Seeds 1..200 per S-1 preset (the preset list is S-1's), a 60 s budget per world, candidates ranked by their smallest precheck margin, the top 5 printed with every precheck value. The `<S-1 preset>` placeholders in `s25_worlds.json` are filled by the user's pick.
3. **Criterion 19 (13-week retention)** needs `CellHistoryModule` **and** `HistoryDriverModule` (the driver does the sampling); it is gated with the step-back tasks.
4. **Criterion 26 (CPU report)** goes to the log and `cow.s25.status` (see Feature 3's amendment 4).
5. **Command registration** uses Feature 3's `S25CommandRegistrar` (Feature 3 amendment 2).

## Status

**Status:** `Approved` (2026-11-16). Plan: [stage-and-world.plan.md](stage-and-world.plan.md) (written 2026-11-16, not started). **Plan Task 1 is the HTML mockup.**
