# Feature Spec: F-3 Deploy and Aliases

**Parent:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md
**Builds on:** @docs/specs/applications/cow/systems/f3-worldassets/world-assets-module-and-bundle-load.md (Feature 2), @docs/specs/applications/cow/systems/f3-worldassets/save-as.md (Feature 3), @docs/specs/applications/dia/systems/diacli/preserve-subfolder-deploy.md (dia-needs #17), @docs/specs/applications/dia/systems/diacli/dev-source-alias.md (dia-needs #16)

**Gameplay Domains:** debug

## Summary

Makes a world asset findable in every kind of build. It delivers:

- the **`world` alias** in `cow.diagame` and the CoW use of the dev-only **`source` alias**;
- an **`AssetLocator`** in `WorldAssetsModule` that resolves, lists and reads world assets: the `source` tree first in a dev launch, the deployed `world` tree otherwise;
- the **single asset-kind table** (folder, extension, format id) that the loader and the savers share;
- the **deploy contract** for CoW: `world`-tagged catalogue records deploy to `global/world/<path below Global/>` (dia-needs #17);
- the checks that keep the catalogue, the source tree and the deployed tree in agreement.

The Dia changes (#16 and #17) are built from their own feature specs; this feature is the CoW side of them.

## Problem

F-3 writes assets to the source tree and the game loads them from a runtime path. In a dev session the loop must work without a pipeline run (save a pack, list it, load it), and in a packaged build there is no source tree at all, so bundle-relative paths like `WorldGen/Baked/x.wld.json` must resolve the same way against the deployed layout. Without one resolver and one set of tests, the two cases drift and a bundle loads in dev but not in the build.

## Goals

1. **One resolver.** All reads and listings of world assets go through `AssetLocator`; no module builds a path itself.
2. **Dev loop without a pipeline run.** In a dev launch, assets just saved are listed and loadable at once.
3. **Same relative paths everywhere.** A path relative to `Global/` means the same file in the source tree and the deployed tree.
4. **Deployable.** Every game-authored asset has a `world`-tagged record that the pipeline deploys with its subfolders intact.
5. **Safe.** The pipeline keeps game-authored records; a catalogue that disagrees with the files is caught by a test.
6. **One table.** The kinds, folders, extensions and format ids are defined once.

## Non-Goals

- The Dia changes themselves (#16 `source` alias, #17 subfolder deploy), built from their own specs.
- Writing assets (Feature 3) and loading bundles (Feature 2); they only call `AssetLocator`.
- Moving or renaming existing files: no world-sim data file exists in the repository yet (the defaults files are created by their owning systems' first features), so there is nothing to move.
- Catalogue pruning (a future command must skip `author: game` records).
- Asset hot-swap or file watching.

## Architecture

`Cluiche/CoW/Shared/WorldAssets/` (CoW-Shared), additions to the Feature 2 module.

```
WorldAssetKinds    the table: kind, folder under Global/, extension, format id, owner
AssetLocator       Resolve / Exists / Read / List over `source` (dev) and `world` (deployed)
```

### Aliases

| Alias | Value | Where it comes from |
|---|---|---|
| `world` | `./global/world`, the deployed `global/world/` tree | `cow.diagame` `config.path_aliases`, present in every build |
| `source` | `Cluiche/Assets/CoW/`, absolute | `dia launch` and `dia run` only, via `--dev-alias` (dia-needs #16). **Reserved:** `cow.diagame` must not declare it (validate rejects it, the runtime ignores it) |
| `root`, `ui_common` | unchanged | unchanged |

This feature adds one line to `cow.diagame`: `"world": "./global/world"` in `path_aliases`. The file has CRLF line endings, so the edit is made as a text change that leaves every other byte alone (the same rule as Feature 3's `set_default`).

### `AssetLocator`

All paths are **relative to `Global/`** (the form used inside bundles, for example `Ecology/Packs/lush.ecology.json`).

```cpp
class AssetLocator {
public:
    ResolvedPath Resolve(const char* relPath) const;     // { absolute path, origin: Source | Deployed } or not found
    bool         Exists(const char* relPath) const;
    bool         Read(const char* relPath, std::string& text) const;
    void         List(const char* relFolder, const char* extension, AssetList& out) const;   // ids + origin
    bool         HasSource() const;                      // the `source` alias is present
};
```

- **Order.** When `source` is present (a dev launch), `Resolve` tries `<source>/Global/<rel>` first and falls back to `<world>/<rel>`. Without `source` (a packaged build) only `<world>/<rel>` is used. Nothing else in the module knows which tree a file came from.
- **Listing** is the union of both trees by id; when an id exists in both, the source copy wins and is reported with origin `Source`. A file present in only one tree is listed with that origin.
- **Differing copies.** When both trees hold the same relative path and the byte sizes differ, an Info message says the source copy is used (a deploy would update the other).
- **Containment.** A relative path containing `..`, a drive letter or a leading separator is rejected; nothing outside the two roots is ever opened.
- Feature 2's `BundleResolver`, `pack.list`, `bundle.list` and `startstate.load` use `AssetLocator`; Feature 3 uses it to detect existing files before an overwrite.

### The asset-kind table

One table (a `constexpr` array in `WorldAssetKinds.h`), matching the parent Asset Table. Features 2 and 3 and the tests read it; nothing repeats a folder or extension as a string.

| Kind | Folder under `Global/` | Extension | Format id | Owner |
|---|---|---|---|---|
| terrain rules (defaults) | `WorldGen/` | `.json` (`terrain_rules.json`) | `cow.terrain_rules` | S-1 |
| world-shape preset | `WorldGen/Presets/` | `.worldshape.json` | `cow.worldshape` | S-1 |
| baked world | `WorldGen/Baked/` | `.wld.json` | `cow.world` | S-1 |
| weather parameters | `Weather/` | `.json` (`weather_parameters.json`) | `cow.weather_params` | A4 |
| weather pack | `Weather/Packs/` | `.weather.json` | `cow.weather_pack` | A4 |
| species, biomes, ecology parameters | `Ecology/` | `.json` | `cow.species`, `cow.biomes`, `cow.ecology_params` | A1 |
| ecology pack | `Ecology/Packs/` | `.ecology.json` | `cow.ecology_pack` | A1 |
| ecology start state | `Ecology/StartStates/` | `.ecostate.json` | `cow.ecology_start` | A1 |
| succession rules | `Succession/` | `.json` | `cow.succession_rules` | A3 |
| world bundle | `Worlds/` | `.cowworld` | `cow.world_bundle` | F-3 |

(Scenarios and testbed baselines stay in `Stages/<Stage>/` and are outside this table.)

### Deploy contract

Provided by dia-needs #17; CoW's side is the data it feeds:
- Every record F-3 writes has tag `world`, `scope` `global` and a `source_path` of the form `Global/<folder>/<file>`, so it deploys to `global/world/<folder>/<file>` and the `world` alias resolves `<folder>/<file>`, the same relative path used in a source launch.
- Hand-authored defaults files that must load in packaged builds get the same `world` tag in the catalogue by hand, with `source_path` under `Global/`.
- A `world`-tagged record whose path is not under `Global/` still deploys, with a Warning from the pipeline (#17 AC5); the CoW consistency test below fails on it.
- A future catalogue `prune` must skip `"author": "game"` records (recorded in #17 and kept here as a requirement).

### Consistency test (CoW)

A GoogleTest reads the real `assets.catalogue.json` and the source tree and fails if any `world`-tagged record:
- has a `source_path` outside `Global/` or an extension that does not match its `type` in the asset-kind table;
- names a file that does not exist in the source tree;
- has a `type` that is not a format id in the table.

and if any file under `Global/` with a table extension has no catalogue record. This catches a hand-added file the pipeline would never deploy.

### Runtime tolerance of the new record types

The catalogue gains record types that are format ids (`cow.weather_pack`, …). The asset pipeline does not restrict types, but the runtime that loads the catalogue at boot may warn on an unknown one. This feature adds a check at the first boot of a CoW build with such records: it must log no Error or Warning about them. If it does, CoW registers a passive handler for those types (records tracked, nothing loaded), a CoW-side change only.

### Logging and observation

- **Channel `WAS`.** Info: which tree a file was read from when both exist and differ; the active alias set at module start (`source` present or not). Warning: a path rejected by the containment rule.
- **Metrics:** reads by origin (source, deployed), lists, misses.
- **Health:** none beyond Feature 2's.

## Acceptance Criteria

1. `cow.diagame` declares `world: ./global/world` and does not declare `source`; every other byte of the file is unchanged.
2. In a dev launch (the `source` alias present), `AssetLocator::Resolve` finds a file that exists only in the source tree and prefers the source copy when both exist.
3. In a launch without `source`, the locator reads only the `world` tree and never touches `Assets/`.
4. `List` returns the union of both trees by id, source winning, each with its origin.
5. A relative path containing `..`, a drive letter or a leading separator is rejected without opening anything.
6. A pack or bundle saved by Feature 3 appears in `pack.list` / `bundle.list` and loads in the same session with no pipeline run.
7. A fixture bundle with subfolder paths resolves identically from a source-style tree and from a deployed-style tree (`global/world/…`), producing the same resolved files.
8. After `dia pipeline --target cow`, every game-authored record's file exists at `global/world/<path below Global/>` and the `world` alias resolves it in a launch without `source`.
9. The consistency test fails on each of: a `world` record outside `Global/`, a wrong extension for its type, a missing source file, an unknown type, and an unrecorded file under `Global/`; it passes on the real catalogue.
10. The asset-kind table is the only place folders, extensions and format ids appear; the Feature 2 and 3 code and tests reference it.
11. A CoW boot with `world`-type records logs no Error or Warning about them, or the passive handler is registered and then the boot is clean.
12. The locator logs one Info line when source and deployed copies of a path differ in size.

## Test Strategy

- **GoogleTests (TDD):** `AssetLocator` over temporary source and deployed trees (each order case, union listing, containment rejections, unreadable files); the kind table against the parent Asset Table; the consistency test on both fixtures and the real catalogue; the `cow.diagame` text edit on a copy of the real file.
- **Integration (pytest, DiaCLI):** copy the CoW assets to a scratch folder with one `world`-tagged fixture record and run `dia pipeline --target cow`; assert the deployed path (this reuses #17's own tests for the path rule and adds the CoW layout).
- **Smoke (manual, until CoW has a headless launch):** `dia launch cow`, save a pack, list and load it, then deploy and relaunch without `source` and load it again.
- No UI or E2E: the card is Feature 5.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| AD-001 | Ask before Dia edits | The Dia changes (#16, #17) are signed off and built from their own specs; this feature edits only CoW files |
| AD-002 | Four-tier placement | CoW-Shared |
| PD-009 | Authored files are source assets; generated output is under `out/` | Assets live under `Assets/CoW/Global/` and deploy |
| PD-010 | `.diagame` is the root | The `world` alias is declared in `cow.diagame` |
| #16 | `source` is reserved and dev-only | `cow.diagame` never declares it; the locator treats it as optional |
| #17 / parent Resolved Decision 3 | `world`-tagged records keep subfolders; `author: game` records are not pruned | The record contract above and the requirement on future prune |

No conflicts. This feature amends one parent statement: the parent says `world` "is used for loading in every build"; in a dev launch the locator looks in `source` first (Resolved Decision 1), and the parent and Feature 2 are updated to say so.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| dia-needs #17 (subfolder deploy) | Deploy path | Approved, build signed off, not built |
| dia-needs #16 (`source` alias) | Dev lookup and saves | Approved, build signed off, not built |
| Features 2 and 3 | Callers of the locator | Approved |
| `PathStore` aliases (DiaCore) | Alias resolution | Done |
| `dia pipeline`, `dia validate manifest` | Deploy and validation | Done (extended by #16, #17) |

## Resolved Decisions

1. **Source first in a dev launch** (2026-10-20). A saved pack must be listable and loadable in the same session, or the tune, save, load loop would need a pipeline run each time. The risk is a file that exists in source but never deploys; the consistency test and the post-pipeline check (AC 8, 9) catch it. Deployed builds have no `source` alias and read `world` only.
2. **Nothing to move yet** (2026-10-20). No world-sim data file exists in the repository, so the parent's "move existing files" becomes "each system creates its defaults at the Asset Table path in its first feature" (S-1, A4, A1, A3 Feature 1).
3. **The runtime-tolerance check is part of this feature** (2026-10-20), because it is the first time the catalogue carries format-id types; a passive handler is the agreed fallback.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. The Dia side is dia-needs #16 and #17 (build signed off, not started).
