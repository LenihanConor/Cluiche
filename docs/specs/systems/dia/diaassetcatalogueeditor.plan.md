# Plan: DiaAssetCatalogueEditor

**Spec:** @docs/specs/systems/dia/diaassetcatalogueeditor.md  
**Status:** Not Started  
**Started:** —  
**Last Updated:** 2026-05-04

## Prerequisite

DiaAssetCatalogue must be Done before this plan can start.  
**Blocked on:** @docs/specs/systems/dia/diaassetcatalogue.plan.md

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 1 | Feature 1 — Manifest Load/Save | Not Started | sonnet | Create `DiaAssetCatalogueEditor` vcxproj. `DiaAssetCatalogueEditor` plugin class (`OnLoad`, `OnUnload`, `OnUpdate`). `ManifestLoadHandler` (Load/Save/NewManifest). `SessionContext` (.context.json persistence to `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/`). Register `load_manifest`/`save_manifest` DiaAPI commands. Dirty tracking via `CommandHistory::IsAtSavePoint()`. Unsaved-changes prompt. Startup re-open offer. File menu + toolbar save button in `index.html`. Acceptance gate: open HTML mockup in browser. |
| 2 | Feature 2 — Asset Record CRUD | Not Started | sonnet | `CreateRecordCommand`, `UpdateRecordCommand`, `DeleteRecordCommand` (each IEditorCommand with Execute/Undo). Register `create_record`, `update_record`, `delete_record` DiaAPI commands. Asset List panel (filterable table: ID/Type/Status/Tags/Scope). Record Editor panel (form: ID, Type, Source Path, Tags chip-list, Scope dropdown, Stage Name). Create Record dialog. Delete confirmation dialog. Inline validation (ID format, duplicate, source path exists). Content hash via `ContentHasher` on create + path change. Acceptance gate: open HTML mockup in browser. |
| 3 | Feature 3 — File Discoverer | Not Started | sonnet | `FileDiscoverer` (Win32 `FindFirstFileW`/`FindNextFileW` recursive scan; pattern match via `AssetTypeRegistry`; filter registered files; `*.folder/` handling). `GenerateDefaultId()`. Register `discover_files` DiaAPI command. `CompoundCommand` for bulk-add. File Discoverer panel (root path input, results table with checkboxes, editable type/ID, Add Selected). Confirmation dialog. Acceptance gate: open HTML mockup in browser. |
| 4 | Feature 4 — Relationship Editor | Not Started | sonnet | `AddRelationshipCommand`, `RemoveRelationshipCommand` (IEditorCommand). Register `add_relationship`, `remove_relationship`, `get_forward_refs`, `get_reverse_refs` DiaAPI commands. Relationship section in Record Editor panel (forward/reverse lists, remove buttons, clickable navigation links, Add Relationship inline form with autocomplete). Acceptance gate: open HTML mockup in browser. |
| 5 | Feature 6 — Validation Panel | Not Started | sonnet | Register `asset_catalogue.validate` DiaAPI command (calls `AssetRegistry::Validate()`). Validation Panel HTML/JS (Validate button, summary bar, error table: severity icon / Asset ID / error type / message, click-to-navigate, empty/clean states, re-validate clears results). Unit tests for command handler JSON serialization. Acceptance gate: open HTML mockup in browser. |
| 6 | Feature 7 — Asset Type Editor Routing | Not Started | sonnet | `AssetTypeEditorRegistry` (HashTable, 32 cap). Register `open_asset` DiaAPI command. `ShellExecuteExW` fallback (folder → "explore", others → "open", no association → warning log). Wire double-click + Open button in Asset List. Wire "Open in Editor" button in Record Editor. Unit tests for registry + handler. Acceptance gate: open HTML mockup in browser. |
| 7 | Feature 8 — Catalogue Rules UI | Not Started | sonnet | `LoadRulesHandler`, `DryRunHandler`, `ApplyRulesHandler` command handlers. `ApplyRulesCommand` (IEditorCommand wrapping `CatalogueRulesEngine::Apply` as CompoundCommand). Rules Panel HTML/JS (rules list, changeset preview with conflict highlighting + manual override badges, action bar). Manual override skip logic in Apply handler. Asset List refresh after Apply. Unit tests for handlers. Acceptance gate: open HTML mockup in browser. |
| 8 | Feature 5 — Relationship Graph View | Not Started | sonnet | `GetForwardRefsHandler`, `GetReverseRefsHandler` (already registered in Feature 4 — reuse). VisJS relationship graph panel (`relationship-graph/index.html`, `graph.js`, `graph.css`). Wire asset selection to graph population. Node expand action (double-click). Click-to-navigate. Type-color map, root node styling, legend bar, empty state message. Unit tests for command handler JSON serialization. Acceptance gate: open HTML mockup in browser. |

## Session Notes

### 2026-05-04
- Plan created. All 8 feature specs Approved.
- Build order: 1 → 2 → 3 → 4 → 6 → 7 → 8 → 5 (per system spec — graph view last, it consumes get_forward/reverse_refs already registered in Feature 4).
- Tasks are numbered by spec feature number for traceability; ordered in the plan by implementation dependency.
- Feature 4 `get_forward_refs`/`get_reverse_refs` DiaAPI commands are also consumed by Feature 5 (graph) — register once in Feature 4, reuse in Feature 5.
- HTML mockup at `docs/specs/features/dia/diaassetcatalogueeditor/mockups/diaassetcatalogueeditor.html` — open in browser to validate each UI feature.
