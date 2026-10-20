# System Spec: T-2 World Data Editor

**Parent:** @docs/specs/applications/cow/cow.md  
**Origin:** 2026-10-19 world-sim product review. The user raised three needs: species, biome and rule tables will outgrow hand-editing; non-programmers will author content; many bundles and packs need browsing and comparing. They also asked for explanations on every value.  
**Mockup:** [mockups/world-data-editor.html](mockups/world-data-editor.html)  
**Built on:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md, @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md, @docs/specs/applications/cluicheeditor/systems/pluginbrowser/game-scoped-plugins.md, DiaAssetCatalogueEditor (`AssetTypeEditorRegistry`, `open_asset`)  
**Sibling:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md

**Gameplay Domains:** debug, tools

## Summary

T-2 is a CluicheEditor plugin for editing CoW's world-sim data files offline. It covers every F-3 asset type:
- **data tables:** species, biomes, succession rules
- **defaults files:** terrain rules, weather and ecology parameters
- **packs:** shown as diffs against the defaults, with compare
- **bundles**
- **scenarios**
- **baked worlds and ecology start states:** full cell editing

Every form, column and cell is generated from DiaValueSchema, so labels, units, ranges, tooltips and cross-file checks come from one definition. The Asset Catalogue opens files in T-2 through `open_asset`.
- **Validation** runs the same validator as `dia validate schema`.
- **Saving while CoW is connected** offers to reload the file in the running game.

Like T-1, it is game-scoped: it exists only while `cow.diagame` is loaded.

### Tool tier

This is the **Editor** tier: offline files. Testbeds remain the place to tune by watching (F-3 Save-as). T-2 is for structured data entry, review and bulk edits. Both write the same files with the same envelope and catalogue records.

## Goals

1. **Edit tables safely.** Species, biome and rule tables are edited as tables with typed columns. Cross-file references (a species naming a biome) are checked as you type and offer "did you mean" fixes.
2. **Explain every value.** Every column, field and cell has a `ValueLabel` and `ValueTooltip` from the schema.
3. **Packs as diffs.** A pack is shown as overrides against the defaults. You can edit an override, reset it to the default, upgrade a stale pack, and compare two packs side by side.
4. **Compose bundles.** A bundle is built from pickers (terrain, weather pack, ecology pack, start state), with "used by" links in both directions.
5. **Edit cells.** Baked worlds and start states can be edited cell by cell or by brush on a hex canvas, with the same layers and ramps as the game.
6. **One truth for validity.** The editor and `dia validate schema` never disagree.
7. **Close the loop.** On save, a connected CoW game can reload the file. "Open in testbed" loads the bundle there.

## Non-Goals

- **Running the simulation or regenerating terrain.** Generation and shaping stay in S-1. T-2 edits a baked result.
- **Defining schemas.** Schema files are hand-written by developers. T-2 reads them; it doesn't author them.
- **Runtime tuning.** That is the testbed sliders and F-3 packs.
- **Analysis.** That is T-1.

## Architecture

### Placement

`Cluiche/CoW/Editor/WorldDataEditor/` in the `CoWEditor` static library, next to T-1. It is listed in `cow_editor.diaapp`.

The hex canvas is generic, so it goes in Dia:
- The cell-grid canvas component that T-1's mini-map and T-2's cell editor share is a **generic** `CellGridCanvas` in `@dia/editor-ui` (`Dia/DiaEditorUI`).
- It draws flat-top hex or square grids with a ramp or palette, hover, selection and brush.
- Its placement follows the layer boundary rule: any game's editor could use it.

```
CoWWorldDataEditorPlugin : IEditorPlugin            (C++)
  ├─ OnLoad: AssetTypeEditorRegistry::RegisterTypeEditor for
  │          every F-3 format id (= catalogue type): cow.world_bundle, cow.weather_pack, cow.ecology_pack,
  │          cow.ecology_start, cow.world, cow.worldshape, cow.species, cow.biomes, cow.succession_rules,
  │          cow.weather_params, cow.ecology_params, cow.terrain_rules, cow.scenario
  │   (unregistered in OnUnload → out of scope ⇒ OS default app, per Game-Scoped Plugins)
  ├─ FileService      read/write source files (F-3 envelope), catalogue upsert via DiaAssetCatalogue
  ├─ SchemaService    loads *.schema.json (DiaValueSchema loader, same code as runtime)
  ├─ ValidateService  runs `dia validate schema --json --file <f>` and parses the result
  ├─ CommandHistory   IEditorCommand per edit (undo/redo), DiaEditor CommandHistory
  └─ GameLink         if CoW is connected: reload / open-in-testbed commands

UI (React 18 + zustand + @dia/editor-ui, Vite)     Cluiche/CoW/Editor/WorldDataEditor/UI/
  model/   pure TS: documents, schema-driven field model, diff, refs index, cell edits (unit-tested)
  views/   AssetTree · TableEditor · FormEditor · PackDiff · PackCompare · BundleComposer
           ScenarioEditor · CellEditor (CellGridCanvas) · Properties · ValidationPanel
```

The UI is kept separate from the editing logic (feedback rule): views call `model/` and the bridge only through the store.

### Schema extensions (DiaValueSchema amendment)

Tables need two generic additions to DiaValueSchema. They belong in Dia because every game has tables; this was a user decision on 2026-10-19.

| Addition | Form | Meaning |
|---|---|---|
| Record paths | `"key": "species[].growth_rate"` | One entry describes a column of an array of records |
| Unique | `"unique": true` | Values must be unique within the array (ids) |
| References | `"type": "ref"` or `"ref[]"`, with `"ref": "cow.biomes/biomes[].id"` | The value must equal an id in another format's record array |
| Required in record | `"required": true` | The column must be present in every record |

- `dia validate schema` gains the ref, unique and required checks, with a closest-match suggestion.
- T-2 builds a **refs index** across all loaded files for live checks and for "used by".

### Views

- **Asset tree:** grouped by system (WorldGen, Weather, Ecology, Succession, Worlds, Scenarios). Each item has type icons, stale and invalid badges, and a dirty dot. Filter by text, type or status.
- **Table editor** (species, biomes, rules):
  - Columns come from schema record paths.
  - Cells are typed: number with unit and range, enum dropdown, ref picker with search, ref[] chips.
  - Errors are shown inline with a fix suggestion.
  - Header tooltips come from the schema.
  - Rows can be added, duplicated and deleted. Deleting a row that others reference warns and lists the referrers.
- **Form editor** (defaults files): fields grouped by `ui.group`, with sliders and inputs from the schema, and the default shown. Changing a defaults value warns which packs override that key.
- **Pack diff:** shows key, default, pack value and Δ, overrides only by default, with a "show all keys" toggle.
  - Edit, reset to default or add an override.
  - A stale banner offers "upgrade": drop unknown keys and re-base to the current version.
- **Pack compare:** two packs of the same kind side by side, highlighting differences, with copy-across.
- **Bundle composer:** pickers for each part, each showing its stale and validity state.
  - Terrain can be baked or preset + seed + size.
  - "Used by" lists the bundles that reference a file.
  - "Open in testbed" runs `cow.world.bundle.load <id>` in the connected game.
- **Scenario editor:** an ordered step list (setup, actions, checks) with the fields of each step type from the scenario schema (`cow.scenario`).
- **Cell editor** (baked worlds, start states):
  - Shows `CellGridCanvas` with a layer picker. Layers come from the `.wld.json` fields, using their schema ramps and palettes.
  - Tools: select (click, shift, box), brush with radius, fill selection, set field on selection.
  - The inspector shows the selected cells' values with tooltips.
  - For start states, cells are shown over their baked world. Only edited cells are saved (sparse overlay), and edited cells are outlined.
- **Validation panel:** errors and warnings with file:key and click-to-jump. It shows the last `dia validate` run and live field checks.

### Save, validate and game link

**Save flow:**
1. Live checks pass, or you confirm saving with warnings.
2. The file is written with the F-3 envelope, and the version is bumped if the format requires it.
3. The catalogue record is added or updated.
4. `dia validate schema --json --file <f>` runs.
5. If CoW is connected, T-2 offers **Reload in game**: `cow.world.pack.load …`, `cow.weather.reload`, `cow.ecology.reload_data` or `cow.world.bundle.load`, depending on the type.

**Baked-world edits:**
- Saving changes the world's content hash, so start states and bundles that use the world are shown as stale (F-3 version rule), with an "update start states" option that re-keys them.
- World guarantees (reachability and so on, S-1) are not re-checked offline.
- If CoW is connected, Save offers **Validate in game**: `cow.worldgen.validate <file>`.
- Otherwise the file is flagged "unvalidated edit" until it is checked.

**Undo and redo:** every edit is an `IEditorCommand`, in a per-document history.

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** open/save, validation summary, reload sent, ref-index rebuild time.
- **Health:** a document that fails to parse is shown read-only with an error.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Plugin shell, files and schema | `CoWEditor` plugin, `open_asset` routes (game-scoped), FileService, SchemaService, asset tree, Properties, undo/redo | Draft | TBD |
| 2 | Table and form editors | Typed cells from schema, refs index, live checks and suggestions, referrer warnings | Draft | TBD |
| 3 | Packs and bundles | Pack diff, upgrade, compare; bundle composer; used-by | Draft | TBD |
| 4 | Validation and game link | `dia validate schema --json`, validation panel, reload in game, open in testbed, validate in game | Draft | TBD |
| 5 | Scenario editor | Step list from the `cow.scenario` schema | Draft | TBD |
| 6 | Cell editor | `CellGridCanvas` (Dia `@dia/editor-ui`), layers, brush and selection tools, start-state overlay, hash and stale propagation | Draft | TBD |

- **Build order:** 1 → 2 → 4 → 3 → 6 → 5.
- Feature 2 needs the DiaValueSchema record-path and ref extension.
- Feature 6 shares `CellGridCanvas` with T-1 Feature 3; whichever is built first creates it.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Game-Scoped Editor Plugins | Exists only with `cow.diagame` | Approved, not built (AD-001 sign-off) |
| DiaAssetCatalogueEditor `AssetTypeEditorRegistry`, `open_asset` | Routing | Done |
| DiaEditor `CommandHistory` / `IEditorCommand` | Undo and redo | Done |
| DiaValueSchema (+ record paths, refs, unique) | Columns, tooltips, checks | Approved, not built; extension added by this spec |
| `dia validate schema` | One validator | Approved, not built |
| F-3 World Assets | File types, envelope, catalogue types, stale rules | Approved, not built |
| `@dia/editor-ui` `ValueLabel`, `ValueTooltip`, new `CellGridCanvas` | UI | Tooltips Approved; canvas added by this spec |
| CoW commands `cow.world.*`, `cow.weather.reload`, `cow.ecology.reload_data`, `cow.worldgen.validate` | Game link | `cow.worldgen.validate` is new (S-1 amendment) |

**Amendments when Approved:**
- **DiaValueSchema:** record paths, `unique`, `required`, and `ref`/`ref[]` with validator checks.
- **`@dia/editor-ui`:** a `CellGridCanvas` component.
- **S-1:** a `cow.worldgen.validate <file>` command that runs the guarantee validators on a baked world.
- **T-1:** the mini-map uses `CellGridCanvas`.
- **cow.md:** register T-2.

## Binding Decisions

| Source | Decision | Constraint on T-2 |
|---|---|---|
| CoW AD-001 | Ask before Dia / CluicheEditor / shared build edits | The `@dia/editor-ui` canvas, the DiaValueSchema extension and the `.vcxproj` reference need sign-off when built |
| CoW AD-002 | Four-tier placement | CoW editor code in `Cluiche/CoW/Editor/`; the generic canvas and schema extension in Dia |
| AED-002, AED-005 | `.diaapp` editor section; React stack | `cow_editor.diaapp`; React 18 |
| F-3 | Envelope, layout, load-and-warn, start states only on baked terrain | T-2 writes the same files the same way, and refuses start states without a baked world |
| VS-004 | Strict validation | The editor uses the CLI validator; no second rule set |
| Feedback: editor actions via DiaAPI | Every action is a command | T-2 actions are registered editor actions (`EditorActionParam` / `ValueParam`), so they can be scripted |

## Test Strategy

- **Vitest (model/):**
  - schema → column model
  - ref index and "used by"
  - live checks (range, enum, ref, unique, required) and suggestions
  - pack diff, reset and upgrade
  - compare
  - cell brush, fill and sparse start-state output
  - hash change marks dependants stale
- **React Testing Library:** every view from fixtures; tooltips present for every column and field.
- **GoogleTests (C++):**
  - FileService round-trips the envelope.
  - Catalogue upsert is idempotent.
  - The validate result parses.
  - `open_asset` routes are registered on load and removed on unload.
- **E2E:**
  1. Open `species.json` from the Asset Catalogue.
  2. Enter "Forest" and see the inline error with its suggestion.
  3. Fix it and save; `dia validate` reports 0 errors.
  4. With a stub game connected, "Reload in game" sends the expected command.
- **Visual gate:** against the mockup.

## Resolved Decisions

1. **On-disk changes: watch and offer reload or keep mine** (2026-10-19). If a file open in T-2 changes on disk (for example a testbed `save_as`), T-2 shows "Changed on disk: Reload / Keep mine". There is no merging in v1.
2. **Unvalidated world edits: warn, block only the default world** (2026-10-19). An "unvalidated edit" baked world shows a warning everywhere. The composer refuses "set as `default_world`" for a bundle that uses it until `cow.worldgen.validate` passes.
3. **Baked worlds and start states are fully editable** (user decision 2026-10-19), with the hash and stale propagation described in Save, validate and game link.

## Status

`Approved` (2026-10-19). Feature specs TBD; no plan yet. Build needs AD-001 sign-off (`@dia/editor-ui` canvas, DiaValueSchema extension, `.vcxproj` reference).
