# Application Spec: CluicheEditor

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose

CluicheEditor is a concrete editor application for building and debugging Cluiche games. It provides a plugin-based editor environment where each Dia subsystem can register its own specialized editor tools. Built on the DiaEditor framework (from the Dia application), CluicheEditor serves as the host application that loads system-specific editor plugins (like DiaApplicationEditor for manifest editing), provides a dockable web-based UI, enables live debugging of running games via WebSocket, and integrates the DiaCLI command system for file manipulation.

CluicheEditor manages `.cluicheproj` project files — the top-level project definition that references `.diaapp` manifests and stores editor state (open files, layout, last connection). A project file is the entry point for all editing sessions.

**Target Users:** Game developers working on Cluiche projects who need to edit manifests, debug running games, configure systems, and use specialized editor tools.

## Systems

| System | Description | Spec |
|--------|-------------|------|
| *(Host Application Only)* | CluicheEditor has no internal Dia systems — it owns application flow (Modules/Phases) that wrap DiaEditor library classes | N/A |

CluicheEditor owns all application flow: the `CluicheEditorProcessingUnit` (extends `Dia::Application::ProcessingUnit`) wires together thin Module and Phase subclasses that delegate to DiaEditor library classes. DiaEditor has no DiaApplication dependency — it is a pure library. CluicheEditor is the seam where the library and the application framework meet.

**Key Dependencies:**
- **Dia.DiaEditor** - The editor framework providing MVC, plugin system, undo/redo, UI, commands, and live connection
- **Dia.DiaUICEF** - CEF-based IUISystem implementation (editor-only — CEF is too heavy at ~100MB for shipping in games)
- **Dia.DiaApplication** - For ProcessingUnit/Phase/Module architecture
- **Dia.DiaCore** - For containers, StringCRC, and foundation utilities
- **Dia.DiaDebugProtocol** - Shared message types for editor-game communication (header-only)

**Loaded Plugins (Examples):**
- **DiaApplicationEditor** (from Dia.DiaApplication) - Edits .diaapp manifests
- **DiaGraphicsEditor** (future) - Edits rendering settings
- **DiaPhysicsEditor** (future) - Edits physics configuration

## Application-Specific Architecture

### Application Flow Pattern

CluicheEditor owns the ProcessingUnit, all Module wrappers, and all Phase subclasses. DiaEditor library objects are members of the Modules; the Modules drive them.

```
CluicheEditorProcessingUnit (extends ProcessingUnit)
  ├─ EditorModelModule       → owns Dia::Editor::EditorModel
  ├─ CommandHistoryModule    → owns Dia::Editor::CommandHistory
  ├─ EditorViewModule        → owns Dia::Editor::EditorView + Win32Window + CEFUISystem
  ├─ EditorViewControllerModule → owns Dia::Editor::EditorViewController
  ├─ GameConnectionModule    → owns Dia::Editor::GameConnectionManager
  ├─ CluicheEditorBootPhase
  ├─ CluicheEditorRunningPhase
  └─ CluicheEditorShutdownPhase
```

The Module layer is intentionally thin: `DoStart` initializes the library object, `DoUpdate` ticks it, `DoStop` shuts it down. No business logic lives in the Modules themselves.

Win32-specific code (window creation, Win32Window) lives in `EditorViewModule` only — DiaEditor library classes have no platform header dependencies.

```cpp
// wWinMain — CEF subprocess guard MUST come before any Dia initialization
int APIENTRY wWinMain(HINSTANCE hInstance, ...) {
    CefMainArgs mainArgs(hInstance);
    int exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr);
    if (exitCode >= 0) return exitCode;
    
    auto* pu = new Cluiche::Editor::CluicheEditorProcessingUnit();
    pu->Start();
    pu->Update();  // Runs until FlaggedToStopUpdating() returns true
    pu->Stop();
    delete pu;
    return 0;
}
```

### Project Files

CluicheEditor opens `.cluicheproj` files as the top-level project definition:
```json
{
  "version": 1,
  "name": "MyGame",
  "manifests": [
    "config/game.diaapp",
    "config/editor-plugins.diaapp"
  ],
  "editor_state": {
    "layout": "default.layout.json",
    "open_files": ["config/game.diaapp"],
    "last_connection": { "address": "localhost", "port": 8080 }
  }
}
```

`EditorModel::LoadProject()` loads the `.cluicheproj`, then calls `EditorApplication::LoadEditorManifest()` for each referenced `.diaapp`. The project defines *what to work on*; manifests define *what tools to load*.

### Plugin Loading

Plugins are specified in `.diaapp` manifests referenced by the project file:
```json
{
  "editor": {
    "enabled": true,
    "plugins": [
      {
        "type": "DiaApplicationEditor",
        "instance_id": "app_editor",
        "layout_mode": "dockable"
      }
    ]
  }
}
```

Each plugin:
1. Lives with its parent system (e.g., `Dia/DiaApplication/Editor/`)
2. Implements `IEditorPlugin` interface (defined in DiaEditor)
3. Registers via `REGISTER_EDITOR_PLUGIN` macro
4. Auto-discovered and loaded by CluicheEditor when the manifest is processed

### Execution Model

CluicheEditor runs as a standard ProcessingUnit with three editor-specific phases:
- **CluicheEditorBootPhase** - Starts EditorModelModule + CommandHistoryModule; transitions immediately to Running
- **CluicheEditorRunningPhase** - Starts all 5 modules (EditorViewModule creates the window and initializes CEF); polls `EditorModel::IsCloseRequested()` for `FlaggedToStopUpdating()`
- **CluicheEditorShutdownPhase** - Final cleanup; returns `FlaggedToStopUpdating() = true` immediately

`EditorViewModule::DoStart` creates the `Win32Window`, sets the close callback → `EditorModel::RequestClose()`, then initializes `CEFUISystem` with windowed rendering and the subprocess path, and loads the React shell page.

## Platform Dependencies

### From Dia Application
- **DiaEditor** - Core framework (MVC, plugin system, undo/redo, UI, commands, live connection)
- **DiaUICEF** - CEF-based IUISystem implementation (editor-only, replaces deprecated DiaUIAwesomium)
- **DiaCore** - Containers, StringCRC, CRC, Observer pattern
- **DiaApplication** - ProcessingUnit/Phase/Module architecture
- **DiaAPI** - Command registry and infrastructure
- **DiaCLI** - Python CLI framework (embedded)
- **DiaDebugProtocol** - Shared message types for editor-game communication (header-only)
- **DiaWebSocket** - WebSocket client/server abstraction (wraps websocketpp)

### External Dependencies
- **CEF (Chromium Embedded Framework)** - Web-based UI rendering (~100MB Release, editor-only)
- **react-mosaic** (JavaScript) - Docking layout library
- **React + TypeScript** (JavaScript) - UI component framework
- **fuse.js** (JavaScript) - Fuzzy search for command palette
- **vis-network** (JavaScript) - Hierarchy visualization
- **recharts** (JavaScript) - Time-series charts

## Out of Scope

What CluicheEditor deliberately does NOT provide:

- **System-specific editor logic** - That belongs in plugins (e.g., DiaApplicationEditor)
- **Framework infrastructure** - That belongs in DiaEditor system
- **Standalone tools** - CluicheEditor is for integrated editing, not one-off scripts
- **Game runtime execution** - CluicheEditor connects to games but doesn't run them (use CluicheTest)
- **Game runtime UI** - DiaUICEF is editor-only; CEF is too heavy for shipping in games. Games use DiaUIUltralight or DiaUIAwesomium for in-game UI.
- **Asset authoring** - No texture painting, model creation, etc. (use external tools + asset pipeline)

## Key Users / Personas

1. **Game Programmers** - Editing manifests, debugging ProcessingUnits, configuring modules
2. **Technical Designers** - Tweaking gameplay values, validating configurations
3. **Engine Developers** - Building custom editor plugins for new systems
4. **QA / Testers** - Connecting to running builds for live inspection

## Inherited Binding Decisions

These decisions from the parent platform spec are binding constraints on CluicheEditor:

| Source | ID | Decision | Impact on CluicheEditor |
|--------|----|----------|-------------------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | Plugin IDs, event types, data paths all use StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture for app structure | CluicheEditorApp extends EditorApplication (which extends ProcessingUnit) |
| Platform | PD-003 | Component-based entities | Plugins follow component pattern where applicable |
| Platform | PD-004 | No STL containers in public APIs | Host app uses DiaEditor's STL-free public interfaces |
| Platform | PD-006 | Visual Studio project files are source of truth | CluicheEditor built as .vcxproj |

## Decisions

<!-- Decisions specific to this application. Binding decisions cascade to all systems within CluicheEditor.
     AI: Always check parent platform decisions (Cluiche.md) and Dia decisions (dia.md) first — those take precedence.
     Use AED- prefix for application-level decision IDs (CluicheEditor Application Decision). -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AED-001 | CluicheEditor owns all application flow (ProcessingUnit, Modules, Phases); DiaEditor is a pure library | DiaEditor must be independently testable without the application framework; CluicheEditor is the seam where library meets framework; thin Module wrappers delegate to DiaEditor library classes | Entire application | Accepted | Yes |
| AED-002 | Plugins specified in .diaapp manifest "editor" section, referenced from .cluicheproj | Single source of truth per project; version controlled; follows existing manifest pattern; project file separates "what to work on" from "what tools to load" | Plugin loading | Proposed | Yes |
| AED-003 | Each system owns its editor as `<System>/Editor/` subdirectory | Locality of related code; system developers maintain their own editors; clear ownership | Plugin organization | Proposed | Yes |
| AED-004 | Editor connects to games via WebSocket (network-first) | Enables remote debugging, multiple editors, console development from day one; more complex but maximally flexible | Live connection | Proposed | Yes |
| AED-005 | UI built with React + DiaUICEF (CEF) + react-mosaic | Modern web stack; docking support; CEF replaces deprecated Awesomium; DiaUICEF is editor-only (too heavy for games) | UI architecture | Proposed | Yes |
| AED-006 | `.cluicheproj` is the top-level project file | Separates editor project concerns from .diaapp manifests; stores editor state; extensible for future project-level needs | Project management | Proposed | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child systems · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should CluicheEditor have any systems, or is it purely a host? | Purely a host — all functionality comes from DiaEditor framework and loaded plugins (AED-001) |
| 2 | Plugins | Where do plugins like DiaApplicationEditor get spec'd? | Under their parent application (Dia) as systems, not under CluicheEditor |
| 3 | Dependencies | Does CluicheEditor depend on Dia or DiaEditor? | Both — Dia is the application containing DiaEditor system |
| 4 | Scope | Can CluicheEditor edit multiple projects simultaneously? | Not initially — single project; multi-project is future enhancement |
| 5 | Live Connection | Should same-process connection be supported alongside WebSocket? | Not initially — WebSocket-first per AED-004; same-process can be optimization later |
| 6 | UI Runtime | Is DiaUICEF (CEF) used in games? | No — DiaUICEF is editor-only. CEF is ~100MB, too heavy for shipping. Games use DiaUIUltralight or DiaUIAwesomium. |
| 7 | Project File | What is the project file format? | `.cluicheproj` (JSON) — references .diaapp manifests, stores editor state. See DiaEditor SED-014. |
| 8 | Docking | Which docking library? | react-mosaic (modern React) — replaces golden-layout reference from earlier drafts |

## Status

`Approved` - Ready for implementation

## Notes

**CEF Subprocess Note:**
CluicheEditor.exe handles both main and CEF helper processes via `CefExecuteProcess()` check at the top of `main()`. This must happen before any Dia initialization.

**Plugin Discovery Flow:**
1. User opens `MyGame.cluicheproj`
2. `EditorModel::LoadProject()` parses the project file
3. For each `.diaapp` in `manifests[]`, `EditorApplication::LoadEditorManifest()` is called
4. Each manifest's `editor.plugins[]` section lists plugin types
5. `EditorPluginRegistry::CreatePlugin()` instantiates each plugin by StringCRC type ID
6. Plugin's `OnLoad()` is called with EditorModel reference
7. Plugin's `GetUIPath()` returns `dia://` URL for its web UI
8. DockingManager creates panel for the plugin
