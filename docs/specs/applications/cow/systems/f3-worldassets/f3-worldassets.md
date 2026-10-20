# System Spec: F-3 World Assets

**Parent:** @docs/specs/applications/cow/cow.md  
**Origin:** 2026-10-19 product review of the world-sim stack. Outcomes B7 (clear assets: how they load and save) and D (author content in the testbeds) had no owner.  
**Consumers:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md, @docs/specs/applications/cow/systems/a4-weather/a4-weather.md, @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md, @docs/specs/applications/cow/systems/a3-biomesuccession/a3-biomesuccession.md, the S-1, S-2.5, S-3 and S-4 World Sim testbeds

**Gameplay Domains:** debug

## Summary

F-3 is the single contract for every file the world-sim stack reads or writes, plus the commands that author those files from inside a testbed.

Before F-3, each system named its own files. Some had a path and some didn't, only some were versioned, and nothing let you save a tuned world as a file the game loads. F-3 fixes four things:

1. **One layout.** Every world-sim asset has a fixed home under `Cluiche/Assets/CoW/Global/<System>/`. Testbed-only files stay in `Stages/<Stage>/`.
2. **One envelope.** Every file starts with `"format"` and `"version"`, and one load-and-warn rule handles version mismatches.
3. **One loader.** `WorldAssetsModule` resolves a **world bundle** (`.cowworld`): terrain, weather pack, ecology pack and start state. It applies each part to the system that owns it. Systems never read asset paths themselves.
4. **Save as.** Testbeds author content with DiaAPI `save_as` commands. In dev launches these write straight into the source tree and the catalogue.

The game picks its default world by naming a bundle in `cow.diagame`.

## Goals

1. **Every asset has a home.** Each world-sim asset type has one extension, one source path, one version key and one owner (the Asset Table).
2. **Author in the testbed.** You can do all of this in a testbed and save the result as a source asset:
   - shape terrain and bake it
   - tune weather or ecology and save the changes as a pack
   - paint ecology cells and save them as a start state
   - put the pieces together into a world bundle
3. **The game loads what you authored.** New Game loads `cow.diagame`'s `default_world` bundle. A testbed's `.diastage` can name a bundle too.
4. **Whole-world reproducibility.** Loading the same bundle gives the same starting world: the same terrain, resolved parameters and start state. Save/load and determinism tests cover this.
5. **Tuning survives change.** Packs hold only overrides, so adding a parameter to a defaults file never breaks an existing pack.
6. **Visible staleness.** A pack or start state built against an older defaults file or world still loads. It logs a Warning and shows a "stale" badge, and saving it again upgrades it.

## Non-Goals

- **Defining the parameters.** Each system owns the meaning and defaults of its own parameters. F-3 owns the files, the envelope, override merging, loading and saving.
- **Savegames.** `.sav` stays E1's (`out/saves/cow/`). A savegame is a running game; a bundle is a starting world.
- **Generated reports.** Runner reports, long-run output and exports for inspection stay in `Cluiche/out/CoW/` (PD-009).
- **An offline editor.** Authoring happens in testbeds. A CluicheEditor plugin can reuse the same DiaAPI commands later (tool tiers: this is the Editor tier hosted in-game, by user decision 2026-10-19).
- **Shipping-build authoring.** `save_as` to source works only in dev launches with the source alias. Without the alias, the commands report that saving is unavailable.
- **Asset hot-swap mid-simulation.** Loading a bundle resets the world. Live parameter edits stay per-system (`set_param`, sliders).

## Architecture

### Placement

**CoW-Shared** (AD-002): `Cluiche/CoW/Shared/WorldAssets/`, namespace `CoW::WorldAssets`, on the SimPU.

The design keeps in mind that this may move to Dia later (product-review decision: simulation cores stay CoW-Shared and are extracted later):
- The envelope, override merge and staleness check have no CoW types in them.
- Only the bundle schema and the participant list are CoW content.

Two DiaCLI changes are **Dia-tier** and need sign-off under AD-001 (see Dependencies).

### Asset Table

Paths are relative to `Cluiche/Assets/CoW/`. Every file has `"format": "<format id>"` and `"version": <int>`.

| Asset | Extension | Source path | Format id | Owner (meaning) | Written by |
|---|---|---|---|---|---|
| Terrain rules (defaults) | `.json` | `Global/WorldGen/terrain_rules.json` | `cow.terrain_rules` (replaces bare `rulesVersion`) | S-1 | hand |
| World-shape preset (recipe) | `.worldshape.json` | `Global/WorldGen/Presets/<id>.worldshape.json` | `cow.worldshape` | S-1 | `cow.worldgen.preset.save` (exists) |
| Baked world | `.wld.json` | `Global/WorldGen/Baked/<id>.wld.json` | `cow.world` | S-1 | `cow.world.terrain.bake` |
| Weather parameters (defaults) | `.json` | `Global/Weather/weather_parameters.json` (moved from `CoW/weather_parameters.json`) | `cow.weather_params` | A4 | hand |
| Weather pack | `.weather.json` | `Global/Weather/Packs/<id>.weather.json` | `cow.weather_pack` | A4 | `cow.world.pack.save_as weather` |
| Species, biomes, ecology parameters (defaults) | `.json` | `Global/Ecology/species.json`, `biomes.json`, `ecology_parameters.json` | `cow.species`, `cow.biomes`, `cow.ecology_params` | A1 | hand |
| Ecology pack | `.ecology.json` | `Global/Ecology/Packs/<id>.ecology.json` | `cow.ecology_pack` | A1 | `cow.world.pack.save_as ecology` |
| Ecology start state | `.ecostate.json` | `Global/Ecology/StartStates/<id>.ecostate.json` | `cow.ecology_start` | A1 | `cow.world.startstate.save_as` |
| Succession rules (defaults) | `.json` | `Global/Succession/succession_rules.json` | `cow.succession_rules` | A3 | hand |
| World bundle | `.cowworld` | `Global/Worlds/<id>.cowworld` | `cow.world_bundle` | F-3 | `cow.world.bundle.save_as` |
| Scenario (CoW-Shared format, S-2.5 Feature 4) | `.scenario.json` | `Stages/<Stage>/scenarios/<id>.scenario.json` | `cow.scenario` | S-2.5 | hand |
| Testbed baselines and edge cases | `.json` | `Stages/<Stage>/…` (e.g. `S25WeatherPatterns/baseline.json`) | per testbed | testbed | testbed commands |

Generated output (not assets): `out/CoW/s25/report_*.json`, `out/CoW/s3/longrun_*.json`, `out/CoW/worlds/` exports, `out/saves/cow/*.sav`.

### File shapes

**Pack (overrides only).** Holds only the keys that differ from the defaults, plus the defaults version it was saved against:

```json
{
  "format": "cow.ecology_pack", "version": 1,
  "base": { "format": "cow.ecology_params", "version": 3 },
  "displayName": "Lush",
  "overrides": { "grazing.rate": 0.8, "regrowth.forest": 1.25 }
}
```

Keys are dotted paths into the defaults file. Weather packs have the same shape with `cow.weather_pack` / `cow.weather_params`.

**Start state (sparse overlay).** Holds only the edited cells, applied after A1 `Initialise`:

```json
{
  "format": "cow.ecology_start", "version": 1,
  "world": { "baked": "archipelago_s42", "hash": "9f3c…" },
  "cells": [
    { "q": 12, "r": 7, "vegetation": 0.9, "herbivores": 40 },
    { "q": 13, "r": 7, "biome": "BurnedAshland" }
  ]
}
```

`world` names the **baked** terrain it was painted on and that terrain's content hash. Start states require a baked world (Resolved Decision 2): bake first, then paint. Field names are A1's `set_field` names.

**World bundle.** Refers to the other files by path relative to `Global/`:

```json
{
  "format": "cow.world_bundle", "version": 1,
  "displayName": "Archipelago (lush)",
  "terrain": { "baked": "WorldGen/Baked/archipelago_s42.wld.json" },
  "weather": "Weather/Packs/harsh_winters.weather.json",
  "ecology": "Ecology/Packs/lush.ecology.json",
  "startState": "Ecology/StartStates/archipelago_s42.ecostate.json"
}
```

- `terrain` is either `{ "baked": … }` or `{ "preset": "<id>", "seed": <u64>, "size": "Small|Standard|Large" }`.
- `weather`, `ecology` and `startState` are optional; leaving one out means "use the defaults" or "no start state".
- Game config: `cow.diagame` → `"config": { "default_world": "<bundle id>" }`.
- Testbeds: a `.diastage` config may set `"world_bundle": "<id>"`. This amends SD-S1-010, which allowed only a `.wld.json` or preset + seed.

### Version rule: load and warn

| Case | Result |
|---|---|
| File `version` > the version the loader understands | **Error**: not loaded |
| Pack `base.version` < current defaults version | Loads. Unknown override keys are dropped with one Warning each, and missing keys come from the defaults. Marked **stale** |
| Start state on non-baked terrain | **Error**: `startstate.save_as` / `load` refuse; bake the terrain first |
| Start state `world.hash` ≠ loaded terrain hash | Loads cells whose `(q, r)` is in range and drops the rest with a Warning. Marked **stale** |
| Bundle references a missing file | **Error**: the bundle doesn't load and the previous world stays |
| Saving a stale file | Writes the current versions, which clears stale |

Format migrations are not built in v1. If a format needs a breaking change, it gets a migration then, the way E1 save participants do.

### Runtime

```
SimPU
  WorldAssetsModule (CoW-Shared)
    ├─ AssetEnvelope        format/version read + check, StaleInfo
    ├─ OverrideMerge        defaults + pack overrides → resolved Json; diff current vs defaults → overrides
    ├─ BundleResolver       .cowworld → WorldBundleSpec (paths resolved through the `world` alias)
    ├─ SourceWriter         save_as: write file to the `source` alias + upsert catalogue record
    └─ participants (ModuleRef, no singletons):
         WorldGenerationModule  (terrain)    IWorldAssetParticipant
         WeatherModule          (weather)    IWorldAssetParticipant
         EcologyModule          (ecology, start state) IWorldAssetParticipant
```

**`IWorldAssetParticipant`** (CoW-Shared). Each owning module implements it:
- `Kind()` returns a StringCRC: `terrain`, `weather`, `ecology`, `ecology_start`.
- `Defaults()` returns the loaded defaults Json.
- `Apply(const Json::Value& resolved)` applies a resolved asset.
- `Capture()` returns the current values. For packs, F-3 diffs these against `Defaults()` to get the overrides.

Systems keep their own live-edit commands. F-3 only resolves, applies and captures.

**Load order** for `LoadBundle(id)`:
1. Terrain: the baked world loads, or the preset + seed is generated. F-3 waits for S-1's completion.
2. Weather: resolved parameters are applied.
3. Ecology: resolved parameters are applied, then A1 `Initialise` runs.
4. Start-state cells are applied.
5. `WorldBundleLoaded{bundleId}` is broadcast.

A failure at any step logs an Error and keeps the previous world.

**Session hint** (amended 2026-10-19, S-3.5): stage transitions carry no arguments. To launch another testbed with the current bundle, `WorldAssetsModule` writes `out/CoW/session/next_bundle.json` (`{bundle, from_stage}`). At start, the next stage loads that bundle once in place of its `.diastage` `world_bundle`, then deletes the file. A stale hint (older than 60 s) is ignored and deleted.

**Path aliases** (registered from `cow.diagame` `path_aliases`):
- `world` → the deployed `global/world/` tree. Used for loading in every build.
- `source` → `Cluiche/Assets/CoW/`. Dev launches only; DiaCLI passes it (dia-needs #16). `save_as` needs it.

### Public Interfaces

DiaAPI commands. Every authoring action is a command, and the panel calls the same commands.

| Command | Does |
|---|---|
| `cow.world.status` | Loaded bundle, its parts, stale flags, whether `source` is present |
| `cow.world.bundle.list` / `load <id>` | List and load bundles |
| `cow.world.bundle.save_as <id> [displayName]` | Write a bundle referencing the currently loaded terrain, packs and start state |
| `cow.world.pack.list <weather\|ecology>` / `load <kind> <id>` / `clear <kind>` | Apply a pack (overrides on top of defaults) or go back to the defaults |
| `cow.world.pack.save_as <weather\|ecology> <id> [displayName]` | Diff current values against the defaults and write the overrides |
| `cow.world.terrain.bake <id>` | Write the current S-1 world to `Global/WorldGen/Baked/<id>.wld.json` |
| `cow.world.startstate.save_as <id>` / `load <id>` / `clear` | Save the edited cells (A1's edit set) as a sparse start state, or apply or clear one |
| `cow.world.bundle.set_default <id>` | Write `cow.diagame` `config.default_world` through the `source` alias. Refused if the bundle's terrain is an "unvalidated edit" (T-2) (amended 2026-10-19, S-3.5) |

- `save_as` refuses to overwrite an existing id unless it is given `overwrite`.
- Catalogue writes from the game only add or update records it owns (Resolved Decision 3).
- Every write adds or updates a record in `assets.catalogue.json`, with the id, the type (`world_bundle`, `weather_pack`, `ecology_pack`, `ecology_start`, `world_baked`) and the `world` tag.

**Events** (DiaMessageBus, small POD): `WorldBundleLoaded{bundleId}`, `WorldAssetSaved{kind, id}`.

**Logging:**
- Channel `WAS`.
- Info: load and save, with the path.
- Warning: stale, dropped keys, dropped cells.
- Error: unsupported version, missing file, `save_as` with no `source` alias.

**Panel:** a "World Assets" card in the F-1 debug host (mockup: [mockups/world-assets-card.html](mockups/world-assets-card.html); schema-driven tooltips: [mockups/field-schema-tooltips.html](mockups/field-schema-tooltips.html)). It shows the loaded bundle and its parts with stale badges, has list/load/save-as controls, and shows whether `source` is present. It needs an HTML mockup before it is built, and it calls only DiaAPI commands.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Envelope and override merge | `AssetEnvelope`, `OverrideMerge` (resolve and diff), stale rules. Pure, with GoogleTests | Draft | TBD |
| 2 | WorldAssetsModule and bundle load | `IWorldAssetParticipant`, `BundleResolver`, load order, `default_world` from `cow.diagame`, `.diastage` `world_bundle`, events | Draft | TBD |
| 3 | Save as | `SourceWriter`, catalogue upsert, `pack.save_as`, `terrain.bake`, `startstate.save_as`, `bundle.save_as`; needs dia-needs #16 | Draft | TBD |
| 4 | Deploy and aliases | `world` deploy category mirroring subfolders (dia-needs #17), `world`/`source` aliases, move existing files to the Asset Table paths, catalogue types | Draft | TBD |
| 5 | World Assets panel card | HTML mockup first; status, list/load/save, stale badges | Draft | TBD |
| 6 | Consumer adoption | S-1, A4 and A1 implement `IWorldAssetParticipant`; the system and testbed specs are amended (see Consumer amendments) | Draft | TBD |

Build order: 1 → 2 → 4 → 3 → 5. Feature 6 runs per consumer as each system's module comes into existence. Feature 1 can start now.

## Consumer amendments (applied when this spec is Approved)

- **S-1:**
  - `preset.save` resolves through the `source` alias. The fallback to `out/CoW/presets/` stays.
  - The export default stays in `out/`; `terrain.bake` is the source-asset path.
  - SD-S1-010 gains `world_bundle`.
  - `terrain_rules.json` gains the envelope.
- **A4:**
  - "Parameters are not saved" becomes "saved as weather packs through F-3".
  - The parameter file moves to `Global/Weather/`.
  - The 5 named presets: see Open Q1.
- **A1:**
  - `cow.ecology.tuning.save`, which overwrites the defaults, stays for changing the defaults themselves. `pack.save_as` is the normal tuning path.
  - Paint and edits can be saved as a start state.
  - Files move to `Global/Ecology/`.
- **A3:** `succession_rules.json` moves to `Global/Succession/` and gains the envelope.
- **S-2.5:** stays preset + seed only, per its own decision; it may also accept a bundle with terrain only. Its scenario files get the envelope.
- **S-3:** the authoring loop (pack, start state, bundle) is exposed in the S3 panel through F-3's card. A mode switch still resets unsaved edits.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaAPI `CommandRegistry` | All commands | Done |
| DiaCore Json, `PathStore` / `FilePath` | Envelope, aliases | Done |
| DiaMessageBus | Events | Done |
| F-1 debug host (panel console, palette) | Panel card | Done |
| E1 SaveLoad | Not a dependency. Bundles are separate from saves. A save records the bundle id it started from, for information only | Done |
| **dia-needs #16: dev source-assets alias in `dia launch`** | `save_as` to source | **Proposed — Dia-tier, needs sign-off (AD-001)** |
| **dia-needs #17: catalogue deploy category that keeps subfolders (`world` tag → `global/world/<subpath>`)** | Bundle paths resolve the same way in dev and deployed builds | **Proposed — Dia-tier, needs sign-off (AD-001)** |
| S-1 / A4 / A1 modules | Participants | Specced, not built |

## Binding Decisions

| Source | Decision | How it constrains F-3 |
|---|---|---|
| AD-001 | Ask before Dia edits | The two DiaCLI changes (#16, #17) are proposals, not built from here |
| AD-002 | Four-tier placement | F-3 is CoW-Shared. Generic parts are kept free of CoW types so they can be extracted later |
| AD-005 | One stage per testbed, modes in panel | Authoring is a panel card inside existing stages, never a new stage |
| PD-009 | Generated output goes under `Cluiche/out/<App>/` | Authored files are source assets in `Assets/`. Reports and exports stay in `out/` |
| PD-010 | `.diagame` is the root | `default_world` lives in `cow.diagame` config |
| PD-001 | StringCRC ids | Asset ids and participant kinds are StringCRC |

## Test Strategy

- **GoogleTests (Feature 1):**
  - Merge, then diff, round-trips to the same overrides.
  - An unknown key is dropped with a Warning.
  - A missing key falls back to the default.
  - A higher version is refused.
  - Each stale case from the version table.
- **GoogleTests (Features 2–3):**
  - Loading a bundle with fake participants applies the parts in order.
  - A missing file keeps the previous world.
  - `save_as` writes the envelope and refuses to overwrite without `overwrite`.
  - Catalogue upsert is idempotent.
- **Reproducibility:** loading the same bundle twice gives identical resolved parameters and identical A1 state hashes after `Initialise` plus the start state. Run at Small and Large.
- **Integration (with S-3):**
  1. Tune → `pack.save_as` → `bundle.save_as`.
  2. Relaunch → `bundle.load`.
  3. Check the values match.
  4. Check `git status` shows the new source files.

## Resolved Decisions

1. **Dev source alias and deploy subfolders are signed off** (2026-10-19, AD-001). dia-needs #16 (`source` alias in `dia launch`) and #17 (subfolder-preserving `world` deploy category) are Approved. Each is built from its own small DiaCLI feature spec, not from F-3.
2. **Start states only on baked terrain** (2026-10-19). A start state painted on preset + seed terrain would silently break when S-1's rules change (same seed, different cells). `startstate.save_as` and `load` refuse unless the loaded terrain is a baked `.wld.json`. Can be loosened later; tightening after work is saved is harder.
   - Rejected: allow on preset + seed and go stale on a `rulesVersion` bump.
3. **The game only adds or updates its own catalogue records** (2026-10-19). Every record F-3 writes carries the `world` tag and `"author": "game"`. `dia pipeline` never deletes `author: game` records, so no lock is needed. Feature 3 tests that a pipeline run between two `save_as` calls loses nothing.

## Open Design Questions

1. **A4's 5 named presets** (coastal_storm, rain_shadow, winter_snow, spring_melt, drought). Are they parameter sets (weather packs → `Global/Weather/Packs/`) or forced weather states (scenarios → `Stages/S25WeatherPatterns/scenarios/`)? The names suggest scenarios. **A4 Feature 6 decides** from what each preset actually changes; F-3 supports either.

## Status
`Approved` (2026-10-19). Feature specs TBD; no plan yet.
