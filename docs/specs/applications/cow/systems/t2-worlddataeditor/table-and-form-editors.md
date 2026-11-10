# Feature Spec: T-2 Table and Form Editors

**Parent:** @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md
**Builds on:** @docs/specs/applications/cow/systems/t2-worlddataeditor/plugin-shell-files-and-schema.md (Feature 1)
**Mockup:** @docs/specs/applications/cow/systems/t2-worlddataeditor/mockups/world-data-editor.html

**Gameplay Domains:** debug, tools

## Summary

The two editors that cover most of CoW's data. The **table editor** edits files that are lists of records (species, biomes, succession rules) with typed cells, live checks, "did you mean" fixes, bulk paste and safe row operations. The **form editor** edits defaults files (terrain rules, weather and ecology parameters) as grouped fields with sliders, defaults and reset. Both are generated from the file's DiaValueSchema entries, both edit only through Feature 1's operations, and both share a **refs index** that powers live reference checks, "used by", safe delete and rename-with-references.

## Problem

Species, biome and rule tables will outgrow hand-editing, and non-programmers will author them. A typo in a biome id silently breaks a species' affinity. A value outside its range is accepted by the JSON and rejected at load. Renaming one id means hunting through several files. Defaults files hold dozens of parameters whose meaning and range are invisible in raw JSON. The editor has to make these mistakes visible as they are typed, explain every value, and make the safe operation the easy one.

## Goals

1. **Typed editing from the schema.** Columns and fields, their editors, units, ranges, enums and tooltips all come from the schema; nothing is hard-coded per file.
2. **Mistakes show up immediately**, with a suggested fix that applies in one click.
3. **Bulk work is possible.** Sort, filter, paste a block of cells, fill down, duplicate and reorder rows.
4. **Ids are safe to change.** Deleting or renaming a referenced id shows every referrer; a rename can update them all.
5. **Defaults files are understandable.** Grouped fields with the default shown, a reset per field, and a marker on every changed value.
6. **One truth for validity.** The live checks give the same results as `dia validate schema` on the same data (shared conformance fixtures).

## Non-Goals

- Packs, bundles and compare (Feature 3), validation panel and the game link (Feature 4), scenarios (Feature 5), cell editing (Feature 6).
- Authoring schemas, or adding a column the schema does not describe (raw keys can be edited in the JSON view or added as unknown keys; see Form editor).
- Atomic undo across several documents.
- Formula cells, charts, or spreadsheet imports beyond tab-separated paste.

## Schema additions this feature needs

Species and biome files use maps keyed by ids (a species' biome `affinity`). DiaValueSchema Feature 1 has no map type, so it gains one, with a matching Feature 2 rule (both amended with this spec):

| Addition | Form | Meaning |
|---|---|---|
| `type: "map"` | `{ "key": "species[].affinity", "type": "map", "keyRef": "cow.biomes/biomes[].id", "valueType": "float", "min": 0, "max": 1 }` | An object whose keys must be ids from another format's record array and whose values have the given type, range and unit |

`dia validate schema` fails a map key that matches no id (with a closest-match suggestion) and a value outside its range, like any other value.

## Architecture

Views in `Cluiche/CoW/Editor/WorldDataEditor/UI/views/`, logic in `model/`. No C++ changes: both editors use Feature 1's bridge requests.

```
model/
  columns.ts       schema entries → ColumnModel (table) and FieldTree (form)
  checks.ts        live checks: type, range, enum, ref, unique, required, map keys → Finding[]
  suggest.ts       closest-match suggestion, same algorithm as the CLI
  refIndex.ts      ids by (format, record path), referrers, used-by
  tableOps.ts      add, duplicate, delete, move, paste, fill-down → Op / batch
  rename.ts        rename plan across documents
views/
  TableEditor · FormEditor · CellEditors (number, bool, enum, string, ref, ref[], map) · SubForm · RenameDialog · DeleteDialog
```

### Which editor opens

By format id: `cow.species`, `cow.biomes` and `cow.succession_rules` open the **table editor** (the file is an array of records at the path the schema's record paths name, for example `species[]`); `cow.weather_params`, `cow.ecology_params` and `cow.terrain_rules` open the **form editor**. Other formats keep Feature 1's raw JSON view until their editors exist. Any document can still be switched to the raw view.

### Column model

Built from the schema entries of the file's format whose keys start with the record path (`species[].…`):
- The column marked `unique` and `required` is the **id column**: first, pinned, and used for row identity in messages.
- Other columns follow `ui.order`, then schema order; `ui.hidden` entries are not shown by default (a "columns" menu can show them); `ui.group` makes a header band.
- Each column carries the entry's `label`, `unit`, `min`/`max`, enum values, `ref`/`keyRef`, `required` and `default`.

### Cell editors

| Schema type | Editor |
|---|---|
| `float`, `int` | numeric input with the unit as a suffix, step arrows from `ui.step`, range hint |
| `bool` | checkbox |
| `enum` | dropdown showing label and description |
| `string` | text input |
| `ref` | searchable picker over the refs index, showing id and display name |
| `ref[]` | chips with add and remove, each chip a ref picker |
| `map`, nested object or array | a summary in the cell (for example "5 biomes"); the **Properties panel** shows the selected row as a sub-form |

Out-of-range, mistyped or dangling values are **kept as typed and flagged**, never silently clamped; saving with flags is allowed after confirmation (Feature 4's save flow).

### Table behaviour

- **Rows.** Virtualised, so 500 rows by 25 columns scroll smoothly. Row operations are `Op`s from Feature 1:
  - **Add:** a new row filled from each column's schema `default` (or empty), with a unique suggested id (`New1`, `New2`, …) and the id cell in edit mode.
  - **Duplicate:** copies the selected rows, giving each a unique id (`Deer` becomes `Deer2`).
  - **Delete:** removes the selected rows; if anything refers to them, the delete dialog lists the referrers and offers Delete anyway or Cancel.
  - **Move up / down:** reorders rows (a `move` op); order in the file is kept exactly.
- **View-only controls.** Sorting, filtering, column search and text search change only what is shown, never the file order; **Restore file order** clears them. Row and column selection is by click, shift-click and keyboard.
- **Keyboard.** Arrow keys and Tab move between cells; Enter or F2 edits; Esc cancels an edit; Ctrl+C and Ctrl+V copy and paste cell blocks; Delete clears the cell to its default or empty.
- **Paste and fill.** Pasting tab-separated text into a selected block writes each value into the matching cell, coerced to the column type; a value that cannot be coerced is written as typed and flagged. A paste or **fill down** (Ctrl+D) is one `batch`, so one undo reverts it.
- **Header.** Column headers carry a `ValueTooltip` (description, unit, range, default, related) from the schema, and an error count when the column has findings.

### Live checks

`checks.ts` runs on every operation, incrementally for the changed rows and fully on open:

| Rule | Finding |
|---|---|
| Type | value is not a number, bool, string or the right kind |
| Range | number outside `min`/`max` |
| Enum | value not one of the enum ids |
| Ref and `ref[]` | no matching id in the refs index |
| Map | a key not in the referenced ids, or a value out of range |
| Unique | duplicate value in a `unique` column |
| Required | a `required` column missing or empty in a record |

A finding is `{ severity, rule, path, message, suggestion? }`. The cell shows a red underline and a tooltip with the message and the suggestion; clicking the suggestion applies it as an operation. A footer shows the document's error and warning counts.

**One rule set.** These are the data-file rules of `dia validate schema` implemented a second time in TypeScript for speed, so they are held to it by shared conformance fixtures: `Dia/DiaValueSchema/Testing/Conformance/data/` holds good and bad data files with the expected findings. The Python validator and this module both run the whole set (DiaValueSchema Feature 2 amended). The suggestion algorithm matches Python's `difflib` ratio (at least 0.6, at most one suggestion), verified on the same fixtures.

### Refs index

`refIndex.ts` holds, for each reference target (`<format>/<record path>`), the ids that exist and, for each id, its referrers.
- **Sources:** an open document's in-memory value is used (so a rename in a dirty document updates reference checks live); other referenced documents are read through `FileService` and cached; the cache is dropped for a file reported by `Poll` as changed.
- **Cost:** a rebuild over 10 files with 300 records each takes under 50 ms; only affected entries are rebuilt after an edit.
- **API:** `idsFor(target)`, `has(target, id)`, `usedBy(formatId, id)` returning `[ { documentPath, valuePath, kind } ]`, shared with Feature 3's "used by".

### Delete and rename

- **Delete** a referenced row: the dialog lists each referrer (file, record, field) and offers Delete anyway (the referrers then show a dangling-reference finding) or Cancel.
- **Rename** an id that has referrers: after the edit commits, `RenameDialog` shows the old and new ids and the references found (including map keys), and offers **Update N references**. Confirming edits each referring document in memory with a `batch` labelled "Rename reference", opening the document as a tab if needed. Every affected document becomes dirty with its own undo; nothing is saved until the user saves each one, and a toast names the tabs. There is no atomic undo across documents in v1.
- Declining leaves the references as they are; they show as dangling-reference findings.

### Form editor

For defaults files:
- **Fields** come from the schema entries (dotted keys) and are grouped by `ui.group`, in `ui.order` then schema order, with collapsible groups (state remembered per document) and a search box over label and key.
- **Controls:** a slider plus numeric input when `min` and `max` exist (step and decimals from `ui.step` and `ui.decimals`), otherwise a numeric input; checkbox, dropdown, text. Each field has a `ValueLabel` and `ValueTooltip`.
- **Default and reset.** The schema `default` is shown as a tick on the slider and in the tooltip; a field whose value differs shows a "changed" marker and a reset button (writes the schema default).
- **Unknown and missing keys.** A key in the file that the schema does not describe appears in an "Unknown keys" group (editable as raw JSON, flagged as a warning); a schema key missing from the file shows the schema default in grey with **Add to file**.
- **Pack warning hook.** A field asks a `PackOverrideIndex` (provided by Feature 3) which packs override its key and shows a warning icon listing them; until that provider exists the list is empty and nothing is shown.
- Every change is a `set` operation; typing in the input and dragging the slider create one undo step per gesture.

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** ref index rebuild time (Debug), rename plans applied (Info: old id, new id, documents touched), paste results (cells written, cells flagged), delete-anyway confirmations (Info).
- **Profiling:** `performance.mark` around live checks, ref index rebuild and table render in dev builds.

## Acceptance Criteria

1. `cow.species`, `cow.biomes` and `cow.succession_rules` open the table editor and `cow.weather_params`, `cow.ecology_params` and `cow.terrain_rules` the form editor; other formats keep the raw view; any document can switch to raw.
2. Columns, their order, labels, units, ranges, enum values, refs and tooltips are generated from the schema entries; a new schema entry adds a column with no code change.
3. Each cell type edits and displays correctly (numeric with unit and step, bool, enum, string, ref picker, ref[] chips, map summary with a sub-form).
4. Out-of-range, mistyped and dangling values are kept as typed and flagged; nothing is clamped.
5. Add, duplicate, delete and move produce the right operations (a new row from schema defaults with a unique id; duplicates get unique ids; delete lists referrers first); each is one undo step.
6. Sorting, filtering and searching never change the file order; Restore file order clears them.
7. Pasting tab-separated text writes the block with type coercion, flags uncoercible cells, and is one undo step; fill down likewise.
8. Live checks report type, range, enum, ref, map, unique and required findings with a message and at most one suggestion; clicking a suggestion applies it as an operation.
9. The TypeScript checks and the Python `dia validate schema` produce the same findings and suggestions on every file in the shared conformance data fixtures.
10. The refs index uses an open document's in-memory value over the file on disk, refreshes on `Poll` changes, and rebuilds 10 files by 300 records in under 50 ms.
11. Deleting a referenced row shows every referrer and requires a choice; renaming a referenced id offers Update N references, which edits the referring documents in memory as dirty changes with their own undo and saves nothing.
12. The form editor groups fields by `ui.group`, shows the default as a tick, marks changed fields and resets them, and handles unknown and missing keys as described.
13. The pack warning shows when a provider returns overriding packs and is absent when it returns none.
14. Every header and field has a `ValueTooltip` from the schema; a field with no schema entry renders with its raw key and no tooltip.
15. A 500-row by 25-column table scrolls smoothly (rows virtualised); an edit applies in under 5 ms and a full live check of the document runs in under 50 ms.
16. The visual result matches the mockup's table, properties and form regions.

## Test Strategy

- **Vitest (model/):**
  - Column model and field tree from fixture schemas (order, groups, hidden, pinned id).
  - Each live check, with the shared conformance data fixtures run against the TypeScript rules and compared with the expected findings.
  - Suggestion ratio against fixture strings.
  - Table operations: add, duplicate (unique ids), delete, move, paste with coercion and flags, fill down, each as ops with inverses (undo restores exactly).
  - Refs index: build, in-memory precedence, invalidation, `usedBy`; rename plan across several documents including map keys.
- **React Testing Library:** both editors from fixtures: cell editors, keyboard navigation, flags and tooltips, delete and rename dialogs, form groups, default tick, reset, unknown and missing keys, pack warning with a fake provider.
- **Conformance (shared):** the data fixtures under `Dia/DiaValueSchema/Testing/Conformance/data/` are run by pytest (`dia validate schema`) and Vitest (`checks.ts`); a new rule must add a fixture.
- **Perf:** scripted render and edit of a 500 by 25 table; full check timing.
- **E2E:** open `species.json` from the Asset Catalogue, enter a biome id in a species' affinity with a typo, see the error and suggestion, apply it, save; rename a biome id and update references across two documents.
- **Visual gate:** against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-004 | Strict validation; one rule set | Live checks are held to the CLI validator by shared conformance fixtures |
| T-2 system spec | The editor and `dia validate schema` never disagree | AC 9 |
| UI/business separation | A typed model first; views depend only on it | `model/` holds checks, ops, ref index and rename plans |
| Editor actions via DiaAPI | Every action is a command | Editing goes through Feature 1's ops and editor actions; no hidden paths |
| Mockup-first rule | A visual mockup before any editor feature is built | The T-2 mockup exists; the visual gate checks against it |
| CoW AD-002 | Four-tier placement | Editor code in `Cluiche/CoW/Editor/`; the map type and conformance fixtures in Dia |
| AD-001 | Ask before Dia edits | The DiaValueSchema `map` addition and the data conformance fixtures need sign-off at build time |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Shell, ops, files, schema service | Approved |
| DiaValueSchema Features 1, 2, 3 | Entries, validator, tooltips; amended here with `map` and the data fixtures | Approved; amended |
| Feature 3 | `PackOverrideIndex` provider for the pack warning | Draft |
| `@dia/editor-ui` `ValueLabel`, `ValueTooltip` | Labels and tooltips | Approved, not built |
| F-3 | The record shapes and file locations | Approved |
| A1 / A3 Feature 1 | The real species, biome and rule files and their sidecar schemas | Specced, not built; editor development uses fixtures |

## Resolved Decisions

1. **Nested values are edited in the Properties sub-form** (2026-10-20), keeping the table scannable; maps, lists and nested objects use the same cell editors there.
2. **Renames offer to update references across documents, in memory** (2026-10-20). Each affected document has its own undo and nothing is written until it is saved; the cost is no atomic cross-document undo.
3. **Flagged, not clamped** (2026-10-20). Out-of-range values stay as typed so the user sees and fixes the real value; the validator, not the editor, is the gate.
4. **Maps get a schema type** (2026-10-20), because species affinity is a map keyed by biome ids; this is generic and belongs in DiaValueSchema.
5. **Live checks are a second implementation held to the first** (2026-10-20), for speed in the UI, with conformance fixtures making disagreement a failing test.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the DiaValueSchema `map` type and the data conformance fixtures.
