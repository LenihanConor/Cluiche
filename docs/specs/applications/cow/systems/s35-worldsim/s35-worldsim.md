# System Spec: S-3.5 World Sim

**Parent:** @docs/specs/applications/cow/cow.md  
**Origin:** 2026-10-19 world-sim product review. Outcomes C ("a testbed to demonstrate and test the work") and D ("a place to author content"). This is the one testbed that runs every world layer together on one world.  
**Mockup:** [mockups/s35-world-sim.html](mockups/s35-world-sim.html)  
**Systems under test:** S-1 World Generation, A4 Weather, A1 Ecology, A3 Biome Succession, F-3 World Assets; Dia: DiaCellLayers, DiaCellHistory, DiaValueSchema

**Gameplay Domains:** debug

## Summary

Each layer already has its own testbed, which is the tool for understanding and tuning that layer:

```
Dragons / game   S-4+        later
World Sim        S-3.5       ← this testbed: every layer together, authoring, whole-world proof
Ecology          S-3         A1, A3
Weather          S-2.5       A4
Terrain          S-1         live terrain editor
```

S-3.5 runs them **together on one world** in one stage (AD-005), with three modes chosen in its panel:

- **Author:** the full authoring loop on one screen:
  1. shape terrain
  2. bake it
  3. choose or tune a weather pack
  4. tune an ecology pack
  5. paint a start state
  6. save a bundle
  7. set it as the game's `default_world`

  This is outcome D's answer to "can I use the testbed to create default terrain and tuning packs?".
- **Run:** load a bundle and run every layer live, with the layer viewer, selection inspector, history timeline and snapshots. T-1 can connect for analysis.
- **Regression:** headless whole-world checks across Small, Standard and Large. It writes a report and a `dia.cell_history` export.

S-3.5 hosts only the **shared** cards (World Assets, Layers, Selection, Timeline) plus S-1's shaping sliders for Author mode. For deep work on one layer it **launches out** to S-2.5 or S-3 with the same bundle. It doesn't duplicate their tools.

## Goals

1. **Author a default world end to end** without leaving the stage, and see it load as the game's New Game world.
2. **Watch the whole world run.** All layers are live and switchable one at a time, any area can be selected and compared, and you can scrub back and restore snapshots.
3. **Prove the world as a whole:**
   - reproducible across sizes and range counts
   - save/load and snapshot restore exact
   - authoring round trip exact
   - Large inside budget
   - every value described
4. **One way to deep-dive.** "Open in S-2.5" and "Open in S-3" carry the current bundle.

## Non-Goals

- **Per-layer proofs.** S-1's guarantees, S-2.5's nine weather checks, S-3's ecology scenarios and A1/A4 equation tests stay in their own testbeds and tests. S-3.5 proves the combination.
- **Dragons.** S-4 adds them later, on top of this world.
- **Real art.** All layers are debug-grade (V-1 later).
- **New simulation code.** S-3.5 only wires up existing modules; if it needs new behaviour, that belongs to a system.

## Architecture

### Placement

Feature-local (AD-002):
- **Code:** `Cluiche/CoW/Modules/` (S35 prefix).
- **Assets:** `Cluiche/Assets/CoW/Stages/S35WorldSim/`.
- **Stage:** `s35_world_sim.diastage`, listed as a Boot transition (AD-004).

```
SimPU (S35WorldSim stage)
  ├─ WorldAssetsModule        F-3: bundle load/save, packs, start state, bake
  ├─ WorldGenerationModule    S-1 (Author: live shaping; Run: loads the bundle's terrain)
  ├─ GameSpeedControllerModule, RandomStreamModule, SaveLoadModule (E1)
  ├─ WeatherModule            A4
  ├─ EcologyModule            A1
  ├─ SuccessionModule         A3 (when built)
  ├─ ValueSchemaModule        DiaValueSchema
  ├─ CellLayerViewerDomain    DiaCellLayers (all layers registered by the modules above)
  ├─ CellHistory recorder     DiaCellHistory (snapshot store = E1 adapter)
  └─ S35WorldSimModule        mode switch, Author step rail, launch-outs, Regression runner, S35 panel card
```

```json
// s35_world_sim.diastage (config excerpt)
{
  "world_bundle": "default",
  "history": { "track": ["ecology.vegetation", "ecology.herbivores", "weather.precipitation"] },
  "s35": { "mode": "Run", "regression": { "years": 20, "sizes": ["Small", "Standard", "Large"],
                                          "ranges": [1, 3, 17], "seed": 42 } }
}
```

### Modes

**Author**

| Step | Card / command | Done when |
|---|---|---|
| 1 Shape terrain | S-1 sliders, seed, `cow.worldgen.set_size` | — |
| 2 Bake | `cow.world.terrain.bake <id>` | A baked `.wld.json` is loaded (enables step 5) |
| 3 Weather pack | `cow.world.pack.load` / tune / `pack.save_as weather <id>` | A pack is chosen, or defaults are used |
| 4 Ecology pack | `cow.world.pack.load` / tune / `pack.save_as ecology <id>` | A pack is chosen, or defaults are used |
| 5 Start state | A1 paint on the selection, `cow.world.startstate.save_as <id>` | Optional |
| 6 Save bundle | `cow.world.bundle.save_as <id>` | The bundle is written |
| 7 Set default | `cow.world.bundle.set_default <id>` (writes `cow.diagame` `default_world`) | Refused if the terrain is an "unvalidated edit" (T-2 rule) |

- Weather and ecology run in **preview** during Author so tuning shows effects: the clock runs at the chosen speed and history is recorded.
- Changing terrain (step 1) resets them.
- `cow.world.bundle.set_default` is a new F-3 command (amendment below).

**Run**
- Load a bundle (`cow.world.bundle.load`) and run all layers live at any game speed.
- Cards: Layers, Selection, Timeline (scrub, events, snapshots), World Assets.
- T-1 shows "connected · S-3.5 · Run".

**Regression** (headless with `dia run cow --stage s35_world_sim --automation regression`, or from the panel)

| Check | Pass |
|---|---|
| R1 Reproducible world | For each size and range count, the bundle run for N years gives an identical combined state hash (S-1 world + A4 fields + A1 state + A3 state). Range counts must also agree with each other |
| R2 Save/load | Save at year N/2, load and continue: the hash at year N equals the uninterrupted run |
| R3 Snapshot restore | Take a snapshot at year N/2, run to N, restore, run to N again: the hash equals the first pass |
| R4 Authoring round trip | `pack.save_as` + `startstate.save_as` + `bundle.save_as` from a tuned state, relaunch, `bundle.load`: resolved parameters and state hash after `Initialise` + start state are identical |
| R5 Perf at Large | Each system meets its budget (A4 week ≤ 10 ms, A1 season ≤ 16 ms). The worst tick where they coincide is ≤ 26 ms. All Release, reported gate |
| R6 Coverage | `dia validate schema` is clean for every loaded file, every registered layer has a schema entry, and every emitted event type is described |

- The report goes to `out/CoW/s35/regression_<seed>.json`, plus `regression_<seed>.cell_history.json` for T-1.
- Panel check lines show pass/fail per size.

### Launch-outs

"Open in S-2.5" and "Open in S-3" transition to that stage with the current bundle id. The hand-off is a CoW-Shared **session hint**: WorldAssetsModule writes `out/CoW/session/next_bundle.json`, and the target stage loads it once in place of its `.diastage` bundle (see Resolved Decisions).

### Logging and observation

- **Channel:** `S35`.
- **Info:** mode change, author steps completed, regression start/finish, launch-outs.
- **Error:** failed checks with the first differing field.
- **Metrics:** per-check duration, worst coinciding tick.
- **Health:** a failed regression check is reported as Degraded.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Stage and Run mode | Stage, modules, Boot transition, bundle load, shared cards, T-1 connection | Draft | TBD |
| 2 | Author mode | Step rail, preview clock, gating (bake before paint, validated before default), `bundle.set_default` | Draft | TBD |
| 3 | Regression mode | R1–R6, headless automation, report and history export, panel check lines | Draft | TBD |
| 4 | Launch-outs | Session hint, "Open in S-2.5/S-3" | Draft | TBD |

- **Build order:** 1 → 3 → 2 → 4.
- Feature 1 can start once S-1 Features 1 and 8, A4 Features 1–3, A1 Features 1–7, F-3 Features 1–2 and DiaCellLayers Feature 1 exist. A3 joins when built: its checks switch on and the combined hash gains A3 state.

## Amendments when Approved

- **F-3:** add `cow.world.bundle.set_default <id>`. It writes `cow.diagame` `default_world` through the `source` alias, adds the bundle to the catalogue, and refuses an "unvalidated edit" terrain. Also add the session hint (`next_bundle.json`).
- **S-2.5, S-3:** honour the session hint at stage start.
- **cow.md:** register S-3.5. The roster grows from 12 to 13 testbeds (AD-005 wording).
- **GDD fold-back** (through the GDD session, AD-003): the GDD owns the testbed roster and what each testbed proves, so S-3.5 and its R1–R6 checks must be folded back.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| S-1 (Features 1, 8; validate command) | Terrain shaping, bake, validate | Specced, not built |
| A4, A1, A3 | Live layers | Specced, not built |
| F-3 World Assets | Bundles, packs, start states, bake, set default | Approved, not built |
| DiaCellLayers, DiaCellHistory, DiaValueSchema | Shared cards, history, coverage | Approved, not built |
| E1 SaveLoad, GameSpeedController, RNG streams | Save/load, clock, determinism | Done |
| T-1 (optional) | Analysis of Run and Regression output | Approved, not built |

## Binding Decisions

| Source | Decision | Constraint |
|---|---|---|
| AD-002 | Four-tier placement | Stage code is feature-local; nothing new in CoW-Shared except the F-3 session hint |
| AD-003 | GDD owns the roster and what testbeds prove | S-3.5 needs a GDD fold-back; until then it is a spec-tree testbed proposal |
| AD-004 | Boot transitions | S-3.5 is listed in `cow_main.diaapp` Boot transitions |
| AD-005 | One stage per testbed; modes in the panel | Author, Run and Regression are panel modes of one stage |
| Size policy (2026-10-19) | Small, Standard, Large; Large is the perf gate | R1–R3 run at all sizes; R5 at Large |
| F-3 Resolved Decision 2 | Start states only on baked terrain | Author step 5 is gated on step 2 |

## Test Strategy

- **The Regression mode is the test.** It runs in CI as `dia run cow --stage s35_world_sim --automation regression` at Small. Standard and Large run nightly with the `SLOW_` convention.
- **GoogleTests:**
  - The combined-hash builder is stable across runs and covers each system's state.
  - The session hint is read once, then deleted.
  - Author gating rules.
- **E2E:**
  1. Author: shape → bake → save the packs and bundle → set it as default.
  2. Relaunch the game and use the front-end's New Game.
  3. The world matches the bundle's hash.
  4. The visual gate is checked against the mockup.

## Resolved Decisions

1. **Launch-out hand-off by session hint file** (2026-10-19). Stage transitions don't carry arguments, so WorldAssetsModule writes `out/CoW/session/next_bundle.json`, and the next stage consumes it once. Rejected: adding stage-transition arguments to DiaApplicationFlow. That would be a Dia change for one use.
2. **The combined state hash comes from E1 participant output** (2026-10-19). S-3.5 hashes the JSON each E1 save participant writes (S-1 world, weather, ecology, succession, RNG, game speed). It already covers all saved state and needs no new `StateHash()` per system. A mismatch reports the first participant and key that differs.
3. **Regression uses a fixed committed bundle** (2026-10-19): `Stages/S35WorldSim/regression.cowworld` (baked terrain plus packs). The gate doesn't move when the game's `default_world` changes. Updating it is a deliberate commit.

## Status

`Approved` (2026-10-19). Feature specs TBD; no plan yet. GDD roster fold-back pending (AD-003).
