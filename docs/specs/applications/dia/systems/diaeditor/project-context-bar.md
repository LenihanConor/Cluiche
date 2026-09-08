# Feature Spec: Project Context Bar

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaEditor | @docs/specs/applications/dia/systems/diaeditor/diaeditor.md |
| Feature | **Project Context Bar** | (this document) |

## Problem Statement

Editor plugins have no shared concept of which `.diagame` project is currently open — each plugin independently manages its own file paths, so opening a project requires repetitive per-plugin file loading and offers no automatic coordination when the project changes or when a live game connection resolves the project automatically.

## Acceptance Criteria

- [ ] The existing Plugin Lifecycle Toolbar centre zone shows a project button: name + `.diagame` filename, or "No project" when none is loaded
- [ ] Clicking the project button opens a dropdown with: **Open .diagame…**, **Recent** (last 5 paths), **Reveal in Explorer**, **Close Project**
- [ ] `IEditorContext` gains `LoadProject(path)`, `GetProject()`, and `OnProjectChanged(callback)` — all plugins receive the callback when the project changes
- [ ] `EditorModel` stores a `ProjectContext` struct: `diagamePath`, derived `applicationManifestPath` (first `manifest`-type import from `.diagame`), `assetCataloguePath` (from `config.assetCatalogue`), `assetRoot` (from `config.assetRoot`)
- [ ] CluicheEditor can be launched with `--project=<path>` to load a `.diagame` at startup; if a game is already running at the default port it connects automatically — live connection takes precedence and updates the project context
- [ ] When `GameConnectionManager` connects and the game reports its `.diagame` path via `get_app_state`, it calls `IEditorContext::LoadProject()` automatically — the toolbar project button updates accordingly
- [ ] Live connection project loading takes priority over any manually opened project (live state is source of truth while connected; project clears to "No project" on disconnect — no automatic restore)
- [ ] Last opened `.diagame` path and recent projects list (last 5) persist across restarts in `.cluicheproj` `editor_state` — written on every `LoadProject` and `CloseProject` call
- [ ] `.diagame` schema gains a `config.assetCatalogue` field pointing to the asset catalogue file; `DiaGameConfig` and `JsonDiaGameSerializer` updated accordingly
- [ ] All four existing plugins (`DiaApplicationFlowEditor`, `DiaApplicationEditor`, `DiaAssetCatalogueEditor`, `DiaAssetRuntimeEditor`) are updated to subscribe to `OnProjectChanged` and derive their file paths from `ProjectContext` — per-plugin manual file open is no longer the primary workflow (though it remains as a fallback)
- [ ] If `--project` is supplied and the path does not exist, the editor starts with "No project" and logs a warning — it does not crash

## Design

### ProjectContext

```cpp
namespace Dia::Editor {
    struct ProjectContext {
        static constexpr int kMaxPath = 512;

        char diagamePath[kMaxPath];             // Absolute path to .diagame
        char applicationManifestPath[kMaxPath]; // First manifest-type import
        char assetCataloguePath[kMaxPath];       // config.assetCatalogue (may be empty)
        char assetRoot[kMaxPath];                // config.assetRoot (may be empty)

        bool IsValid() const { return diagamePath[0] != '\0'; }
    };
}
```

### IEditorContext additions

```cpp
namespace Dia::Editor {
    using ProjectChangedCallback = void(*)(const ProjectContext&, void* userData);

    class IEditorContext {
    public:
        // ... existing methods ...

        // Project context
        bool LoadProject(const char* diagamePath);
        const ProjectContext& GetProject() const;
        void OnProjectChanged(ProjectChangedCallback callback, void* userData);
    };
}
```

`LoadProject` reads the `.diagame` via `Dia::Game::DiaGameManifestLoader`, populates `ProjectContext`, then fires all registered `OnProjectChanged` callbacks. Returns `false` and leaves context unchanged if the file cannot be loaded.

### DiaGameConfig schema addition

```cpp
struct DiaGameConfig {
    Dia::Core::Containers::String256 assetRoot;
    Dia::Core::Containers::String256 assetCatalogue; // NEW — path to .catalogue.json
};
```

`JsonDiaGameSerializer` reads the optional `"assetCatalogue"` field from the `"config"` block. Unknown fields are preserved via `rawConfig` — no existing `.diagame` files break.

### Toolbar centre zone

The React `Toolbar.tsx` centre zone replaces its spacer with a `ProjectContextButton` component:

```
┌──────────────────────────────────────────────────────────────────┐
│ [Plugin Icons...]  │  📁 CluicheTest · cluiche.diagame ▾  │  ● Connected │
└──────────────────────────────────────────────────────────────────┘
```

- Subscribes to `"project_changed"` topic from `WebUIBridge`
- Dropdown: Open .diagame… | Recent ▶ (submenu) | Reveal in Explorer | ─── | Close Project
- "No project" state shows a dimmed placeholder with Open affordance
- When live-connected and project was set by connection, a small live indicator (green dot) appears on the button to show it was auto-resolved

### Startup argument

`EditorViewModule::DoStart` checks `GetCommandLineArgs()` for `--project=<path>`:
- If found, calls `IEditorContext::LoadProject(path)` after model initialises
- Then checks if a game is running at `localhost:8080` (or `--connect=<addr>` if supplied); if reachable, calls `GameConnectionManager::Connect()` — on success, live context overrides the CLI-supplied project

### Live precedence

`GameConnectionController` on connect:
1. Sends `get_app_state` to the game
2. Game response includes `diagame_path` field
3. Controller calls `IEditorContext::LoadProject(diagame_path)` — this fires `OnProjectChanged` for all plugins
4. `ProjectContext::source` flag is set to `kLive` (vs `kManual`)
5. On disconnect: always clear to "No project" — user re-opens manually via the project button

### Plugin migration pattern

Each plugin subscribes in `OnLoad`:

```cpp
void DiaApplicationFlowEditorPlugin::OnLoad(IEditorContext* ctx) {
    ctx->OnProjectChanged([](const ProjectContext& p, void* ud) {
        auto* self = static_cast<DiaApplicationFlowEditorPlugin*>(ud);
        if (p.IsValid())
            self->LoadManifest(p.applicationManifestPath);
        else
            self->ClearManifest();
    }, this);

    // Load immediately if project already open
    if (ctx->GetProject().IsValid())
        LoadManifest(ctx->GetProject().applicationManifestPath);
}
```

| Plugin | Derives path from |
|--------|------------------|
| DiaApplicationFlowEditor | `ProjectContext::applicationManifestPath` |
| DiaApplicationEditor (v1) | `ProjectContext::applicationManifestPath` |
| DiaAssetCatalogueEditor | `ProjectContext::assetCataloguePath` |
| DiaAssetRuntimeEditor | No file path — validates connected game matches `ProjectContext::diagamePath` via `get_app_state` response |

Per-plugin manual file open is retained as a fallback (for opening a one-off file outside the current project) but is no longer the primary workflow.

### Request / topic handlers

| Handler | Type | Description |
|---------|------|-------------|
| `project.open` | Request | Opens native file dialog filtered to `.diagame`; calls `LoadProject`; returns `{ok, path}` |
| `project.open_path` | Request | Loads a specific path (used by Recent list and `--project` arg); returns `{ok, error?}` |
| `project.close` | Event | Clears project context; fires `OnProjectChanged` with empty `ProjectContext` |
| `project.get_recent` | Request | Returns last 5 recently opened paths from `.cluicheproj` `editor_state.recent_projects` |
| `project_changed` | Topic (push) | C++ → JS when project context changes; payload: `{name, diagamePath, source}` |

### Persistence

Last opened project and recent list are stored in `.cluicheproj` `editor_state` — no new files needed:

```json
{
    "version": 1,
    "name": "Test",
    "manifests": ["Data/test-editor-plugins.diaapp"],
    "editor_state": {
        "last_project": "C:/Projects/CluicheTest/cluichetest.diagame",
        "recent_projects": [
            "C:/Projects/CluicheTest/cluichetest.diagame",
            "C:/Projects/MyGame/mygame.diagame"
        ]
    }
}
```

`EditorModel::LoadProject` writes back to `.cluicheproj` after a successful load. `EditorModelModule::DoStart` reads `editor_state.last_project` at startup and calls `LoadProject` automatically if the path still exists. `CloseProject` clears `last_project` and saves.

Note: `.cluicheproj` is committed, so recent projects will appear in version control. If that is undesirable, the `editor_state` block can be added to `.gitignore` as a partial-file exclusion, or the team can accept it as per-developer noise.

## Implementation Files

### New Files
- `Dia/DiaEditor/Project/ProjectContext.h` — `ProjectContext` struct + `kLive`/`kManual` source enum
- `Cluiche/CluicheEditor/UI/src/toolbar/ProjectContextButton.tsx` — React dropdown component

### Modified Files
- `Dia/DiaEditor/MVC/IEditorContext.h` — `LoadProject`, `GetProject`, `OnProjectChanged`
- `Dia/DiaEditor/MVC/EditorModel.h/.cpp` — `ProjectContext` storage, callback list, `LoadProject` impl
- `Dia/DiaGame/DiaGameConfig.h` — add `assetCatalogue` field
- `Dia/DiaGame/Serialization/JsonDiaGameSerializer.cpp` — read optional `assetCatalogue`
- `Dia/DiaEditor/Connection/GameConnectionController.h/.cpp` — call `LoadProject` on connect; restore on disconnect
- `Cluiche/CluicheEditor/ApplicationFlow/Modules/EditorModelModule.h/.cpp` — read `--project` arg; read/write `editor_state` in `.cluicheproj`
- `Cluiche/CluicheEditor/UI/src/layout/Toolbar.tsx` — replace centre spacer with `ProjectContextButton`
- `Dia/DiaApplicationEditor/DiaApplicationFlowEditorPlugin.h/.cpp` — subscribe to `OnProjectChanged`
- `Dia/DiaApplicationEditor/DiaApplicationEditorPlugin.h/.cpp` — subscribe to `OnProjectChanged`
- `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h/.cpp` — subscribe to `OnProjectChanged`
- `Dia/DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.h/.cpp` — subscribe to `OnProjectChanged`
- `Dia/DiaDebugServer/Handlers/AppStateHandler.cpp` — add `diagame_path` field to `get_app_state` response
- `Dia/DiaEditor/DiaEditor.vcxproj` + `.filters` — add `ProjectContext.h`
- `Cluiche/CluicheEditor/CluicheEditor.vcxproj` + `.filters` — add `ProjectContextButton.tsx` reference

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | StringCRC for all IDs | **Compliant** — callback registration uses `StringCRC` handler keys; topic names are `StringCRC` constants in `Dia::Editor::DataPath` |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — `EditorModelModule` owns project loading; DiaEditor library classes have no Module dependency (SED-015/AED-001) |
| Platform | PD-003 | Component-based entities | **N/A** — no entity construction |
| Platform | PD-004 | No STL in public APIs | **Compliant** — `ProjectContext` uses fixed `char[]` arrays; `ProjectChangedCallback` is a plain function pointer + `void*`; no `std::string` or `std::function` in public headers |
| Platform | PD-005 | x64 only | **Compliant** — no 32-bit considerations |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — all new files added to `.vcxproj` and `.vcxproj.filters` |
| Platform | PD-007 | C++20 required | **Compliant** — no C++20 incompatibilities introduced |
| Platform | PD-008 | Directory.Build.props owns build settings | **Compliant** — no per-project build overrides |
| Platform | PD-009 | Generated output under `Cluiche/out/<AppName>/` | **Compliant** — no new generated files; persistence is in `.cluicheproj` which is a source file, not generated output |
| Platform | PD-010 | `.diagame` is project root; systems resolve from it | **Compliant** — this feature IS the mechanism that makes PD-010 real for the editor; all plugins now resolve from `.diagame` via `ProjectContext` |
| Application | AED-001 | CluicheEditor owns app flow; DiaEditor is pure library | **Compliant** — `IEditorContext::LoadProject` is a library method; `EditorModelModule` (CluicheEditor) drives it; `--project` arg parsing lives in `EditorModelModule` |
| Application | AED-002 | Plugins from `.diaapp` manifest | **Compliant** — plugin loading unchanged; this feature adds project context on top |
| Application | AED-004 | WebSocket network-first connection | **Compliant** — live project loading goes through `GameConnectionManager`; no alternative connection path added |
| Application | AED-006 | `.cluicheproj` is top-level project file | **Compliant — orthogonal concerns.** `.cluicheproj` loads *editor plugins* (via `.diaapp` manifests); `.diagame` is the *game project* being edited. This feature adds `.diagame` as a new concept alongside `.cluicheproj`, not replacing it. `editor_state` in `.cluicheproj` may optionally persist the last-opened `.diagame` path. |
| System | SED-001 | `IEditorPlugin` interface minimal and stable | **Compliant** — plugin interface unchanged; `OnProjectChanged` is on `IEditorContext`, not `IEditorPlugin` |
| System | SED-004 | WebSocket JSON protocol | **Compliant** — `diagame_path` field added to existing `get_app_state` response; additive, no breaking change |
| System | SED-008 | Observer pattern, not polling | **Compliant** — `OnProjectChanged` callback list follows observer pattern |
| System | SED-011 | Two-tier observer paths | **Compliant** — `"project_changed"` is a shared framework topic in `Dia::Editor::DataPath` |
| System | SED-012 | StringCRC constants in DataPath | **Compliant** — `DataPath::kProjectChanged` added as a typed constant |
| System | SED-014 | `.cluicheproj` is top-level project file | **Compliant** — same reasoning as AED-006 above. SED-014 governs editor plugin loading; this feature governs game project context. Orthogonal. |
| System | SED-015 | DiaEditor is pure library | **Compliant** — `ProjectContext`, `IEditorContext` changes are pure library; no Module/Phase dependency |
| System | SED-016 | GameConnectionManager boots clean | **Compliant** — project loading on connect is a side effect of a successful connection, not a boot-time requirement |
| System | SED-020 | Plugin output under `Cluiche/out/CluicheEditor/<PluginName>/` | **Compliant** — no new generated output files; persistence routes through `.cluicheproj` |
| System | SED-021 | Per-plugin session context via `.context.json` | **Compliant** — project context is editor-shell state stored in `.cluicheproj`, not plugin session state; plugins continue to own their own `.context.json` files |

## Open Questions

| # | Question | Status | Resolution |
|---|----------|--------|------------|
| 1 | AED-006 / SED-014 say `.cluicheproj` is the top-level project file, but this feature roots everything in `.diagame`. Are these in conflict, or does `.cluicheproj` wrap `.diagame`? | **Resolved** | Orthogonal concerns. `.cluicheproj` loads editor plugins; `.diagame` is the game project being edited. No structural conflict. `editor_state` in `.cluicheproj` may cache the last-opened `.diagame` path for convenience. |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Architecture | AED-006/SED-014 say `.cluicheproj` is the top-level project file. This feature roots everything in `.diagame`. Are they the same file, does `.cluicheproj` reference `.diagame`, or does `.cluicheproj` get superseded? | Resolved — orthogonal. `.cluicheproj` loads editor plugins; `.diagame` is the game project. The Project Context Bar opens `.diagame` independently. `editor_state` in `.cluicheproj` may cache the last-opened `.diagame` path as a convenience. | Orthogonal — no conflict. `.cluicheproj` owns editor plugin loading; `.diagame` owns game project content. Both coexist. |
| 2 | Live precedence | When disconnecting live, should the editor restore the last manually-opened project or clear to "No project"? | Restore last manual project if one existed; clear if the session was started via `--project` only | Always clear to "No project" on disconnect — user re-opens manually. No automatic restore. |
| 3 | Plugin fallback | If `ProjectContext::assetCataloguePath` is empty (field not yet in the `.diagame`), should `DiaAssetCatalogueEditor` fall back to convention (`<assetRoot>/assets.catalogue.json`) or show "No catalogue configured"? | Show "No catalogue configured" with a link to add the field — safer than silent convention | Show "No catalogue configured" — no silent fallback. Drives adoption of the `config.assetCatalogue` field. |
| 4 | DiaAssetRuntimeEditor | This plugin has no file path to load — on `OnProjectChanged` it should validate the connected game matches the new project. What if they don't match (game is running a different `.diagame` than the opened project)? | Show a warning banner in the plugin: "Connected game is running a different project" | Live always wins — `GameConnectionController` calls `LoadProject` with the game's `.diagame` on connect, overriding any prior manual project. All plugins receive `OnProjectChanged` and reload. No warning needed. |
| 5 | `get_app_state` protocol | The `diagame_path` field is new on the game side (DiaDebugServer). Is this already in the `get_app_state` response, or does DiaDebugServer need a change too? | DiaDebugServer needs a small addition to include `diagame_path` in `get_app_state` — flag as a dependency | Tracked as a task within this spec — single additive field on an existing response. Add `Dia/DiaDebugServer/Handlers/AppStateHandler.cpp` to implementation files. |
| 6 | Automation | `--project` + auto-connect: if the game is not yet running when the editor starts (common in CI), how long should it wait before giving up on auto-connect? | Attempt once at startup; if not reachable, stay offline — user or script triggers connect manually | Single attempt at startup only. No retry, no timeout flag. CI scripts handle retry externally if needed. |
| 7 | Recent projects | Should the recent list show full paths or just project names + truncated paths? | Name + truncated path (last 2 path segments) with full path as tooltip | Full absolute path. Developers know their paths and don't need truncation. |
| 9 | `.cluicheproj` writeback | `editor_state` in `.cluicheproj` will be written on every `LoadProject`. If the file is read-only or under source control, this will fail silently or visibly — how should write failure be handled? | Log a warning and continue; recent projects are a convenience, not critical state | Log warning only — non-critical state, no UI notification needed. |
| 8 | `.diagame` schema change | Adding `config.assetCatalogue` to `DiaGameConfig` touches `DiaGame` — a different system. Is that an acceptable cross-system change from this feature spec, or should it be a separate amendment to the DiaGame system spec? | Amendment note in this spec is sufficient; no new DiaGame feature spec needed for a single additive field | Tracked as a task in this spec. Single additive optional field; `rawConfig` preserves unknowns so no existing `.diagame` files break. |

## Status

`Done` — 2026-05-18
