# Plan: Project Context Bar

**Spec:** @docs/specs/applications/dia/systems/diaeditor/project-context-bar.md  
**Status:** In Progress

## Session Notes

**Spec decisions summary (Platform → App → System → Feature):**

PD-001: StringCRC for IDs — topic names use `StringCRC` constants in `Dia::Editor::DataPath`.  
PD-004: No STL in public APIs — `ProjectContext` uses `char[]`; callbacks use plain function pointers + `void*`.  
SED-015: DiaEditor is pure library — `ProjectContext`, `IEditorContext` go in DiaEditor; `EditorModelModule` (CluicheEditor) drives everything.  
SED-008: Observer pattern — `OnProjectChanged` callback list, not polling.  
SED-011/012: Two-tier observer, `DataPath::kProjectChanged` constant.  
AED-001: CluicheEditor owns app flow — `--project` arg parsing and `editor_state` persistence live in `EditorModelModule`, not in DiaEditor.  
AED-006/SED-014: Orthogonal — `.cluicheproj` loads editor plugins; `.diagame` is the game project. Both coexist.

**Architecture notes from code survey:**
- `IEditorContext.h` does **not** exist — it is a new file to create.
- `EditorModel::LoadProject(path)` already loads `.cluicheproj`. New method will be named `LoadDiagameProject` to avoid collision. The spec's `IEditorContext::LoadProject` maps to `EditorModel::LoadDiagameProject`.
- `EditorPluginContext::mModel` is `EditorModel*` — plugins access the new API via `context.mModel->OnDiagameProjectChanged(...)`.
- `GameConnectionController` currently holds no reference to `EditorModel` — needs a new `SetEditorModel(EditorModel*)` or the connection module passes it in during initialization.
- `game_info` proto is sent game→editor on connect. `get_app_state` is a **new JSON request** the editor sends to the game post-connect; DebugServer needs a new `get_app_state` query handler returning `diagame_path`.
- `DiaApplicationFlowEditor` and `DiaApplicationEditor` directories — their plugin files exist under `Dia/DiaApplicationFlowEditor/` and `Dia/DiaApplicationEditor/` respectively.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **ProjectContext.h + IEditorContext.h** — create `Dia/DiaEditor/Project/ProjectContext.h` (struct + `ProjectSource` enum + `ProjectChangedCallback` typedef); create `Dia/DiaEditor/MVC/IEditorContext.h` (pure virtual interface with `LoadDiagameProject`, `GetDiagameProject`, `OnDiagameProjectChanged`, `ClearDiagameProject`); add both to `DiaEditor.vcxproj` + `.filters` | Build pass | Done | haiku | New files only; no logic |
| 2 | **DiaGameConfig schema** — add `assetCatalogue` field (`String256`) to `DiaGameConfig` in `DiaGameManifest.h`; update `JsonDiaGameSerializer.cpp` to read/write optional `"asset_catalogue"` from the `"config"` JSON block (preserves `rawConfig` passthrough) | Build pass | Done | haiku | Additive; no existing files break |
| 3 | **EditorModel game-project API** — make `EditorModel` extend `IEditorContext`; add `mDiagameContext` (`ProjectContext`), callback list (8 slots), recent-projects list; implement `LoadDiagameProject`, `GetDiagameProject`, `OnDiagameProjectChanged`, `ClearDiagameProject`, `SetRecentProjects`, `GetRecentProject`; add `DataPath::kProjectChanged` StringCRC constant | Build pass | Done | sonnet | Core of the feature |
| 4 | **EditorModelModule — `--project` arg + `editor_state` persistence** — parse `--project=<path>` named arg; call `LoadDiagameProject`; read/write `editor_state.last_project` + `recent_projects` in `.cluicheproj`; mirror recent list into `EditorModel` on each change | Build pass | Done | sonnet | Lives in CluicheEditor per AED-001 |
| 5 | **DebugServer — `get_app_state` + `diagame_path`** — add `SetDiagamePath(const char*)` to `DebugServer`; register `get_app_state` JSON query; `DebugServerHostModule::OnConfigure` picks up `diagame_path` from JSON config | Build pass | Done | sonnet | Game-side ready; wire via manifest config when DebugServerHostModule is added to CluicheTest |
| 6 | **GameConnectionController — auto-load on connect/disconnect** — add `SetEditorContext(IEditorContext*)` to controller; send `get_app_state` after handshake; call `LoadDiagameProject` on response; call `ClearDiagameProject` on `DisconnectInternal` | Build pass | Done | sonnet | Wired via `GameConnectionEditorPlugin::OnLoad` |
| 7 | **WebSocket request handlers** — `ProjectContextController` in `DiaEditor/Project/`; registers `project.open_path`, `project.close`, `project.get_recent`, `project.open` (placeholder); pushes `project_changed` topic on context change | Build pass | Done | sonnet | Wired in `GameConnectionEditorPlugin` |
| 8 | **ProjectContextButton.tsx + Toolbar** — `ProjectContextButton.tsx` created; subscribes to `project_changed`, shows project name/filename, live green dot, dropdown with Open/Recent/Reveal/Close; `Toolbar.tsx` centre zone updated | Build pass | Done | sonnet | UI test pending in Task 14 |
| 9 | **Plugin migration — DiaAssetCatalogueEditor** — subscribes `OnDiagameProjectChanged`; loads from `assetCataloguePath` or shows "No catalogue configured" | Build pass | Done | sonnet | |
| 10 | **Plugin migration — DiaAssetRuntimeEditor** — subscribes `OnDiagameProjectChanged`; stores `diagamePath` for future validation | Build pass | Done | sonnet | |
| 11 | **Plugin migration — DiaApplicationEditor** — subscribes `OnDiagameProjectChanged`; opens `applicationManifestPath` on project change | Build pass | Done | sonnet | DiaApplicationFlowEditor does not exist yet as a plugin |
| 12 | **vcxproj + filters cleanup** — all new files referenced; `dia pipeline --target googletest` green | Build pass | Done | haiku | |
| 13 | **C++ test pass** — audit coverage across all new C++ code: `EditorModel` game-project API (load, clear, callbacks, edge cases), `JsonDiaGameSerializer` `assetCatalogue` field, `GameConnectionController` auto-load/clear flow, `ProjectContextController` request handlers, plugin migration callbacks for all 4 plugins; add missing tests to `TestEditorModel.cpp` and sibling test files; target: every public AC has at least one test | `dia run googletest` green | Done | sonnet | 57 new tests across 3 new files: TestEditorModelDiagame.cpp (26), TestJsonDiaGameSerializer.cpp (22), TestProjectContextController.cpp (9); 4549 total green |
| 14 | **UI test pass** — build and exercise `ProjectContextButton` in the running editor: no-project state shows "No project" dimmed; `project.open_path` with a valid `.diagame` updates button label; dropdown Recent list populates after 2+ loads; Close Project clears button; live-connected state shows green dot; disconnect clears to "No project"; check for regressions in existing Toolbar panel-toggle buttons and connection state indicator | Manual walkthrough + HTML mockup visual gate | Done | sonnet | 13 new Vitest tests in ProjectContextButton.test.tsx (all green); fixed 14 pre-existing Toolbar/Integration test failures by adding `request` mock + updating button count; 119 UI tests green total |

## Deferred / Out of Scope

- `project.open` native Win32 file dialog — functional placeholder returns error; can be added in follow-up
- Reveal in Explorer button — renders in UI, no-ops until Task 7 extended
- `.gitignore` partial-file exclusion for `editor_state` — team decision, not a code task
