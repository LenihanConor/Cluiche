# Feature Spec: T-2 Cell Editor

**Parent:** @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md
**Builds on:** @docs/specs/applications/cow/systems/t2-worlddataeditor/plugin-shell-files-and-schema.md (Feature 1), @docs/specs/applications/cow/systems/t2-worlddataeditor/table-and-form-editors.md (Feature 2, cell editors and checks), @docs/specs/applications/cow/systems/t2-worlddataeditor/packs-and-bundles.md (Feature 3, used-by and bundle checks), @docs/specs/applications/cow/systems/t2-worlddataeditor/validation-and-game-link.md (Feature 4, save flow and validate in game), @docs/specs/applications/dia/systems/diaeditorui/cell-grid-canvas.md
**Mockup:** none yet; a cell-editor mockup is required before this feature is built (see Mockup gate)

**Gameplay Domains:** debug, tools

## Summary

Edits the two cell-based files: **baked worlds** (`cow.world`, `.wld.json`) and **ecology start states** (`cow.ecology_start`, `.ecostate.json`). A hex canvas shows one layer at a time with the same ramps and palettes as the game; tools select cells, paint them with a brush, fill a selection and set a field on it; an inspector shows and edits the selected cells' values with tooltips.

For a baked world it edits the cells in the file. For a start state it edits a **sparse overlay** drawn over its baked world; only edited cells are saved. Saving a world changes its content hash, so the editor lists the start states and bundles that depend on it as broken and offers to re-key them, and it marks the world an **unvalidated edit** until the game checks it.

## Problem

World shape comes from S-1's generator, but designers need to fix a placement, flatten a coast, paint a test valley or build a start state for a testbed, by looking at the map. Hand-editing a 6,656-cell JSON array is not feasible. The edit has to be visual, undoable and safe: it must not silently invalidate the start states keyed to the world, and it must not claim a world is valid when only the game can say so.

## Goals

1. **See the map.** Any per-cell field as a layer, with the schema's ramps, palettes, legend and tooltips.
2. **Edit by looking.** Select, brush, fill, set a field on a selection, eyedropper, bucket.
3. **Edit in one undo step per gesture**, with a live preview that does not flood the history.
4. **Start states as overlays.** Edit cells over their world; save only what changed.
5. **Honest consequences.** A saved world change breaks dependants visibly, with a re-key path, and is flagged unvalidated until the game validates it.
6. **Same layers as the game.** Layer names, colours and legends come from the schema, not from editor code.

## Non-Goals

- Generating or reshaping terrain (S-1) and re-checking world guarantees offline (the game does that; Feature 4's Validate in game).
- Editing the derived fields `latitude`, `terrain_flags` and `downstream` (see Editable fields).
- Editing the derived `regions[]` summaries (recomputed by S-1 on import).
- Editing `sites` (nodes and sites belong to the S-1 panel and the dragon layer).
- Editing other cell-based files (none exist yet).
- A WebGL renderer, animation or time scrubbing.

## Architecture

Views in `Cluiche/CoW/Editor/WorldDataEditor/UI/views/`, logic in `model/`. No new C++ beyond Feature 1's file services and Feature 3's `WorldContentHash`.

```
model/
  cellModel.ts       typed arrays per field, built from the document and updated incrementally from ops
  layers.ts          schema entries → layers (kind, ramp or palette, editable)
  footprint.ts       hex distance, cells within radius, box, bucket fill (flat-top odd-q)
  strokes.ts         preview buffer, commit → one batch of set ops
  overlayModel.ts    start state = base world (read-only) + sparse overlay
  impact.ts          dependants of a world; re-key plans
views/
  CellEditor (canvas, toolbar, layer picker, legend) · CellInspector · RekeyDialog · ImpactList · RegionsList
```

### The canvas

`CellGridCanvas` (Dia, `@dia/editor-ui`) gains the extensions this feature needs, specified in that component's own spec (amended with this feature): selection and outline sets, box-select and stroke callbacks, a brush-footprint preview set, zoom and pan, and exported geometry helpers (`hexDistance`, `cellsWithin`, `cellsInBox`). It stays generic: it draws and reports; the editor decides what a stroke means.

### Cell model

The document value is a large JSON tree (`world.cells[i].biome`). Views do not walk it. `cellModel.ts` keeps one typed array per field (`Uint8Array` for category ids, `Float32Array` for numbers), built when the document opens and **updated incrementally by observing the ops** applied to the document (a `set` on `world.cells[i].field` writes one entry), so undo, redo and reloads keep the arrays and the JSON in step. The canvas reads the arrays.

Cell index is `row * width + column` (`y * width + x` in the file), matching DiaCellLayers.

### Layers

Layers come from the schema sidecar for the world's cell fields (`for: cow.world`, keys `world.cells[].<field>`), each with its kind, enum values or range, `ui.ramp` or palette, `label`, `unit` and `ui.default_base`. One layer is shown at a time; the legend and tooltips use the shared schema text. The editor adds no colours of its own. (The sidecar is authored with S-1; dependency below.)

### Editable fields

| Field | Editing |
|---|---|
| `biome`, `terrain`, `water_source`, `depth_zone` | enum values from the schema |
| `elevation` | number in the schema range |
| `region` | a region id from the file's `regions[]` |
| `landmass` | a small integer id |
| `latitude`, `terrain_flags`, `downstream` | **read-only**: shown on the canvas and in the inspector, never painted. `latitude` follows the row and the world's latitude range; the flags and river directions follow terrain and rivers, and a hand-edited direction can create a loop the game's guarantees would reject |

Values outside a field's range or enum are refused in the editor (a brush cannot create one); the inspector can still type one, which is flagged by Feature 2's checks.

### Tools

| Tool | Behaviour |
|---|---|
| **Select** | click selects a cell, shift-click toggles, drag selects a box, Ctrl+A selects all, Esc clears |
| **Brush** | paints the chosen field's value on the cells within radius 0 to 6 (hex distance) of the pointer; a footprint preview follows the pointer |
| **Fill selection** | sets the chosen field to the chosen value on every selected cell |
| **Bucket** | sets the value on the contiguous region of cells that share the clicked cell's value (neighbours by hex adjacency) |
| **Eyedropper** | picks the clicked cell's value into the tool (also Alt+click in any tool) |
| **Clear field** | start states only: removes the field from the clicked or selected cells' overlay |
| **Pan / zoom** | middle-drag or space-drag pans; the wheel zooms; Fit resets |

**Gesture to history.** While a brush stroke is in progress the changes live in a preview buffer that the canvas draws; on release the stroke is committed as **one `batch`** of `set` operations labelled "Brush biome (37 cells)". Fill and bucket commit one batch each. So one gesture is one undo step, and the history never holds per-pixel noise. A stroke that changes nothing commits nothing.

### Inspector

The Properties panel shows the selection: the count; for each field either the value (when all selected cells agree) or "mixed (N distinct values)"; the schema tooltip for each field. Editing a field in the inspector sets it on every selected cell as one batch. Read-only fields display without an editor. For a single cell it also shows its region and, for a start state, whether the cell is in the overlay.

### Baked world

- The canvas shows the world's grid (`world.topology` must be `hex_flat_odd_q`, otherwise the file opens in the raw view with the reason).
- A **Regions** list shows the file's `regions[]` read-only, marked "recomputed on load": S-1 recomputes `cell_count`, `primary_biome`, `purity`, `centre_cell` and `neighbours` from the cells when it imports (S-1's JSON import is amended). The cells are the truth.
- **Save.** Feature 4's save flow, plus:
  1. the catalogue record's `validated` flag is set to `false` (an **unvalidated edit**; T-2 may set this flag on any record, including game-authored ones, since it is exactly what the flag is for);
  2. the new `WorldContentHash` of the saved text is compared with the old; if it changed, the **impact dialog** opens (below);
  3. **Validate in game** is offered when connected (Feature 4); a pass clears the flag.
- The `provenance` block is left as it is. (An edited world is no longer the output of its recorded seed and parameters; S-1 opens imports locked for that reason.)

### Hash and stale propagation

`impact.ts` finds the **dependants** of a baked world: start states whose `world.baked` names it, and the bundles that use the world or those start states (Feature 3's used-by). After a save that changed the hash the `ImpactList` shows each start state as **broken** (its recorded hash no longer matches, so the game would refuse it at load) and each bundle that uses one as **affected**. The `RekeyDialog` offers, for the listed start states:

- **Re-key**: set each start state's `world.hash` to the new hash. Its overlay cells are kept.
- **Re-key and drop overlay cells on edited cells**: as above, but remove overlay entries for cells whose base values changed in this edit, so the overlay never overrides a deliberate world change.
- **Leave broken.**

Re-keying edits those start-state documents in memory (dirty, with their own undo); nothing is written until each is saved. A start state also shows the broken marker whenever it is opened against a world whose hash differs from the recorded one.

### Start state

- **Document:** `{ "format": "cow.ecology_start", "world": { "baked": id, "hash": … }, "cells": [ { "q", "r", <field>: <value>, … } ] }`.
- **Base world** is read from `Global/WorldGen/Baked/<id>.wld.json` through `FileService`, read-only. If it is missing, the editor shows an error and opens the overlay in the raw view. If its hash differs from the recorded one, a banner says so and offers Re-key; editing is still allowed.
- **Layers** are the base world's terrain layers (read-only) plus the **overlay fields** from A1's per-cell field schema (`for: cow.ecology.cell`: for example `producers_total`, species populations, `biome`). The overlay layer shows set values coloured and unset cells in a neutral "not set" shade; terrain layers show the base.
- **Editing.** Brush, fill and the inspector write overlay fields; **Clear field** removes them; a cell with no fields left is removed from the list. A value equal to "not set" is never stored. Edited cells are outlined on every layer.
- **Names.** Field names are A1's `set_field` names; unknown names are flagged. Painting `biome` is stored as the `biome` field, which the game applies by reinitialising that cell (noted in the tooltip).
- **Save** writes only edited cells, **sorted by row then column**, with the stable writer, the same order A1's capture uses.

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** a stroke or fill committed (field, value, cell count), a world saved with hash old and new, dependants found and re-keys applied, the validated flag changes, a start state opened against a different hash.
- **Profiling:** `performance.mark` around render, stroke preview and commit; slow commits logged.

## Acceptance Criteria

1. A baked world opens in the cell editor with a canvas of the file's grid and a layer picker listing the schema's cell-field layers with the game's ramps, palettes, legend and tooltips; a world with another topology opens in the raw view with the reason.
2. Each tool behaves as in the table; the brush footprint (radius 0 to 6) uses hex distance and is previewed under the pointer; the bucket fills the contiguous same-value region.
3. A brush stroke, a fill, a bucket and an inspector edit each commit as exactly one `batch` of `set` operations with a readable label; undo and redo restore the world exactly; a stroke that changes nothing commits nothing.
4. During a stroke the changes appear in the canvas from a preview buffer, and the document and history change only on release.
5. `cellModel` arrays match the document after any sequence of edits, undos and redos (randomised test with a fixed seed), and after a reload from disk.
6. `latitude`, `terrain_flags` and `downstream` cannot be painted or edited and display read-only; the other fields reject values outside their range or enum in the tools.
7. The inspector shows the count, a value or "mixed (N distinct values)" per field, and the schema tooltip; editing a field sets it on all selected cells in one undo step.
8. The Regions list shows `regions[]` read-only and marked "recomputed on load".
9. Saving a world uses Feature 4's save flow, sets the catalogue `validated` flag to `false`, and offers Validate in game when connected; a pass clears the flag.
10. If a save changes the content hash, the impact dialog lists the start states keyed to the world as broken and the bundles using them as affected, and offers Re-key, Re-key and drop edited-cell overlays, or Leave broken; the choices edit the start-state documents in memory with their own undo.
11. A start state opens over its baked world (read-only base), shows overlay fields coloured and unset cells neutral, outlines edited cells on every layer, and shows a banner with Re-key when the hash differs or an error when the world is missing.
12. Editing a start state stores only edited cells, removes a cell when its last field is cleared, never stores a "not set" value, and saves them sorted by row then column using A1's field names.
13. Unknown overlay field names and out-of-range values are flagged by Feature 2's checks.
14. At Large (104 × 64), pointer movement with the brush preview stays at 60 fps, committing a 2,000-cell batch takes under 100 ms, and undoing it takes under 100 ms.
15. The extended `CellGridCanvas` props (selection, outline, box and stroke callbacks, footprint preview, zoom and pan, geometry helpers) exist and pass the canvas's own tests.
16. The visual result matches the cell-editor mockup (see Mockup gate).

## Mockup gate

The T-2 mockup has no cell editor. Before this feature is built a mockup is created and approved, showing: the hex canvas with the layer picker and legend, the tool strip with brush radius, the selection and footprint preview, the inspector with "mixed" values and read-only fields, the Regions list, the start-state overlay (outlined edited cells, "not set" shade, hash banner), the impact dialog and the re-key choices, and the unvalidated-edit badge.

## Test Strategy

- **Vitest (model/):**
  - `footprint` against hand-computed hex distances, boxes and bucket regions on odd and even rows.
  - `cellModel` consistency over randomised op sequences (fixed seed), including undo and redo and reload.
  - `strokes`: preview versus commit, no-op strokes, batch labels and inverses.
  - `overlayModel`: sparse storage, removal of empty cells, sorted output, "not set" never stored.
  - `impact`: dependants of a world, re-key plans (with and without dropping edited cells), bundles affected.
- **React Testing Library:** `CellEditor`, `CellInspector`, `RekeyDialog`, `ImpactList`, `RegionsList` from fixtures; read-only fields; tooltips.
- **Canvas tests (Dia):** the extension props, geometry helpers, zoom and pan hit-testing.
- **GoogleTests (C++):** `WorldContentHash` stability; the catalogue `validated` flag update leaves every other byte of the catalogue unchanged.
- **E2E:** open a baked world, paint a biome with the brush, undo, repaint, save (hash changes, unvalidated badge shows, impact dialog lists a start state), re-key it, save it; with a stub game, Validate in game sends the command and a pass clears the badge.
- **Perf:** scripted brush strokes and a 2,000-cell commit at Large.
- **Visual gate:** against the new mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| T-2 Resolved Decision 3 | Baked worlds and start states are fully editable (user decision 2026-10-19) | Every authored per-cell field is editable; only derived fields are read-only |
| T-2 Resolved Decision 2 | Unvalidated world edits warn; only the default world is blocked | The `validated: false` flag and the composer's refusal (Feature 3) |
| F-3 | Start states only on baked terrain; hash mismatch refuses a start state at load | Dependants are shown as broken and re-keyed deliberately |
| F-3 save rules | The game adds or updates only its own records | T-2 sets `validated` on any record, which is the flag's purpose, and adds nothing else |
| SD-S1-009 | Imports open locked; provenance records the generator | Provenance is left untouched |
| Mockup-first rule | A visual mockup before any editor feature is built | Mockup gate above |
| UI/business separation | A typed model first | `model/` holds all editing logic |
| Editor actions via DiaAPI | Every action is a command | Cell edits go through Feature 1's operations; save, re-key and validate are editor actions |
| AD-001 | Ask before Dia edits | The `CellGridCanvas` extensions in `Dia/DiaEditorUI` need sign-off at build time |
| CoW AD-002 | Four-tier placement | Editor code in `Cluiche/CoW/Editor/`; the generic canvas stays in Dia |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 to 4 | Shell, checks, used-by, save flow, validate in game | Approved |
| `CellGridCanvas` and its extensions | The canvas | Approved; extensions added to its spec with this feature |
| S-1 JSON import | Recompute `regions[]` summaries on import (amended) | Approved; amended |
| S-1 world-field schema sidecar (`for: cow.world`) | Layers, ramps, enums | To be authored with S-1 Feature 4 |
| A1 per-cell field schema (`for: cow.ecology.cell`) | Start-state overlay fields | To be authored with A1 Feature 1 and 2 |
| F-3 `WorldContentHash`, catalogue writer | Hash and flag updates | Approved |
| DiaValueSchema Features 1, 3, 5 | Entries, tooltips, ramps | Approved, not built |

## Resolved Decisions

1. **S-1 recomputes region summaries on import** (2026-10-20). One implementation of the region algorithm; the cells are the truth, and the saved summaries are informational.
2. **Derived fields are read-only** (2026-10-20): `latitude`, `terrain_flags`, `downstream`. "Fully editable" covers everything authored; hand-editing derived data would only produce worlds the guarantees reject.
3. **A gesture is one batch** (2026-10-20), built from a preview buffer, so undo is meaningful and the history stays small.
4. **Re-keying is a choice, with an option to drop overlay cells on edited cells** (2026-10-20), because re-keying alone could let an old overlay override a deliberate world change.
5. **Sites are out of scope** (2026-10-20); they belong to S-1's panel and the dragon layer.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. A cell-editor mockup must be created and approved before it is built (Mockup gate); the `CellGridCanvas` extensions need AD-001 sign-off.
