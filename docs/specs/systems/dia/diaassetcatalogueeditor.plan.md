# Plan: DiaAssetCatalogueEditor

**Spec:** @docs/specs/systems/dia/diaassetcatalogueeditor.md  
**Status:** In Progress  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-06

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
| 5 | Feature 6 — Validation Panel | Done | sonnet | Depends on Task 2 |
| 6 | Feature 7 — Asset Type Editor Routing | Done | sonnet | Depends on Task 2 |
| 7 | Feature 8 — Catalogue Rules UI | Done | sonnet | Depends on Task 4 |
| 8 | Feature 5 — Relationship Graph View | Done | sonnet | Depends on Task 4 (reuses get_forward/reverse_refs) |

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

## Finishing Tasks (Gap Close — 2026-05-06)

Audit revealed 4 acceptance criteria unmet across Features 5 and 8, plus spec/plan housekeeping. Tasks 9–12 close these gaps. Build order: 12 (housekeeping, no code) → 9 → 10 → 11 → 13.

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 9 | Feature 5 — Graph: type-based node coloring | Done | haiku | In `UI/index.html`: derive per-node color from asset type prefix (texture/entity/config/stage/etc). AC2 unmet — currently colors by role only. |
| 10 | Feature 5 — Graph: expand no-op for already-expanded nodes | Done | haiku | Add `graphExpandedNodes` set; guard `fetchAndAddGraphRefs` to skip if node already expanded. AC9 unmet. |
| 11 | Feature 8 — Rules: expose rule enumeration | Done | sonnet | Add `RuleInfo` DTO + `GetRule(i)` to `CatalogueRulesEngine`. Register `asset_catalogue.get_rules` DiaAPI handler. Wire UI to render rules list table on load. AC2 unmet. |
| 12 | Housekeeping — correct spec/plan statuses | Done | haiku | Feature specs 1,2,3,4,6,7 → Done. Features 5,8 → In Progress. System spec note updated. BACKLOG updated. |
| 13 | Feature 8 — Rules: manual override tracking | Done | opus | Add per-field manual-override flags to `AssetRecord`. Set in `UpdateRecordCommand::Execute`. Surface in `RuleChange::mIsManualOverride`. Add badge to UI + filter in `apply_rules` handler. AC9/AC10 closed. |

## Task 9: Feature 5 — Graph: Type-Based Node Coloring

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/relationship-graph-view.md  
**AC addressed:** AC2 — nodes colored by asset type, legend shown.

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 9.1 | Add `TYPE_COLORS` map in `index.html` — type prefix → color (texture=#007acc, entity=#4ec9a0, config=#ce9178, stage=#dcdcaa, default=#9cdcfe) | haiku | Extract prefix from node ID via `id.split('.')[0]` |
| 9.2 | Change `addGraphNode` to store type prefix; update `renderGraph` to use `TYPE_COLORS[n.type]` instead of `GRAPH_COLORS[n.role]` | haiku | Root node retains larger radius (r=18) for visual distinction |
| 9.3 | Update graph legend bar to show type colors instead of role colors | haiku | `#graph-legend` element in HTML |
| 9.4 | **Acceptance gate**: open mockup graph tab, compare legend and node colors | haiku | |

**Commit after Task 9.**

## Task 10: Feature 5 — Graph: Expand No-Op

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/relationship-graph-view.md  
**AC addressed:** AC9 — expanding an already-expanded node is a no-op.

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 10.1 | Add `var graphExpandedNodes = {}` to graph state in `index.html` | haiku | Object used as a set: `graphExpandedNodes[id] = true` |
| 10.2 | In `fetchAndAddGraphRefs(id, isExpand)`, guard with `if (isExpand && graphExpandedNodes[id]) return;` then set `graphExpandedNodes[id] = true` on completion | haiku | Root load (`isExpand=false`) always proceeds; reset on `loadGraphForAsset` |
| 10.3 | Clear `graphExpandedNodes` in `loadGraphForAsset` (fresh graph load) | haiku | |
| 10.4 | **Acceptance gate**: verify double-click on expanded node makes no network call | haiku | |

**Commit after Task 10.**

## Task 11: Feature 8 — Rules Enumeration

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/catalogue-rules-ui.md  
**AC addressed:** AC2 — rules list shows name, match criteria, action type after load.

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 11.1 | Add `RuleInfo` struct to `CatalogueRulesEngine.h` — `mName`, `mMatchType` (as string), `mMatchValue`, `mActionType` (as string), `mActionParam` | sonnet | Public DTO; `GetRule(unsigned int index) const` returns `RuleInfo` |
| 11.2 | Implement `GetRule()` in `CatalogueRulesEngine.cpp` — map internal `MatchType`/`ActionType` enums to display strings | sonnet | |
| 11.3 | Register `asset_catalogue.get_rules` handler in `DiaAssetCatalogueEditorPlugin::RegisterRulesHandlers()` — returns `{ rules: [{ name, match, matchValue, action, actionParam }] }` | sonnet | Call after `load_rules` succeeds |
| 11.4 | In `index.html` `onLoadRules()`, after success: call `get_rules` and render rules list table above the Dry Run preview area | sonnet | Columns: Name, Match, Action |
| 11.5 | **Acceptance gate**: load a rules file, verify rules list populates | haiku | |

**Commit after Task 11.**

## Task 12: Housekeeping

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 12.1 | Feature specs 1,2,3,4,6,7 → Status: `Done` | haiku | Edit each .md file's Status section |
| 12.2 | Feature specs 5,8 → Status: `In Progress` | haiku | |
| 12.3 | System spec (`diaassetcatalogueeditor.md`) → add note that 6/8 features are Done, 2 in progress | haiku | Do not change `Approved` — system spec stays Approved until all features Done |
| 12.4 | Update BACKLOG.md — move DiaAssetCatalogueEditor row to In Progress, note Tasks 9–13 remaining | haiku | |

**Commit after Task 12.**

## Task 13: Feature 8 — Manual Override Tracking

**Spec:** @docs/specs/features/dia/diaassetcatalogueeditor/catalogue-rules-ui.md  
**AC addressed:** AC9 (manual badge in preview), AC10 (manual fields skipped on apply).  
**Note:** Highest complexity task — touches DiaAssetCatalogue core. Confirm scope before starting.

| # | Sub-task | Model | Notes |
|---|----------|-------|-------|
| 13.1 | Add `mManualOverrideFlags` bitmask to `AssetRecord` — one bit per field (scope, tags, source_path, stage) | opus | Define `AssetRecord::ManualField` enum for bit positions |
| 13.2 | Set bits in `UpdateRecordCommand::Execute()` for each field the user explicitly changed | opus | Compare old vs new record; set flag on any changed field |
| 13.3 | Clear bits in `ApplyRulesCommand::Execute()` for fields written by a rule (rules own those fields now) | opus | |
| 13.4 | Populate `RuleChange::mIsManualOverride` in `CatalogueRulesEngine::Evaluate()` — check source record's flag for the target field | opus | Add `mIsManualOverride` bool to `RuleChange` struct |
| 13.5 | Update `apply_rules` handler — filter out entries where `mIsManualOverride == true` unless `data["overwrite_manuals"]` is true | sonnet | |
| 13.6 | UI: add yellow "manual" badge to dry-run preview rows where `mIsManualOverride == true` | haiku | |
| 13.7 | **Acceptance gate**: manually set a field, run dry run, verify badge appears; run apply, verify field is skipped | haiku | |

**Commit after Task 13.**

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

### 2026-05-06
- Gap audit: plan was incorrectly marked Done. Tasks 4–8 (relationship editor, validation, routing, rules UI, graph) are implemented in C++ and UI, but 4 acceptance criteria remain unmet across Features 5 and 8.
- Feature 5 gaps: AC2 (node color by type, not role) and AC9 (expand no-op for already-expanded nodes). Both are JS-only fixes in `UI/index.html`.
- Feature 8 gap: AC2 (rules list table after load) requires `GetRule(i)` on `CatalogueRulesEngine` — not currently public. AC9/AC10 (manual override badge + skip-on-apply) requires per-field bitmask on `AssetRecord` — high complexity, cross-cutting change.
- Added Tasks 9–13 to close gaps. Build order: 12 → 9 → 10 → 11 → 13.
- Task 13 (manual override) is the most invasive; confirm scope with user before starting.
- Status corrected to In Progress.
- Task 13 completed: `ManualOverrideField` enum + `mManualOverrideFlags` on `AssetRecord`; `UpdateRecordCommand` sets bits; `CatalogueRulesEngine::Evaluate()` populates `RuleChange::mIsManualOverride`; `ApplyRulesCommand` skips manual-flagged fields (with `mOverwriteManuals` opt-in); UI shows yellow "manual" badge in dry-run.
