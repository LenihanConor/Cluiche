# Feature Spec: S-3.5 Author Mode

**Parent:** @docs/specs/applications/cow/systems/s35-worldsim/s35-worldsim.md
**Builds on:** @docs/specs/applications/cow/systems/s35-worldsim/stage-and-run-mode.md (Feature 1), @docs/specs/applications/cow/systems/f3-worldassets/world-assets-panel-card.md, @docs/specs/applications/cow/systems/f3-worldassets/save-as.md
**Mockup:** @docs/specs/applications/cow/systems/s35-worldsim/mockups/s35-world-sim.html (the "Authoring Pipeline" step rail)

**Gameplay Domains:** debug

## Summary

Author mode is the guided path from a blank world to the game's default world, on one screen. A **step rail** in the S35 card walks the seven steps (shape terrain, bake, weather pack, ecology pack, start state, save bundle, set default). Each step shows whether it is locked (and why), available, done or has unsaved changes, and offers its main action. A **preview clock** lets weather and ecology run while you tune, so effects are visible. Every action is an existing `cow.*` command; the rail adds the order, the gating display and the shortcuts, not new behaviour.

## Problem

All the pieces exist (S-1's sliders, A4 and A1 tuning, A1 paint, F-3's save-as and bundle commands), but using them to author a default world means knowing the order, knowing that start states need a baked world, that bundles refuse unsaved parts, and that the default must be validated. A first-time author hits each of these as a refusal. The rail makes the order, the prerequisites and the reasons visible, and keeps the whole loop on one screen.

## Goals

1. **Author a default world end to end** without leaving the stage, and see it load as New Game's world.
2. **The rail never lies.** Locks, reasons and done states come from the same data the commands use, so the rail and a refusal can never disagree.
3. **Visible tuning.** Weather and ecology run live during Author, with the clock under the author's control and history recorded.
4. **Safe resets.** Changing terrain resets the running simulation and says exactly what is lost; nothing on disk changes without a Save.
5. **One screen.** Deep tools stay in their own cards; the rail opens and drives them, and duplicates none.

## Non-Goals

- New simulation, new tuning controls or new asset commands (they belong to S-1, A4, A1 and F-3).
- Editing worlds cell by cell offline (T-2).
- Running the regression checks (Feature 3) or launching out (Feature 4).
- Automatic saving, or deleting files written earlier in a session.

## Architecture

All of Author mode is the S35 card's Author area (the Feature 1 placeholder), built like the rest of the card: a pure **`AuthorStepModel`** (state in, view out), carried by `S35WorldSimDebugDomain`, rendered by the `s35_world_sim` custom card. The rail's buttons run `cow.*` commands or forward to another card's domain command; the model contains no asset or simulation logic.

### Inputs to the model

- `cow.world.status` (F-3): loaded bundle or `custom`, terrain source (`preset`, `baked`, `edited`), each part (weather pack, ecology pack, start state) with its stale and unsaved flags, load state, and the `actions` block with `{enabled, reason}` per action.
- Rail-local choices that are not facts about the world: for steps 3 and 4 whether the author has chosen **defaults** (no pack), for step 5 whether it was **skipped**. Kept in the S35 module in memory, never written to disk.
- GameSpeedController state (paused, speed) and the history recorder's sample count, for the preview strip.

### The seven steps

| # | Step | Main action | Available when | Done when |
|---|---|---|---|---|
| 1 | Shape terrain | S-1 sliders, seed, `cow.worldgen.set_size`, Regenerate | always | S-1 has accepted a world |
| 2 | Bake | `cow.world.terrain.bake <id>` | a world is accepted (`terrain.bake` enabled) | terrain source is `baked` |
| 3 | Weather pack | `pack.load weather`, tune in the Weather card, `pack.save_as weather <id>`, or **Use defaults** | a world is accepted | a pack is loaded and has no unsaved changes, or defaults were chosen |
| 4 | Ecology pack | the same for ecology | a world is accepted | the same |
| 5 | Start state | paint with A1's paint on a selection, `startstate.save_as <id>` | **terrain is baked** (`startstate.save_as` enabled) | a start state is loaded and saved, or **Skip** was chosen |
| 6 | Save bundle | `cow.world.bundle.save_as <id>` | steps 2 to 4 done, no part has unsaved changes (`bundle.save_as` enabled) | the loaded bundle is the one just saved |
| 7 | Set default | `cow.world.bundle.set_default <id>` | step 6 done (`bundle.set_default` enabled) | `cow.diagame` `default_world` names the bundle |

- **Locks come from F-3.** A step's availability is the `enabled` flag of its action in `cow.world.status.actions`, and the lock tooltip is that action's `reason` verbatim ("bake the terrain first (start states need baked terrain)", "weather has unsaved changes: `pack.save_as weather <id>` first, or `pack.clear weather`", "run Validate in game"). The rail has no gating rules of its own.
- **States** per step: `locked`, `available`, `done`, `unsaved` (done, but the part has changes since), `reset` (made stale by a terrain change).
- **Order is advice, not a wizard.** Any available step can be used at any time; the rail highlights the next undone one.

### Step controls

Each step shows a short summary and its main action inline where that is simple (an id field and a button for Bake, a pack picker and **Use defaults** for steps 3 and 4, **Skip** for step 5). For anything richer the rail reuses an existing card instead of repeating it:
- **Show card** expands and scrolls to the owning card (S-1 shaping, Weather, Ecology, Layers and Selection for painting, World Assets). The S35 card's page script does this by setting the panel's open-card state, in the same file that already renders the F-2 card.
- **Save as…** buttons forward to the World Assets domain (`open_dialog {kind}`), so the same dialog, diff preview and command line from F-3 Feature 5 appear; the rail does not duplicate it.

### Preview clock

In Author mode weather and ecology run live so tuning shows its effect:
- The clock is GameSpeedController's: a **preview strip** at the top of the rail shows Play/Pause, the speed presets, the current date (`Y3 W12`) and the history sample count, and calls `cow.speed.*`. After a terrain or bundle load it starts **paused** (A2 EC-03).
- History is recorded (Feature 1's driver), so tuning can be scrubbed, compared and snapshotted with the Timeline card.
- **Reset preview** (`cow.s35.author.reset_preview`) re-initialises weather and ecology from the current terrain and parameters, starting a new history run. It is available when the terrain is baked (it is `cow.world.terrain.load` of the baked world with the current parameters); before the bake, the S-1 Regenerate button does the same job. Reset discards painted cells and unsaved start-state edits, so it asks for confirmation when F-3 reports unsaved cell edits.
- The preview never writes any file.

### Terrain changes

Changing the terrain (a shaping slider or Regenerate in step 1, or loading another world) **resets the simulation state** of weather and ecology (they re-initialise on `WorldGenerationComplete`), clears the rail-local choices, and marks steps 2 to 7 `reset` until they are redone. It does **not** change loaded parameters or packs, and does **not** delete anything on disk. If the terrain was baked, S-1's locked-import prompt appears first ("moving this slider unbakes the world"), and the card lists what will be lost: the painted cells and the baked association.

### Finishing

Once step 7 is done the rail shows a **summary**: the files written this session (from the `WorldAssetSaved` events: paths and kinds), the bundle id and the default world, and a note that the files are in the source tree for review and commit (S-3.5 runs no git commands). A **Load default world** button runs `cow.world.bundle.load` on the current default so the author can see it load as New Game would.

### Commands

| Command | Effect |
|---|---|
| `cow.s35.author.status` | The rail view as JSON: each step's state, reason and summary, the rail-local choices, the preview strip |
| `cow.s35.author.choose <weather\|ecology> <defaults>` | Record "use defaults" for step 3 or 4 |
| `cow.s35.author.skip startstate` | Record "skip" for step 5 |
| `cow.s35.author.reset_preview` | Re-initialise from the baked terrain and current parameters (confirmation rule above) |

Everything else the rail does is a `cow.world.*`, `cow.worldgen.*`, speed or card command.

### Logging and observation

- **Channel:** `S35`.
- **Info:** each step completed (id and path), the preview reset, the summary shown.
- **Warning:** a refused action with its reason, a terrain change that unbaked the world.
- **Metrics:** authoring sessions, steps completed, preview resets.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. The Author area shows the seven steps with state, summary and main action, and highlights the next undone step; any available step can be used in any order.
2. Each step's availability and lock reason equal the matching action's `enabled` and `reason` in `cow.world.status.actions`, verbatim; the rail contains no gating rule of its own (checked by a table-driven test across all lock cases).
3. Bake is locked until a world is accepted; the start-state step is locked until the terrain is baked, with F-3's reason; the bundle step is locked while any part has unsaved changes, naming the part; Set default is locked until the bundle is saved, and refused for an unvalidated-edit terrain with the Validate in game route.
4. Steps 3 and 4 are done when a pack is loaded with no unsaved changes or **Use defaults** was chosen; step 5 when a saved start state is loaded or **Skip** was chosen; the choices are held in memory and cleared on a terrain change.
5. Step controls that need a richer UI open the owning card; Save as… opens the World Assets save dialog for the right kind, with its diff preview and command line; the rail duplicates none of them.
6. The preview strip shows Play/Pause, speed presets, date and sample count, calls the speed commands, and the clock starts paused after a terrain or bundle load.
7. Weather and ecology run live in Author mode and history is recorded.
8. Reset preview re-initialises from the baked terrain and current parameters, starts a new history run, asks for confirmation when cell edits are unsaved, and is unavailable (pointing to Regenerate) before the bake.
9. A terrain change resets the simulation state and the rail-local choices, marks steps 2 to 7 `reset`, keeps parameters, packs and files, and shows what will be lost before a baked world is unbaked.
10. After step 7 the summary lists the files written this session and the default world; **Load default world** loads it.
11. Nothing in Author mode writes a file except through the `save_as`, `bake` and `set_default` commands.
12. The rail is generated by `AuthorStepModel` (pure) and every control runs a `cow.*` command or forwards to another card's domain command.
13. The visual result matches the Author area of the mockup.

## Test Strategy

- **GoogleTests (TDD):**
  - `AuthorStepModel` over status fixtures: every step state; the lock cases against `status.actions` (a table-driven test that feeds the real F-3 refusal strings); rail-local choices and their reset; the next-step highlight; terrain-change reset; the summary content.
  - The author commands and the reset-preview confirmation rule with fakes for F-3 status, speed and history.
- **Contract golden:** the Author area state in the existing panel contract style.
- **Vitest (page tests):** the rail from fixtures: states, tooltips, forwarded buttons, Show card behaviour.
- **E2E (DiaAutomation), the system's test:**
  1. In Author mode: shape (set a seed and size), bake, choose or tune packs, paint a few cells, save the start state, save the bundle, set it as default.
  2. Relaunch the game and use the front end's New Game.
  3. The loaded world's combined state hash equals the bundle's.
  4. `git status` shows the new source files (the E2E runs on a scratch copy, see Feature 3's `--source` override).
  The scripted run drives the commands the rail runs, so it also proves the rail's command surface.
- **Visual gate:** the Author area of the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| F-3 Resolved Decision 2 | Start states only on baked terrain | Step 5 is locked by F-3's own reason |
| T-2 Resolved Decision 2 | Unvalidated world edits block only the default world | Step 7 is locked by F-3's `set_default` rule |
| F-3 Feature 5 | The World Assets card owns the save dialog | The rail forwards to it |
| Editor actions via DiaAPI | Every action is a command | The rail runs commands only |
| UI/business separation | A typed model first | `AuthorStepModel` is pure |
| System spec | S-3.5 adds no new simulation code | The rail only orders and forwards |
| AD-005 | Modes are panels of one stage | Author is an area of the S35 card |
| CH-S35-01 | Modes are developer-only | The card is `DIA_DEBUG` only |
| A2 EC-03 | Loads resume paused | The preview clock starts paused |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Stage, mode machine, S35 card, history driver | Approved |
| F-3 Features 2, 3 and 5 | Status and `actions`, save-as and set-default, World Assets card and dialog | Approved, not built |
| S-1 | Shaping sliders, size, Regenerate, bake, locked-import prompt | Specced, not built |
| A4, A1 | Tuning cards; A1 paint and edit set | Specced, not built |
| DiaCellLayers | Selection for painting | Approved, not built |
| GameSpeedController, DiaCellHistory | Preview clock and history | Done; Approved |

## Resolved Decisions

1. **The rail has no rules of its own** (2026-10-20). Locks, reasons and done states derive from F-3's status and `actions`, so a refusal at the command and a lock on the rail are one fact.
2. **Order is advice, not a wizard** (2026-10-20). Authors tune and bake in whatever order the work needs; the rail only shows what is possible and what is next.
3. **A terrain change resets state, not parameters or files** (2026-10-20), because tuning is expensive to redo and files are only ever changed by an explicit save.
4. **Save dialogs are reused, not duplicated** (2026-10-20): the rail forwards to the World Assets card, so the diff preview and command line are the same everywhere.
5. **The preview starts paused** (2026-10-20), as every load does (A2 EC-03), so the author controls when the world starts to change.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build order is after Feature 3.
