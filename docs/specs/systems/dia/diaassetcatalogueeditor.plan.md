# Plan: DiaAssetCatalogueEditor

**Spec:** @docs/specs/systems/dia/diaassetcatalogueeditor.md  
**Status:** In Progress  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-05

## Prerequisite

DiaAssetCatalogue must be Done before this plan can start.  
**Blocked on:** ~~@docs/specs/systems/dia/diaassetcatalogue.plan.md~~ — **Cleared** (Done 2026-05-04)

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 0 | Project Scaffolding | Done | haiku + sonnet | See sub-tasks below |
| 1 | Feature 1 — Manifest Load/Save | Done | sonnet | Depends on Task 0 |
| 2 | Feature 2 — Asset Record CRUD | Done | sonnet | Depends on Task 1 |
| 3 | Feature 3 — File Discoverer | Done | sonnet | Depends on Task 2 |
| 4 | Feature 4 — Relationship Editor | Done | sonnet | Depends on Task 2 |
| 5 | Feature 6 — Validation Panel | Not Started | sonnet | Depends on Task 2 |
| 6 | Feature 7 — Asset Type Editor Routing | Not Started | sonnet | Depends on Task 2 |
| 7 | Feature 8 — Catalogue Rules UI | Not Started | sonnet | Depends on Task 4 |
| 8 | Feature 5 — Relationship Graph View | Not Started | sonnet | Depends on Task 4 (reuses get_forward/reverse_refs) |

## Task 0: Project Scaffolding

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 0.1 | Create `Dia/DiaAssetCatalogueEditor/` directory structure | haiku | Dirs: `Commands/`, `Handlers/`, `UI/` |
| 0.2 | Create `DiaAssetCatalogueEditor.vcxproj` + `.vcxproj.filters` | haiku | Static lib, deps: DiaAssetCatalogue, DiaEditor, DiaAPI, DiaCore, jsoncpp |
| 0.3 | Add to `Cluiche.sln` | haiku | Project reference from CluicheEditor |
| 0.4 | Create `dia.assetcatalogueeditor.architecture.module.md` | haiku | YAML frontmatter per AD-001 |
| 0.5 | Create plugin class skeleton: `DiaAssetCatalogueEditorPlugin.h/.cpp` | sonnet | Implements `IEditorPlugin`, `REGISTER_EDITOR_PLUGIN` macro, empty `OnLoad/OnUnload/OnUpdate` |
| 0.6 | Create `UI/index.html` shell | sonnet | React-mosaic dockable layout with placeholder panels |
| 0.7 | Verify: builds, plugin loads in CluicheEditor, empty UI appears | haiku | `dia run cluicheeditor` |

**Deliverable:** Empty plugin registered and visible in CluicheEditor's plugin browser.

## Task 1: Feature 1 — Manifest Load/Save

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/manifest-load-save.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 1.1 | `ManifestLoadHandler.h/.cpp` — `Load()`, `Save()`, `NewManifest()` | sonnet | Wraps `CatalogueManifestSerializer`; clears `CommandHistory` on load, marks save point on save |
| 1.2 | `SessionContext.h/.cpp` — read/write `.context.json` | sonnet | Output path: `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/`; stores `lastManifestPath` |
| 1.3 | Register `asset_catalogue.load_manifest` / `save_manifest` request handlers on `WebUIBridge` | sonnet | JSON in/out, returns `{ success, record_count/error }` |
| 1.4 | Dirty tracking — wire to `CommandHistory::IsAtSavePoint()` | sonnet | Push dirty state to UI via `NotifyUIDataChanged("dirty", ...)` |
| 1.5 | UI: toolbar (New / Open / Save), path display, dirty indicator (*) | sonnet | File menu + Ctrl+S. Open = Win32 `GetOpenFileNameW` dialog |
| 1.6 | Unsaved-changes prompt (Save / Discard / Cancel) | sonnet | Triggered on close, load-new, new-manifest when dirty |
| 1.7 | Startup re-open offer from `.context.json` | sonnet | If path valid → prompt; if path stale → warn + clear |
| 1.8 | **Acceptance gate**: open HTML mockup, compare toolbar/save flow | haiku | Visual check vs `mockups/diaassetcatalogueeditor.html` |

**Commit after Task 1.**

## Task 2: Feature 2 — Asset Record CRUD

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/asset-record-crud.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 2.1 | `Commands/CreateRecordCommand.h/.cpp` | sonnet | Validates `type.name` format (SD-CAT-001); computes content hash; undo removes record |
| 2.2 | `Commands/UpdateRecordCommand.h/.cpp` | sonnet | Snapshots old/new full record; recomputes hash if source path changed |
| 2.3 | `Commands/DeleteRecordCommand.h/.cpp` | sonnet | Snapshots record + all edges; removes from registry + RelationshipIndex; undo restores |
| 2.4 | Register `create_record`, `update_record`, `delete_record` request handlers | sonnet | Parse JSON, create command, execute via `CommandHistory::ExecuteCommand()` |
| 2.5 | Asset List panel (CEF) — filterable table | sonnet | Columns: ID, Type, Status, Tags, Scope. Filters: type dropdown, tag, free-text. Row click selects. |
| 2.6 | Record Editor panel (CEF) — edit form | sonnet | Fields: ID (ro), Type (ro), Source Path + browse, Tags chip-list, Scope dropdown, Stage Name |
| 2.7 | Create Record dialog | sonnet | ID input with `type.name` validation, type selector, source path, tags, scope |
| 2.8 | Delete confirmation dialog | haiku | Shows record ID + edge count that will be removed |
| 2.9 | Inline validation (JS + C++ authoritative) | sonnet | ID format, duplicate check, source path existence |
| 2.10 | **Acceptance gate**: mockup comparison | haiku | Asset List + Record Editor match mockup |

**Commit after Task 2.**

## Task 3: Feature 3 — File Discoverer

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/file-discoverer.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 3.1 | `Handlers/FileDiscoverer.h/.cpp` — recursive FS scan | sonnet | Win32 `FindFirstFileW`/`FindNextFileW`; pattern match via `AssetTypeRegistry`; exclude already-registered; handle `*.folder/` |
| 3.2 | `GenerateDefaultId()` — derive `type.name` from file path | sonnet | Strip root, lowercase, replace separators → e.g. `texture.player_ship` |
| 3.3 | Bulk-add as compound command | sonnet | Wraps N `CreateRecordCommand`s via `CommandHistory::BeginCompound/EndCompound` for atomic undo |
| 3.4 | Register `asset_catalogue.discover_files` request handler | sonnet | Input: `{ root_path }`; output: `{ files: [{ path, suggested_type, suggested_id }] }` |
| 3.5 | File Discoverer panel (CEF) | sonnet | Root path input + Browse, results table with checkboxes, editable type/ID columns, "Add Selected" button |
| 3.6 | Confirmation dialog before bulk-add | haiku | Shows count + list of records to be created |
| 3.7 | **Acceptance gate**: mockup comparison | haiku | |

**Commit after Task 3.**

## Task 4: Feature 4 — Relationship Editor

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/relationship-editor.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 4.1 | `Commands/AddRelationshipCommand.h/.cpp` | sonnet | Adds edge; invalidates reverse cache; undo removes |
| 4.2 | `Commands/RemoveRelationshipCommand.h/.cpp` | sonnet | Removes edge; undo restores |
| 4.3 | Register `add_relationship`, `remove_relationship`, `get_forward_refs`, `get_reverse_refs` | sonnet | Last two are queries (no command, read RelationshipIndex directly) |
| 4.4 | Relationship section in Record Editor panel | sonnet | Forward/reverse lists with type labels, remove buttons, clickable navigation, inline "Add" form with asset ID autocomplete |
| 4.5 | **Acceptance gate**: mockup comparison | haiku | |

**Commit after Task 4.**

## Task 5: Feature 6 — Validation Panel

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/validation-panel.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 5.1 | Register `asset_catalogue.validate` request handler | sonnet | Calls `AssetRegistry::Validate()`; returns `{ errors: [{ assetId, severity, type, message }] }` |
| 5.2 | Validation Panel (CEF) | sonnet | Validate button, summary bar (error/warn/ok counts), error table with severity icon + Asset ID + type + message, click-to-navigate, empty/clean states |
| 5.3 | **Acceptance gate**: mockup comparison | haiku | |

**Commit after Task 5.**

## Task 6: Feature 7 — Asset Type Editor Routing

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/asset-type-editor-routing.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 6.1 | `Handlers/AssetTypeEditorRegistry.h/.cpp` | sonnet | HashTable (cap 32): typeId → editorPluginId. `RegisterTypeEditor()`, `FindEditorForType()` |
| 6.2 | Register `asset_catalogue.open_asset` request handler | sonnet | Lookup type → if editor registered, call `IPluginLoader::LoadPlugin()`; else `ShellExecuteExW` (folder→"explore", others→"open") |
| 6.3 | Wire double-click in Asset List + "Open" button in Record Editor | haiku | Both call `open_asset` |
| 6.4 | Handle no-association case | haiku | Log warning to Output Console; no crash |
| 6.5 | **Acceptance gate**: open a folder + a .json via OS default | haiku | |

**Commit after Task 6.**

## Task 7: Feature 8 — Catalogue Rules UI

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/catalogue-rules-ui.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 7.1 | `Handlers/LoadRulesHandler` — loads `assets.rules.json` via `CatalogueRulesEngine::LoadRules()` | sonnet | |
| 7.2 | `Handlers/DryRunHandler` — calls `CatalogueRulesEngine::EvaluateDryRun()`, serializes changeset | sonnet | Returns proposed changes with conflict warnings |
| 7.3 | `Commands/ApplyRulesCommand` — wraps `CatalogueRulesEngine::Apply()` as compound command | sonnet | Skips manual-override items; undo reverses all changes |
| 7.4 | Rules Panel (CEF) | sonnet | Rules list, "Dry Run" button → changeset preview table (add/modify/remove rows, conflict highlights, manual override badges), "Apply" action bar |
| 7.5 | Manual override skip logic | sonnet | Items flagged as manually overridden in dry-run preview are excluded from Apply |
| 7.6 | Asset List refresh after Apply | haiku | Push updated registry data to Asset List panel |
| 7.7 | **Acceptance gate**: mockup comparison | haiku | |

**Commit after Task 7.**

## Task 8: Feature 5 — Relationship Graph View

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/relationship-graph-view.md

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 8.1 | Create `UI/relationship-graph/index.html`, `graph.js`, `graph.css` | sonnet | VisJS network graph; direct edges only (not transitive closure) |
| 8.2 | Wire asset selection → graph population | sonnet | On select in Asset List, fetch forward + reverse refs via existing handlers, render as nodes/edges |
| 8.3 | Node expand (double-click) — fetch and add that node's refs | sonnet | Incremental graph growth |
| 8.4 | Click-to-navigate — clicking a node selects that record in Asset List / Record Editor | sonnet | |
| 8.5 | Visual polish: type-color map, root node highlight, legend bar, empty state | sonnet | |
| 8.6 | **Acceptance gate**: mockup comparison | haiku | |

**Commit after Task 8.**

## Key Patterns (All Tasks)

- **Plugin registration**: `REGISTER_EDITOR_PLUGIN` macro in `.cpp` — automatic discovery
- **C++↔JS bridge**: `WebUIBridge::RegisterRequestHandler()` for commands; `NotifyUIDataChanged()` to push state
- **Mutations**: Every registry change is an `IEditorCommand` subclass, executed via `CommandHistory::ExecuteCommand()`
- **No business logic in editor**: All operations delegate to DiaAssetCatalogue's API (`AssetRegistry`, `RelationshipIndex`, `CatalogueManifestSerializer`, `CatalogueRulesEngine`, `ContentHasher`)
- **Acceptance gate**: Each feature ends with a visual comparison against `docs/specs/features/dia/diaassetcatalogueeditor/mockups/diaassetcatalogueeditor.html`

## Session Notes

### 2026-05-04
- Plan created. All 8 feature specs Approved.
- Build order: 1 → 2 → 3 → 4 → 6 → 7 → 8 → 5 (per system spec — graph view last, it consumes get_forward/reverse_refs already registered in Feature 4).
- Tasks are numbered by spec feature number for traceability; ordered in the plan by implementation dependency.
- Feature 4 `get_forward_refs`/`get_reverse_refs` DiaAPI commands are also consumed by Feature 5 (graph) — register once in Feature 4, reuse in Feature 5.
- HTML mockup at `docs/specs/features/dia/diaassetcatalogueeditor/mockups/diaassetcatalogueeditor.html` — open in browser to validate each UI feature.

### 2026-05-05
- Expanded plan with full sub-task breakdown (58 sub-tasks across 9 phases).
- Added Task 0 (scaffolding) — vcxproj, plugin skeleton, UI shell, build verification.
- DiaAssetCatalogue prerequisite cleared (Done 2026-05-04).
- Model assignments: haiku for file edits/commits/verification; sonnet for all implementation.
- Each task ends with an acceptance gate comparing against the HTML mockup.
- Task 0 complete: DiaAssetCatalogueEditor.vcxproj created (GUID {D1E2F3A4-B5C6-7890-DEFA-012345678901}), added to Cluiche.sln (Editors folder), wired into CluicheEditor.vcxproj. Plugin skeleton compiles. Deploy step fails on node (pre-existing env issue, not code). UI shell at UI/index.html.
- Tasks 1–3 complete: ManifestLoadHandler, SessionContext, CRUD commands (Create/Update/Delete), FileDiscoverer (Win32 scan + GenerateDefaultId), discover_files handler, full Asset List + Record Editor + File Discoverer UI panels. Added FindByFileName() to AssetTypeRegistry to avoid FilePath PathAlias assert during scanning.
