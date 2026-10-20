# Feature Spec: T-2 Plugin Shell, Files and Schema

**Parent:** @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md
**Mockup:** @docs/specs/applications/cow/systems/t2-worlddataeditor/mockups/world-data-editor.html
**Built on:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md, @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md, @docs/specs/applications/cluicheeditor/systems/pluginbrowser/game-scoped-plugins.md

**Gameplay Domains:** debug, tools

## Summary

The foundation of T-2. It delivers the `CoWWorldDataEditorPlugin` (derived from `LiveConnectionPluginBase`, so it can reach a running game in Feature 4) that exists only while `cow.diagame` is loaded; the `open_asset` routes for every F-3 format id; a C++ **FileService** (list, read, atomic write, change detection, catalogue upsert) and **SchemaService** (loads `*.schema.json` and the ramps for the UI); and the React shell: asset tree, document tabs, Properties panel, a generic read-only JSON view, and undo/redo built on one small set of edit operations that every later editor view will use.

No table, form, pack, bundle, scenario or cell editing yet: Features 2, 3, 5 and 6 add those views, and Feature 4 adds validation and the game link.

## Problem

Every later T-2 view needs the same things: a plugin that exists only for CoW, files that are read and written in exactly the formats F-3 uses, a schema loaded the same way the runtime loads it, and a document model with undo that all views edit through. Building those once, against fixtures, lets each view be developed and tested on its own.

## Goals

1. **Game-scoped.** The plugin and its routes exist only while `cow.diagame` is loaded.
2. **Open from anywhere.** `open_asset` on any F-3 file type opens it in T-2; the asset tree lists every world-sim file.
3. **Same bytes as F-3.** Reading and writing use F-3's envelope, stable formatting and catalogue rules, from the same code.
4. **Same schema as the runtime.** The schema is loaded by DiaValueSchema's loader, not re-implemented.
5. **One edit model.** Every change is an operation with an inverse, so undo and redo are the same for all views.
6. **Honest state.** Dirty documents, files changed on disk and files that fail to parse are shown clearly and never lost.

## Non-Goals

- Table and form editors (Feature 2), packs and bundles (Feature 3), validation and the game link (Feature 4), scenarios (Feature 5), cell editing (Feature 6).
- Authoring schemas (hand-written by developers).
- Merging concurrent edits (v1 is reload or keep mine).
- Running or regenerating the world.

## Architecture

`Cluiche/CoW/Editor/WorldDataEditor/` in the `CoWEditor` static library (next to T-1; whichever feature is built first creates the library). Listed in `cow_editor.diaapp` and named from `cow.diagame` `editor.manifests`.

```
CoWWorldDataEditorPlugin : LiveConnectionPluginBase          (C++)   // connection is optional; everything here works offline
  ├─ OnLoad     register open_asset routes (AssetTypeEditorRegistry) for every F-3 format id; register editor actions
  ├─ OnUnload   remove the routes and actions
  ├─ FileService      list / read / write / poll / catalogue upsert
  ├─ SchemaService    schema + ramps for the UI
  └─ UI bridge        typed requests and one change notification

UI (React 18 + zustand + @dia/editor-ui, Vite)     .../WorldDataEditor/UI/
  model/   documents, DocOp + history, asset tree model, change detection state (pure TS, unit-tested)
  store/   open documents, active document, tree filter, selection
  views/   AssetTree · DocumentTabs · RawJsonView · Properties · ChangedOnDiskBanner
```

Views call `model/` and the store only; they never call the bridge directly.

### Shared F-3 code

`FileService` reuses F-3's pure code rather than re-implementing it: the asset-kind table, `AssetEnvelope`, `PackFile`, the stable writers and the catalogue reader/writer. Those files live in a `Core/` folder of `CoW/Shared/WorldAssets/` with no module, bus or runtime dependencies, built as a static library (`CoWWorldAssetsCore`) that both the CoW runtime and `CoWEditor` link. F-3 Features 1 and 3 are amended to say so (the catalogue writer is a pure `CatalogueFile` class that the module calls).

### Routes and actions

- **`open_asset` routes** (registered in `OnLoad`, removed in `OnUnload`): one per format id: `cow.world_bundle`, `cow.weather_pack`, `cow.ecology_pack`, `cow.ecology_start`, `cow.world`, `cow.worldshape`, `cow.species`, `cow.biomes`, `cow.succession_rules`, `cow.weather_params`, `cow.ecology_params`, `cow.terrain_rules`, `cow.scenario` (catalogue type = format id). A route opens the plugin if it is closed and focuses a tab for the file. Outside CoW the routes do not exist, so the OS default app opens the file (Game-Scoped Plugins).
- **Editor actions** (so they can be scripted, with `ValueParam` descriptions): `world_data.open {path}`, `world_data.close {path}`, `world_data.save {path}`, `world_data.save_all`, `world_data.undo`, `world_data.redo`, `world_data.reload {path}`.

### FileService

Rooted at the loaded game's asset folder (`Cluiche/Assets/CoW/`, from the loaded `.diagame`); no alias is needed.

| Operation | Behaviour |
|---|---|
| `ListAssets` | Scans `Global/**` and `Stages/*/scenarios/` for the extensions in the asset-kind table, and reads the catalogue; returns `{ path, kind, formatId, recordId?, inCatalogue, mtime, size }`. Files with an unknown extension are ignored |
| `ReadDocument(path)` | Reads text, parses JSON, runs `CheckEnvelope` for the kind's format; returns `{ text, status, fileVersion, mtime, size }`. A file that cannot be parsed is returned with an error and opened read-only |
| `WriteDocument(path, text)` | Writes through a temporary file and renames; refuses if the file's mtime or size changed since it was read unless told to overwrite |
| `Poll` | Once a second, compares the mtime and size of open documents and of the tree's folders; reports changed, added and removed files in one notification |
| `CatalogueUpsert(record)` | Read-modify-write of `assets.catalogue.json` using the shared catalogue writer, in the repository's exact format; adds a record only when the path has none |

- **Records T-2 creates** have no `author` (they are hand-authored), `tags: ["world"]`, `scope: "global"`, `type` equal to the format id, and a baked-world record carries `validated` (set by Feature 6). Because they have no `author`, a testbed's `save_as` will refuse to overwrite them (F-3's protection of hand-authored records), which is intended.
- Writing never leaves `Assets/CoW/`; paths with `..` or a drive letter are rejected.

### SchemaService

- Loads every `*.schema.json` and `dia.ramps` file under the asset folder with DiaValueSchema's **loader and registry classes used standalone** (no module, no service stream), the same code the runtime uses. A schema that fails to load is reported with the file, entry and reason and does not block the others.
- Serves entries to the UI in the `valueschema.get` shape (Feature 3 of DiaValueSchema), plus `ramps`, so `ValueLabel` and `ValueTooltip` render the same text as the game.
- Re-reads a schema file when it changes on disk (via `Poll`) and tells the UI.
- An asset with no schema entry for a key is still shown (raw name, no tooltip); the missing entry is Feature 4's validation concern.

### Document model (TypeScript)

```ts
interface Doc { path; kind; formatId; text; value: Json; envelope; mtime; size;
                readOnly: boolean; error?: string; history: History; savePoint: number }

type Op =
  | { kind: 'set',    path: Path, value: Json }
  | { kind: 'insert', path: Path, index: number, value: Json }     // into an array
  | { kind: 'remove', path: Path, index: number }
  | { kind: 'move',   path: Path, from: number, to: number }
  | { kind: 'batch',  label: string, ops: Op[] }
```

- `apply(doc, op)` returns the new value and the **inverse** op; `History` is a stack with a position, a maximum depth of 200, a label per entry and a cleared redo tail after a new edit.
- **Dirty** = the history position differs from the save point. Undoing to the save point makes the document clean. A reload or a successful save resets the history or the save point respectively.
- Views produce only these operations (Features 2, 3, 5 and 6 add no new ones), so undo, redo, dirty state and the history list work the same everywhere. Paths address nested objects by key and arrays by index.
- Ctrl+Z and Ctrl+Y act on the active document; a compound edit (`batch`) is one step.

### UI shell

- **Asset tree** (left): groups WorldGen, Weather, Ecology, Succession, Worlds, Scenarios; items show a type icon, the display name or file name, a dirty dot, and badges. This feature supplies the tree, the dirty dot and a "changed on disk" badge; the `stale`, `invalid` and `unvalidated` badges are filled by Features 3, 4 and 6. Filter by text, type and status.
- **Document tabs** (centre): any number of open documents, with a dirty dot and a close button. Closing a dirty tab asks Save / Discard / Cancel.
- **Raw JSON view:** a read-only, formatted JSON view with path breadcrumbs. It is the fallback for any document until its dedicated editor exists, and for files that fail to parse (shown with the parse error).
- **Properties** (right): file path, format id and version, catalogue record fields, the stale/validated state when known, and the schema entry for the selected path (rendered with `ValueTooltip`).
- **Changed on disk:** if an open, unmodified document changes, it reloads and a toast says so; if it is dirty, a banner offers **Reload** or **Keep mine** (the system's Resolved Decision 1). A removed file stays open and is marked deleted.

### Basic save

`Save` serialises the document with the stable writer for its kind, calls `WriteDocument`, upserts the catalogue record if absent, and moves the save point. Validation, version bumps and the game reload are added to this flow by Feature 4, which wraps it; Feature 1 defines the steps as a list so Feature 4 inserts its own.

### Dirty state and game switching

The plugin reports whether any document is dirty to the host, so Game-Scoped Plugins' Save / Discard / Cancel prompt on a game switch (AC8 of that feature) covers T-2 documents. Save writes them; Discard closes them without writing.

### Bridge requests

`listAssets`, `readDocument`, `writeDocument`, `schemaEntries`, `reportDirty`, and one notification `filesChanged` from `Poll`. Each is typed on both sides.

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** open, save and close (path, kind), schema load summary and each schema failure, files changed on disk, a document that failed to parse, route and action registration.
- **Profiling:** UI timings in dev builds (`performance.mark`); FileService scan time logged at Debug.
- **Health:** a document that fails to parse is shown read-only with its error.

## Acceptance Criteria

1. With `cow.diagame` loaded the plugin can be created and its routes and actions are registered; with another game, or none, it cannot be created and no CoW route exists (a saved layout naming it opens without it).
2. `open_asset` on a file of each of the 13 format ids opens it in T-2 (focusing an existing tab for the same file); outside CoW the same call falls back to the OS default.
3. `ListAssets` returns every file with a table extension under `Global/**` and `Stages/*/scenarios/`, marks whether it has a catalogue record, and ignores other files.
4. `ReadDocument` returns the parsed value and the envelope status; a file that fails to parse opens read-only with the error and is never overwritten by a save.
5. `WriteDocument` is atomic, refuses to replace a file changed since it was read unless overwrite is chosen, and cannot write outside `Assets/CoW/`.
6. `CatalogueUpsert` adds a record in the repository's exact format and leaves an existing record and every other byte of the file unchanged (shared catalogue writer; golden test on the real file); records T-2 creates carry no `author`.
7. `SchemaService` loads the real sidecars and ramps with DiaValueSchema's loader, reports each failing file without blocking the rest, and serves entries in the `valueschema.get` shape.
8. Every edit made through `apply` returns an inverse; undo then redo restores the document exactly; a new edit clears the redo tail; the depth is capped at 200; a `batch` is one undo step.
9. A document is dirty exactly when its history position differs from its save point; undoing to the save point makes it clean; saving moves the save point; a reload clears the history.
10. The asset tree groups, filters and shows the dirty dot and the changed-on-disk badge; the stale, invalid and unvalidated badge slots exist and are empty until Features 3, 4 and 6.
11. Document tabs open, switch and close; closing a dirty tab asks Save / Discard / Cancel.
12. `Poll` reports changed, added and removed files once a second; a clean open document reloads with a toast; a dirty one shows the Reload / Keep mine banner and nothing is merged; a deleted file stays open marked deleted.
13. Properties shows the file and catalogue fields and the schema entry (label, unit, range, description) for the selected path, with `ValueTooltip`.
14. The plugin reports dirty state to the host, so a game switch with unsaved documents prompts Save / Discard / Cancel and Cancel keeps the game.
15. The editor actions exist with `ValueParam` descriptions and behave like the matching UI actions.
16. `CoWEditor` links the F-3 `Core` library and the DiaValueSchema loader, and `CoWWorldAssetsCore` has no module, bus or runtime dependencies (`dia check deps` clean).

## Test Strategy

- **Vitest (model/):** `apply` and inverse for each op kind over fixtures, undo/redo round trips (a randomised sequence with fixed seed), depth cap, dirty and save-point rules, tree grouping and filtering, change-detection state transitions.
- **React Testing Library:** shell views from fixtures, dirty dots, tab close prompt, changed-on-disk banner, Properties with a fixture schema.
- **GoogleTests (C++):** `FileService` against a temporary asset folder (list, read, parse failure, atomic write, stale-write refusal, path escape, poll results), catalogue upsert golden on a copy of the real catalogue, `SchemaService` over fixture sidecars (good, bad, mixed), route and action registration and removal on load and unload, dirty reporting.
- **E2E:** with a stub host: load `cow.diagame`, T-2 appears; `open_asset` opens a file; edit, undo, redo, save; load `cluichetest.diagame`, T-2 closes and the routes are gone.
- **Visual gate:** the shell regions against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-001 | Ask before Dia / CluicheEditor / shared build edits | The `CluicheEditor.vcxproj` reference and the DiaValueSchema loader link need sign-off at build time |
| CoW AD-002 | Four-tier placement | CoW editor code in `Cluiche/CoW/Editor/`; the pure F-3 core stays in CoW-Shared |
| AED-002, AED-005 | `.diaapp` editor section; React and DiaUICEF stack | `cow_editor.diaapp`; React 18 + zustand + `@dia/editor-ui` |
| Game-Scoped Plugins | Fully unavailable out of scope; routes removed on unload | AC 1 and 2 |
| F-3 | Envelope, layout, catalogue rules, hand-authored records protected | Shared pure code; no `author` on T-2 records |
| VS-004 | One validator | Feature 1 adds no validation rules of its own |
| Editor actions via DiaAPI | Every action is a command | The editor actions above |
| UI/business separation | Typed model first; views depend only on it | `model/` and the store |
| Tool tiers | Editor tier: offline files | Feature 1 uses no game connection; the plugin derives from `LiveConnectionPluginBase` only because the DiaEditor rule requires it of any plugin that will connect (Feature 4) |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Game-Scoped Editor Plugins | Scope, routes, dirty prompt | Approved, not built |
| F-3 Features 1 and 3 | The pure core: kind table, envelope, pack file, catalogue writer | Approved; amended for the `Core` library |
| DiaValueSchema Features 1, 3, 5 | Loader, entry shape, ramps | Approved, not built |
| DiaAssetCatalogueEditor `AssetTypeEditorRegistry`, `open_asset` | Routing | Done |
| DiaEditor `LiveConnectionPluginBase` (an `IEditorPlugin`), editor actions | Plugin and actions | Done |
| `@dia/editor-ui` `ValueLabel`, `ValueTooltip` | Properties | Approved, not built |

## Resolved Decisions

1. **Undo lives in the TypeScript model** (2026-10-20). One operation set with inverses serves every view; the host sees only dirty state and the title.
2. **Derives from `LiveConnectionPluginBase`** (2026-10-20, corrected from an earlier "plain `IEditorPlugin`"). DiaEditor requires every plugin that needs a game connection to derive from it, with no ad-hoc connection code. T-2 is offline-first, so the connection stays optional and nothing in this feature depends on it; Feature 4 uses the base class for the game link.
3. **One plugin, many documents** (2026-10-20). Tabs inside one plugin match the mockup and keep one tree, one schema service and one dirty report.
4. **Polling, not OS watchers** (2026-10-20). A one-second mtime and size check is simple, portable and enough for a handful of open files and one tree scan.
5. **Pure F-3 code is shared, not duplicated** (2026-10-20), so the editor and the game write the same bytes.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off (`CluicheEditor.vcxproj` reference, DiaValueSchema loader link) and Game-Scoped Editor Plugins.
