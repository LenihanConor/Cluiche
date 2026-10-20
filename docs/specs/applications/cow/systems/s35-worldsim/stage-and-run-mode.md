# Feature Spec: S-3.5 Stage and Run Mode

**Parent:** @docs/specs/applications/cow/systems/s35-worldsim/s35-worldsim.md
**Mockup:** @docs/specs/applications/cow/systems/s35-worldsim/mockups/s35-world-sim.html
**Builds on:** @docs/specs/applications/cow/systems/f3-worldassets/world-assets-module-and-bundle-load.md, @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md, @docs/specs/applications/dia/systems/diacellhistory/recorder-and-tiers.md, @docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md

**Gameplay Domains:** debug

## Summary

The S-3.5 stage and its default mode. It delivers:

- the **`S35WorldSim` stage**: `.diastage`, its `.diaapp` module composition on SimPU, and the Boot transition (AD-004);
- the initial **bundle load** through F-3 (session hint, stage config, game default);
- the **shared cards** hosted together: World Assets, Layers, Selection and Timeline;
- **`S35WorldSimModule`**: the mode switch (Author, Run, Regression), the S35 panel card, and a session label for T-1;
- the CoW-Shared **`HistoryDriverModule`** that turns game-clock ticks into DiaCellHistory samples, shared by every testbed that records history.

Run mode is the default: load a bundle and watch every layer run, select areas, scrub and restore. The Author step rail and the Regression runner are Features 2 and 3; this feature gives them the mode machine and the card they plug into.

## Problem

Each layer has its own testbed, but nothing runs them together on one world, and nothing is the place to see whether the whole world behaves. The stage is mostly wiring (S-3.5 adds no simulation), but the wiring has real decisions: which modules, in what order, who samples history and when, how a bundle is chosen at start, and what T-1 should show when it connects.

## Goals

1. **One stage, every layer.** Terrain, weather, ecology (and succession when built) live on one world, with the shared cards.
2. **The right world at start.** A fresh session hint, else the stage's bundle, else the game's `default_world`, else an empty world with a clear prompt.
3. **Correct history.** Samples are taken once per game week at a safe point, labelled by calendar, with the world id the editors need.
4. **Modes without hidden state.** Switching modes is explicit, refuses to lose unsaved work, and is visible to T-1.
5. **No new simulation.** Everything simulated comes from existing modules.

## Non-Goals

- The Author step rail and preview clock (Feature 2).
- The regression checks, runner and report (Feature 3).
- Launch-outs to S-2.5 and S-3 (Feature 4).
- Real art, dragons, or player-facing modes.
- Adopting the history driver in S-2.5 and S-3 (they adopt it as they are built).

## Architecture

### Placement

Feature-local for the stage (AD-002): code in `Cluiche/CoW/Modules/` (S35 prefix), assets in `Cluiche/Assets/CoW/Stages/S35WorldSim/` (`s35_world_sim.diastage`, `misc/ApplicationFlow/s35_world_sim.diaapp`), following the F-2 stage's layout. The stage is created with the `/new-cow-stage` skill from the testbed roster. **`HistoryDriverModule` is CoW-Shared** (`Cluiche/CoW/Shared/History/`), because S-2.5 and S-3 need the same driver.

### Module composition (SimPU)

In `.diaapp` order (earlier modules update first within a frame):

```
SaveLoadModule              E1
HistoryDriverModule         CoW-Shared (before GameSpeedController, like E1: samples at the frame boundary)
GameSpeedControllerModule, RandomStreamModule
WorldAssetsModule           F-3 (participants: below)
WorldGenerationModule       S-1  (terrain participant)
WeatherModule               A4   (weather participant)
EcologyModule               A1   (ecology and start-state participants)
SuccessionModule            A3   (when built)
ValueSchemaModule           DiaValueSchema
CellLayersModule            DiaCellLayers (viewer domain)
CellHistoryModule           DiaCellHistory (recorder and its domain)
S35WorldSimModule           mode machine, S35 card, session label
```

Main and render PUs are the baseline CoW set. A module that does not exist yet (A3) is simply absent; Features that need it switch on when it is built.

### `.diastage` config

```json
{ "world_bundle": "@default",
  "history": { "track": ["ecology.producers_total", "ecology.group.herbivore", "weather.precipitation"] },
  "s35": { "mode": "Run" } }
```

`"@default"` is F-3's token for `cow.diagame` `default_world`. (The system spec excerpt wrote `"default"`; amended.) The `regression` block belongs to Feature 3.

### Initial bundle

`WorldAssetsModule`'s initial-bundle picker (F-3 Feature 2) runs at start: a session hint no more than 60 seconds old, else the stage's `world_bundle`, else nothing. If `default_world` is not set, or the bundle fails to load, the stage starts with an empty world, the S35 card shows "No world loaded: load a bundle, or use Author mode", and the World Assets card shows the reason (F-3's `failed` or `partial` status). After a successful load the clock is **paused** (A2 EC-03), and the Play controls are the GameSpeedController card's.

### Shared cards

S-3.5 adds no card of its own for these; it includes the modules that register them:

| Card | Registered by | From |
|---|---|---|
| World Assets | `WorldAssetsModule`'s debug domain | F-3 Feature 5 |
| Layers (picker, legend, hover) and Selection (tools, query, inspector) | `CellLayerViewerDomain` | DiaCellLayers Features 1 to 4 |
| Timeline (scrub, events, snapshots, delta) | `CellHistoryModule`'s domain | DiaCellHistory Features 2 to 4 |
| Game speed | `GameSpeedDebugDomain` | A2 |
| Layer-specific debug (S-1 sliders, A4 and A1 domains) | their own modules | their systems |

The **S35 card** (below) is the stage's own card. Snapshot buttons in the Timeline card are disabled with a reason until the CoW snapshot adapter over E1 exists.

### `S35WorldSimModule` and the mode machine

States `Author`, `Run`, `Regression`; the start mode is `s35.mode` (default `Run`).

| Command | Effect |
|---|---|
| `cow.s35.mode <Author\|Run\|Regression>` | Switch mode (rules below) |
| `cow.s35.status` | JSON: stage, mode, bundle id or `custom` or `none`, world id, clock state, history sample count, last mode-change time |

- **Switching.** Run and Author switch freely. Switching **to Regression** pauses the clock and requires confirmation when the World Assets status reports unsaved changes in any part (F-3), because a regression run replaces the world with the fixed regression bundle. The refusal text names the unsaved parts. Switching out of Regression is free.
- **What a mode changes.** The S35 card's content (Run: status and launch-outs; Author: the step rail, Feature 2; Regression: the runner controls, Feature 3) and nothing else: the shared cards and every module behave identically in all modes.
- Mode changes are logged and exposed in `cow.s35.status`.

### S35 panel card

Following the F-2 pattern: a pure **`S35PanelModel`** (state in, view out, no UI or module dependencies), a panel-only **`S35WorldSimDebugDomain`**, and the `s35_world_sim` custom card in CoW's `debug-panel.html`. The card shows the mode switch, the status line (bundle, world id, clock, sample count), the mode-specific area (empty placeholders for Author and Regression until Features 2 and 3), and the launch-out buttons slot (Feature 4). Every button runs a `cow.s35.*` or `cow.world.*` command; the card never calls module code.

### `HistoryDriverModule` (CoW-Shared)

Wires DiaCellHistory to the game clock; it holds no history logic.

- **Samples once per game week.** It handles `SimulationTick` and, for any tick whose band is Week or coarser, records a pending request (tick day and band). Like E1, it executes at the **start of the next simulation frame**, before GameSpeedController publishes the next day, so every system has fully handled the tick; ordering it before `GameSpeedControllerModule` in the `.diaapp` provides this. It then calls `history.Sample({ tick, label, band })`.
- **Label.** The calendar from the day count: year `day / 364 + 1`, week of year `(day % 364) / 7 + 1`, label `"Y12 W30"`. The band index is the tick's `TimeBand` (Day, Week, Season, Year, Decade, Century), matching DiaCellHistory's band names.
- **No missed samples.** Because GameSpeedController publishes at most one tick per frame, the driver sees every week; a reset or a bundle load clears its pending request and tells the recorder to start a new run (the recorder's reset, DiaCellHistory Feature 3).
- **World id.** It installs the recorder's world-id provider: `baked:<id>@<first 8 hash digits>` for a baked terrain, `preset:<preset>:<seed>:<size>` for an unedited preset, `edited:<n>` otherwise, from the terrain participant's `Capture()`. T-1 uses it to gate navigation.
- **Config.** `history.band` (default `Week`) and the tracked-layer list come from the module config and the stage; the recorder's budgets keep DiaCellHistory's defaults.
- The sampling rate is a testbed setting, not a game one: games that need other cadences can configure `band`.

### Session label for T-1

`WorldAssetsModule` exposes a read-only DiaDebugServer query `cow.session.get` returning `{ stage, mode, bundle, world }` (the stage module and `S35WorldSimModule` fill in stage and mode through a small `SetSessionInfo` call). T-1 shows it in its connection pill as `connected · S-3.5 · Run` when present, and plain `connected` otherwise (T-1 Feature 1 amended). Nothing else about T-1 changes.

### Logging and observation

- **Channel:** `S35`.
- **Info:** stage start (initial bundle chosen and why), mode changes, bundle loads and failures, history samples (every 52nd, Debug otherwise).
- **Warning:** no world at start, a mode switch refused.
- **Metrics:** samples taken, mode changes, time in each mode.
- **Health:** Degraded while the World Assets status is `partial` or `failed`.

## Acceptance Criteria

1. S-3.5 is listed in `cow_main.diaapp`'s Boot transitions and starts from the Boot menu; the stage's `.diaapp` composes the modules above in that order with absent modules tolerated.
2. At start the initial bundle is chosen as: fresh session hint, else the `.diastage` `world_bundle` (`"@default"` resolves to `default_world`), else none; with none, or on failure, the stage starts with an empty world and a visible reason.
3. After a successful bundle load the clock is paused.
4. The World Assets, Layers, Selection, Timeline and Game speed cards are present and work in every mode; snapshot buttons are disabled with a reason when no snapshot store exists.
5. `cow.s35.mode` switches Author, Run and Regression; switching to Regression pauses the clock and asks for confirmation when any F-3 part has unsaved changes, naming them; other switches are free.
6. `cow.s35.status` returns stage, mode, bundle, world id, clock state and sample count.
7. The S35 card is generated by `S35PanelModel` (pure) through `S35WorldSimDebugDomain` and the `s35_world_sim` custom card, and every control runs a `cow.*` command.
8. `HistoryDriverModule` records a sample for every Week-or-coarser tick, once, at the start of the next simulation frame, after all systems handled the tick; none are missed or duplicated across 20 simulated years at any speed.
9. Sample labels are `Y<year> W<week>` from the day count (day 0 is `Y1 W1`, day 364 is `Y2 W1`) and the band index matches the tick's band.
10. A bundle load or world reset clears the driver's pending request and starts a new history run.
11. The recorder's world id follows the terrain source (`baked:…`, `preset:…`, `edited:…`) and changes when it does.
12. `cow.session.get` returns stage, mode, bundle and world; T-1 shows `connected · S-3.5 · Run` when it is available and `connected` when not.
13. The stage adds no simulation code: all behaviour is in existing modules, the driver and the S35 module.
14. The visual result matches the stage and Run-mode regions of the mockup.

## Test Strategy

- **GoogleTests (TDD) with fakes:**
  - `HistoryDriverModule`: tick handling for every band, deferral to the next frame, ordering before the tick publisher, no missed or duplicate samples over a 20-year scripted run at several speeds, label arithmetic including year and week boundaries, reset on bundle load, world id from terrain `Capture()` for each source.
  - `S35WorldSimModule`: mode machine, the Regression confirmation rule, status JSON.
  - `S35PanelModel`: view for each mode and state, intent to command mapping.
  - `cow.session.get` content; initial bundle selection with the F-3 fakes.
- **Contract golden:** the `s35_world_sim` card state, in the existing panel contract style.
- **Vitest (page tests):** the card from the golden and fixtures.
- **E2E (DiaAutomation):** launch the stage from the Boot menu with a fixture default world; assert the bundle loaded and the clock paused; step one week and assert one history sample labelled `Y1 W2`; switch modes; a stub T-1 reads the session label.
- **Visual gate:** the stage and Run regions of the mockup.
- Regression, Author and launch-out behaviour are tested in Features 3, 2 and 4.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| AD-002 | Four-tier placement | Stage code feature-local; the history driver, used by several testbeds, is CoW-Shared |
| AD-004 | Boot transitions | Listed in `cow_main.diaapp` |
| AD-005 | One stage per testbed; modes in the panel | Author, Run and Regression are modes of the one stage |
| System spec | S-3.5 adds no new simulation code | Only wiring, the driver and the mode card |
| GameSpeedController / E1 pattern | Safe work happens at the frame boundary, ordered before the clock | The driver samples at the start of the next frame |
| F-1 panel pattern | Panel model + panel-only domain + custom card | `S35PanelModel`, `S35WorldSimDebugDomain`, `s35_world_sim` card |
| CH-S35-01 | Modes are developer-only | The card is `DIA_DEBUG` only; shipping-build gating is a separate later item |
| No singletons | Module references | The driver and S35 module reach others by reference |
| Editor actions via DiaAPI | Every action is a command | `cow.s35.*` and `cow.world.*` |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| F-3 Features 1 to 5 | Bundle load, World Assets card, session hint | Approved, not built |
| S-1 Features 1 and 8, A4 Features 1 to 3, A1 Features 1 to 7 | Layers and participants | Specced, not built |
| DiaCellLayers Features 1 to 4 | Layers and Selection cards | Approved, not built |
| DiaCellHistory Features 1 to 4 | Recorder, Timeline, snapshots | Approved, not built |
| DiaValueSchema | Described values | Approved, not built |
| E1, GameSpeedController, RNG streams | Save/load, clock, determinism | Done |
| CoW `ISnapshotStore` adapter over E1 | Snapshot buttons | Backlog task, not built |
| T-1 Feature 1 | Shows the session label | Approved; amended |
| A3 | Adds its layers and checks when built | Specced, not built |

## Resolved Decisions

1. **The history driver is CoW-Shared and specced here** (2026-10-20). Every testbed that records history needs the same calendar label, band mapping and world id; S-2.5 and S-3 adopt it as they are built (their specs carry a note).
2. **Sampling happens at the next frame boundary** (2026-10-20), ordered before the clock like E1's saves, so a sample never sees a half-handled tick.
3. **Mode switches only confirm for Regression** (2026-10-20), the one mode that replaces the world; Author and Run share the loaded world.
4. **The session label is a small generic query** (2026-10-20), so T-1 can say which testbed and mode it is connected to without knowing S-3.5.
5. **An empty world is a valid start** (2026-10-20): a fresh checkout has no `default_world`, and Author mode is how it gets one.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Can start once S-1 Features 1 and 8, A4 Features 1 to 3, A1 Features 1 to 7, F-3 Features 1 and 2 and DiaCellLayers Feature 1 exist.
