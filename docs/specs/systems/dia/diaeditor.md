# System Spec: DiaEditor

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaEditor is the editor framework system that provides the foundation for building game development tools within the Cluiche platform. It implements an MVC architecture with a plugin system that allows each Dia subsystem to register specialized editor tools (e.g., DiaApplicationEditor for manifest editing). DiaEditor is a **pure C++ library with no DiaApplication dependency** — it contains no Module, Phase, or ProcessingUnit subclasses. Application flow (wiring modules into phases, phase transitions, lifecycle) lives entirely in the consumer executable (CluicheEditor). This separation keeps DiaEditor independently testable and reusable without the application framework.

## Responsibilities

- **MVC Architecture** - Provide EditorModel (project data), EditorView (UI), EditorViewController (command dispatch) as plain library classes
- **Plugin System** - Define IEditorPlugin interface, EditorPluginRegistry, EditorManifestLoader, and macro-based registration
- **Undo/Redo** - Framework-level command history (IEditorCommand, CommandHistory) used by all plugins
- **Live Game Connection** - GameConnectionManager: WebSocket client lifecycle, explicit on-demand connection, protocol handling
- **Data Synchronization** - Bidirectional sync between editor and game (subscribe to data, push updates)

## Public Interfaces

### Plugin System

**IEditorPlugin Interface:**
```cpp
namespace Dia::Editor {
    enum class LayoutMode {
        kFullScreen,  // Plugin takes entire window
        kDockable     // Plugin can be docked with others
    };
    
    class IEditorPlugin {
    public:
        virtual ~IEditorPlugin() = default;
        
        // Metadata
        virtual const char* GetName() const = 0;
        virtual const char* GetVersion() const = 0;
        virtual const char* GetDescription() const = 0;
        
        // UI integration
        virtual const char* GetUIPath() const = 0;  // Path to web UI assets
        virtual LayoutMode GetLayoutMode() const = 0;
        
        // Lifecycle
        virtual void OnLoad(EditorModel* model) = 0;
        virtual void OnUnload() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        
        // Plugin-specific data access
        virtual void* GetPluginData() = 0;
    };
    
    class IEditorPluginFactory {
    public:
        virtual ~IEditorPluginFactory() = default;
        virtual IEditorPlugin* Create() = 0;
    };
}
```

**EditorPluginRegistry:**
```cpp
namespace Dia::Editor {
    class EditorPluginRegistry {
    public:
        static EditorPluginRegistry& Instance();
        
        void RegisterPlugin(const StringCRC& typeId, IEditorPluginFactory* factory);
        IEditorPlugin* CreatePlugin(const StringCRC& typeId);
        void UnregisterPlugin(const StringCRC& typeId);
        
        const DynamicArrayC<StringCRC, 32>& GetRegisteredPlugins() const;
        bool HasPlugin(const StringCRC& typeId) const;
    };
}
```

**Registration Macro:**
```cpp
// Mirrors DIA_REGISTER_MODULE pattern from ApplicationTypeRegistry
#define REGISTER_EDITOR_PLUGIN(ClassName, TypeName) \
    namespace { \
        struct ClassName##Factory : public IEditorPluginFactory { \
            IEditorPlugin* Create() override { return new ClassName(); } \
        }; \
        static ClassName##Factory g_##ClassName##Factory; \
        struct ClassName##Registrar { \
            ClassName##Registrar() { \
                EditorPluginRegistry::Instance().RegisterPlugin( \
                    Dia::Core::StringCRC(TypeName), &g_##ClassName##Factory); \
            } \
        }; \
        static ClassName##Registrar g_##ClassName##Registrar; \
    }

// Usage example (in DiaApplicationEditor):
// REGISTER_EDITOR_PLUGIN(DiaApplicationEditor, "DiaApplicationEditor")
```

### Core Library Classes

DiaEditor classes are plain C++ — no Module/Phase/ProcessingUnit inheritance. They are instantiated and driven by the consumer application (CluicheEditor).

**EditorModel:**
```cpp
namespace Dia::Editor {
    class EditorModel {
    public:
        EditorModel();
        
        // Project management (.cluicheproj files)
        void LoadProject(const char* projectPath);
        void SaveProject();
        void CloseProject();
        bool HasOpenProject() const;
        const char* GetProjectPath() const;
        
        // Close request (set by window close callback, polled by running phase)
        void RequestClose();
        bool IsCloseRequested() const;
        void Reset();  // Clears state on stop
        
        // Observable pattern (notify UI and plugins of data changes)
        void RegisterObserver(const StringCRC& dataPath, Dia::Core::Observer* observer);
        void UnregisterObserver(const StringCRC& dataPath, Dia::Core::Observer* observer);
        void NotifyObservers(const StringCRC& dataPath);
    };
}
```

**Plugin Data Type Safety:**

Each plugin data struct declares a `static const StringCRC kPluginDataTypeId`. EditorModel stores the type ID alongside the `void*` and checks it on retrieval (debug assert, zero cost in Release).

```cpp
// Plugin data struct declares its type ID (one line):
struct ManifestEditorData {
    static const StringCRC kPluginDataTypeId;  // = StringCRC("ManifestEditorData")
    // ... fields ...
};

// Plugin registers data:
mEditorModel->SetPluginData(pluginId, mPluginData, ManifestEditorData::kPluginDataTypeId);

// Retrieval checks type (debug assert on mismatch):
template<typename T>
T* EditorModel::GetPluginData(const StringCRC& pluginId) {
    PluginDataEntry& entry = mPluginDataMap.Get(pluginId);
    DIA_ASSERT(entry.typeId == T::kPluginDataTypeId);
    return static_cast<T*>(entry.data);
}
```

**Observer Data Path Conventions:**

Data paths use a two-tier model — shared framework paths for cross-plugin coordination, and plugin-scoped paths for intentionally visible plugin state. Plugin `void*` data remains fully private.

*Shared paths* (framework-defined, any plugin can observe):
```cpp
namespace Dia::Editor::DataPath {
    static const StringCRC kProjectPath("project_path");
    static const StringCRC kConnectionState("connection_state");
    static const StringCRC kDirtyState("dirty_state");
    static const StringCRC kUndoRedoState("undo_redo_state");
}
```

*Plugin-scoped paths* (plugin writes, others can opt-in to observe):
```
Convention: "plugin.<plugin_id>.<path>"

Examples:
  "plugin.diaapplicationeditor.active_manifest"
  "plugin.diaapplicationeditor.selected_phase"
  "plugin.diagraphicseditor.active_pipeline"
```

Isolation is the default — a plugin that doesn't care about other plugins ignores scoped paths entirely. Cross-plugin coordination is opt-in via explicit observation of another plugin's scoped path.

**Project File (`.cluicheproj`):**

EditorModel manages `.cluicheproj` files — the top-level project definition for CluicheEditor. A project file references `.diaapp` manifests and other resources, stores editor state, and is extensible for future needs.

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
    "open_files": [
      "config/game.diaapp"
    ],
    "last_connection": {
      "address": "localhost",
      "port": 8080
    }
  }
}
```

- **`manifests`** — list of `.diaapp` files this project works with
- **`editor_state`** — user-local editor state (open files, layout, last connection). Kept in the project file for now; can be split to a separate user-local file later if version control becomes a concern.
- **Extensible** — future fields (asset paths, build configs, team settings) can be added without breaking existing projects; unknown fields are preserved on save.

`EditorApplication::LoadEditorManifest()` loads plugins from a `.diaapp`. `EditorModel::LoadProject()` loads the `.cluicheproj`, then calls `LoadEditorManifest()` for each referenced manifest. They are distinct steps: project defines *what to work on*, manifest defines *what tools to load*.

**EditorViewController:**
```cpp
namespace Dia::Editor {
    class EditorViewController {
    public:
        EditorViewController();
        
        void SetCommandHistory(CommandHistory* history);
        void SetModel(EditorModel* model);
        
        // UI event handling (view → controller → model)
        void OnUIEvent(const StringCRC& eventType, const Json::Value& data);
    };
}
```

**EditorView:**
```cpp
namespace Dia::Editor {
    class EditorView {
    public:
        EditorView();
        
        void Initialize(Dia::UI::IUISystem* uiSystem);
        void Shutdown();
        
        // Component registration (for plugins)
        void RegisterComponent(const char* componentName, const char* uiPath);
        void UnregisterComponent(const char* componentName);
        
    private:
        Dia::UI::IUISystem* mUISystem;
    };
}
```

### Manifest Loading

**EditorManifestLoader:**
```cpp
namespace Dia::Editor {
    class EditorManifestLoader {
    public:
        struct PluginEntry {
            char typeId[128];
            char instanceId[128];
        };
        using PluginCallback = void(*)(const PluginEntry&, void* userData);
        
        // Parses a .diaapp file and invokes callback for each plugin entry.
        // Consumer (CluicheEditor) calls LoadPlugin() in the callback.
        static bool Load(const char* manifestPath, PluginCallback callback, void* userData);
    };
}
```

### Undo/Redo System

**IEditorCommand:**
```cpp
namespace Dia::Editor {
    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual const char* GetDescription() const = 0;
    };
}
```

**CommandHistory:**
```cpp
namespace Dia::Editor {
    class CommandHistory {
    public:
        CommandHistory(int maxDepth = 100);
        
        void ExecuteCommand(IEditorCommand* command);
        
        // Interactive compound grouping
        void BeginCompound(const char* description);
        void EndCompound();
        bool IsCompoundActive() const;
        
        bool CanUndo() const;
        bool CanRedo() const;
        void Undo();
        void Redo();
        
        const char* GetUndoDescription() const;
        const char* GetRedoDescription() const;
        
        void Clear();
        
        // Save point tracking (drives dirty flag)
        void MarkSavePoint();
        bool IsAtSavePoint() const;
    };
}
```

### Command System

**CommandDispatcher:**
```cpp
namespace Dia::Editor {
    class CommandDispatcher : public Dia::Application::Module {
    public:
        // Execute DiaCLI command via embedded Python
        int ExecuteCommand(const char* command, const DiaAPI::CommandArgs& args);
        
        // Query available commands (for UI autocomplete)
        const DynamicArrayC<const DiaAPI::CommandInfo*, 64>& GetAvailableCommands();
        
        // Output redirection (capture stdout for console UI)
        void SetOutputCallback(OutputCallback callback);
        
    protected:
        virtual void DoStart(const IStartData* startData) override;
        virtual void DoStop() override;
        
    private:
        DiaPython::Interpreter* mPythonInterp;
        DiaAPI::CommandRegistry* mCommandRegistry;
        OutputCallback mOutputCallback;
    };
}
```

### Live Connection System

**GameConnectionManager:**
```cpp
namespace Dia::Editor {
    struct ConnectionInfo {
        const char* address;
        uint16_t port;
        bool isConnected;
        float latency;  // ms
    };
    
    enum class ConnectionResult {
        kSuccess,
        kFailed,
        kTimeout,
        kAlreadyConnected
    };
    
    class GameConnectionManager {
    public:
        // Lifecycle — Initialize creates WebSocket infrastructure (does NOT connect)
        void Initialize();
        void Shutdown();
        void Update(float deltaTime);
        
        // Explicit on-demand connection (called by UI action when game is ready)
        // Tool boots cleanly with no game present; Connect() is user-triggered
        ConnectionResult Connect(const char* address, uint16_t port = 8080);
        void Disconnect();
        bool IsConnected() const;
        ConnectionInfo GetConnectionInfo() const;
        
        // Data subscription (editor ← game)
        void SubscribeToData(const StringCRC& dataType, DataCallback callback);
        void UnsubscribeFromData(const StringCRC& dataType);
        
        // Push updates (editor → game)
        void SendUpdate(const StringCRC& dataPath, const Json::Value& data);
        
    private:
        WebSocketClient* mWebSocket;
        DynamicArrayC<DataSubscription, 32> mSubscriptions;
        ConnectionInfo mConnectionInfo;
    };
}
```

**WebSocketClient:**
```cpp
namespace Dia::Editor {
    class WebSocketClient {
    public:
        enum class State { kDisconnected, kConnecting, kConnected, kError };
        
        ConnectionResult Connect(const char* address, uint16_t port);
        void Disconnect();
        State GetState() const;
        
        void Send(const char* message);
        void SetMessageCallback(MessageCallback callback);
        void SetErrorCallback(ErrorCallback callback);
        
        void Update();  // Poll for messages (called by GameConnectionManager)
    };
}
```

**DebugProtocol:**

Message types, serialization helpers, and data type constants are defined in the shared **DiaDebugProtocol** system (@docs/specs/systems/dia/diadebugprotocol.md). Both DiaEditor (client) and DiaDebugServer (server) include the same headers from `Dia::DebugProtocol::` namespace. GameConnectionManager uses these types directly — no editor-local protocol definitions.

### UI Bridge

WebUIBridge is the editor-level communication API. It sits on top of DiaUI's `IUISystem`, adding editor-specific routing: three message shapes (fire-and-forget events, request/response RPC, and topic pushes), dispatching events to the correct handler, and serializing data updates.

```
JS side:  window.dia.callCpp(name, argsJson)   ← DiaUI (transport, IPC to browser process)
              ↑
          window.CluicheEditor.{undo,redo,...}  ← EditorBridge.ts (routes via dia.callCpp)
              ↑
          React components                       ← plugin UI
```

**Message shapes (all carried over a single `DiaEditor_call` transport):**

| Shape | JS → C++ payload | Response path |
|-------|-------------------|---------------|
| Event (fire-and-forget) | `{ type, data }` | none |
| Request/response | `{ type, reqId, data }` | C++ calls `DiaEditor_onResponse({reqId, result})` on the main frame |
| Topic push (C++ → JS) | n/a | C++ calls `DiaEditor_onDataChanged({topic, data})` on the main frame |

The shell re-broadcasts every topic push to iframes via `postMessage({__dia:true, topic, data})` so dockable panels can subscribe regardless of which frame they live in.

**WebUIBridge:**
```cpp
namespace Dia::Editor {
    class WebUIBridge {
    public:
        using EventHandler = std::function<void(const Json::Value& data)>;
        using RequestHandler = std::function<Json::Value(const Json::Value& data)>;

        explicit WebUIBridge(Dia::UI::IUISystem* uiSystem);

        void Initialize(EditorViewController* controller);

        // JavaScript → C++
        void RegisterEventHandler(const StringCRC& eventType, EventHandler handler);
        void RegisterRequestHandler(const StringCRC& eventType, RequestHandler handler);

        // C++ → JavaScript (topic-based push). Topic is passed as a string so
        // the JS side can dispatch on it without needing a CRC lookup.
        void NotifyUIDataChanged(const char* topic, const Json::Value& data);

    private:
        // Single transport handler registered with IUISystem. Parses {type,
        // reqId?, data}, dispatches to request or event handler, and sends
        // the response envelope for reqId-bearing calls.
        std::string HandleEditorCall(const std::string& argsJson);
        void SendResponse(const std::string& reqId, const Json::Value& result);

        Dia::UI::IUISystem* mUISystem;
        EditorViewController* mController;
        // ... handler tables (DynamicArrayC<EventEntry, 32>, RequestEntry)
    };
}
```

**Built-in request/event handlers** registered by `EditorView::Initialize`:
- `get_panels` (request) — returns the registered dockable panel list
- `get_commands` (request) — returns the registered command list
- `load_layout` (request) — returns the serialized DockingLayout
- `save_layout` (event) — persists a layout tree sent from JS
- `execute_command` (event) — routes to EditorViewController and emits a console entry

### Docking Layout

**DockingLayout:**
```cpp
namespace Dia::Editor {
    class DockingLayout {
    public:
        // Load/save layout configuration
        void LoadLayout(const char* layoutPath);
        void SaveLayout(const char* layoutPath);
        
        // Serialization (for .diaapp manifest or user prefs)
        Json::Value Serialize() const;
        void Deserialize(const Json::Value& layout);
        
        // Component management
        void RegisterComponent(const char* componentName, const char* uiPath);
        void UnregisterComponent(const char* componentName);
        bool HasComponent(const char* componentName) const;
    };
}
```

## Dependencies

### Required Systems (from Dia)
- **DiaCore** - Containers (DynamicArrayC, HashTable), StringCRC, Observer pattern, Singleton, FilePath
- **DiaUI** - IUISystem interface (implemented by DiaUICEF; DiaEditor holds a pointer but does not depend on DiaUICEF directly)
- **DiaDebugProtocol** - Shared message types and serialization for editor-game communication (header-only)
- **DiaWebSocket** - WebSocket client transport (used by GameConnectionManager)

**Note:** DiaEditor has **no dependency on DiaApplication** (ProcessingUnit, Phase, Module). Application flow is the consumer's responsibility (CluicheEditor).

### External Dependencies
- **CEF (Chromium Embedded Framework)** - Web rendering engine (~100MB)
- **websocketpp** - WebSocket transport (via DiaWebSocket abstraction layer)
- **jsoncpp** - JSON parsing (already in External/)
- **react-mosaic** (JavaScript) - Docking layout library
- **React + TypeScript** (JavaScript) - UI component framework

## Non-Responsibilities

What DiaEditor explicitly does NOT handle:

- **System-specific editor logic** - That belongs in plugins (e.g., DiaApplicationEditor owns manifest editing)
- **Game runtime execution** - DiaEditor connects to games but doesn't run them
- **Asset authoring** - No texture painting, model creation (use external tools + asset pipeline)
- **Build system** - Use DiaAPI/DiaCLI for builds, not editor
- **Version control** - Editor saves files, but doesn't manage git/svn

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaDebugServer | Counterpart - runs in games to provide WebSocket server for editor connection | WebSocket protocol (JSON messages) |
| DiaApplicationEditor | Plugin - uses DiaEditor framework to edit .diaapp manifests | IEditorPlugin interface |
| CluicheEditor | Consumer - derives from EditorApplication to create concrete editor | EditorApplication base class |
| DiaDebugProtocol | Shared dependency - message types for editor-game protocol | `Dia::DebugProtocol::` types (header-only) |

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaEditor:

| Source | ID | Decision | Impact on DiaEditor |
|--------|----|----------|---------------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | Plugin type IDs, event types, data paths all use StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture for app structure | Applies to the consumer (CluicheEditor), not DiaEditor itself. DiaEditor is a pure library — no Module/Phase/ProcessingUnit subclasses. |
| Platform | PD-003 | Component-based entities (IComponent/IComponentObject) | Plugins use Component pattern where applicable (e.g., UI components) |
| Platform | PD-004 | No STL containers in public APIs | IEditorPlugin uses `const char*`, DynamicArrayC, not std::string, std::vector |
| Platform | PD-006 | Visual Studio project files are source of truth | DiaEditor built as .vcxproj in Dia solution |
| Dia | AD-001 | Module system with YAML frontmatter | DiaEditor documented as `dia.dia.diaeditor.architecture.module.md` |
| Dia | AD-002 | No STL in public APIs | Reinforces PD-004; public interfaces use Dia types |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | All classes in `Dia::Editor::` namespace |
| Dia | AD-004 | ProcessingUnit/Phase/Module for apps | Applies to CluicheEditor (the consumer), not DiaEditor. DiaEditor library classes are driven by the consumer's Modules/Phases. |
| Dia | AD-005 | Component-based entities | Reinforces PD-003; plugins use Component pattern where applicable |

## System-Specific Decisions

Decisions specific to DiaEditor system. Binding decisions constrain all features within DiaEditor.

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SED-001 | Plugin interface (`IEditorPlugin`) is minimal and stable | Minimize breaking changes; plugins are maintained by different teams | Proposed | Yes |
| SED-002 | Plugins register via macro, not manual registration | Follows ApplicationTypeRegistry pattern; zero-config auto-discovery | Proposed | Yes |
| SED-003 | Each editor plugin lives at `Dia/Dia<System>Editor/` (top-level peer, not a subdirectory of the system) | Standalone location; easier to build/link as independent DLL; avoids embedding a large plugin (UI assets, React components) inside the system it edits | Accepted | Yes |
| SED-004 | WebSocket protocol uses JSON (not binary) | Human-readable for debugging; flexible schema; standard tooling support | Proposed | Yes |
| SED-005 | CEF replaces Awesomium for web UI | Awesomium deprecated/abandoned; CEF actively maintained, modern Chromium | Proposed | Yes |
| SED-006 | Docking managed by react-mosaic (JavaScript), not C++ | Web-native React solution; serializable; modern library; reduces C++ complexity | Proposed | Yes |
| SED-007 | CommandDispatcher embeds Python (not HTTP bridge) | Lower latency; simpler than blue-console-server; reuses DiaPython | Proposed | Yes |
| SED-008 | EditorModel uses Observer pattern (not polling) | Efficient UI updates; follows DiaCore pattern; decoupled model/view | Proposed | Yes |
| SED-009 | Framework provides undo/redo via IEditorCommand + CommandHistory | Every plugin needs undo; retrofitting is painful; command pattern enables save-point tracking | Proposed | Yes |
| SED-010 | Use DiaDebugProtocol for all editor-game wire types | Single source of truth prevents protocol drift between DiaEditor and DiaDebugServer | Proposed | Yes |
| SED-011 | Two-tier observer paths: shared framework paths + plugin-scoped `"plugin.<id>.<path>"` | Cross-plugin coordination opt-in; isolation by default; void* data stays private | Proposed | Yes |
| SED-012 | Shared data path constants as typed StringCRC in `Dia::Editor::DataPath` | Same pattern as DDP-006; compile-time safety, no raw strings at call sites | Proposed | Yes |
| SED-013 | Plugin data structs declare `kPluginDataTypeId`; EditorModel asserts type on retrieval | Catches wrong-type casts in Debug; matches Dia `kUniqueId` pattern; no RTTI; zero cost in Release | Proposed | Yes |
| SED-014 | `.cluicheproj` is the top-level project file; references `.diaapp` manifests | Separates "what to work on" from "what tools to load"; extensible for future project-level concerns | Proposed | Yes |
| SED-015 | DiaEditor is a pure C++ library — no DiaApplication dependency (no Module/Phase/ProcessingUnit subclasses) | Independently testable without the application framework; reusable in future editor executables; consumer (CluicheEditor) owns all application flow via thin Module/Phase wrappers that call into DiaEditor library classes | Accepted | Yes |
| SED-016 | GameConnectionManager boots clean with no game; Connect() is explicit and user-triggered | Tool must be usable without a running game; non-blocking boot; avoids startup failure or hang when no game is present | Accepted | Yes |
| SED-017 | EditorManifestLoader is a static utility (not a class instance) that parses .diaapp and invokes a callback per plugin | Decouples manifest parsing from plugin lifecycle; consumer decides how to instantiate plugins; callback + void* avoids std::function in the public API | Accepted | Yes |
| SED-018 | Fullscreen is implemented by collapsing the Mosaic layout to a single leaf node; the Mosaic component stays mounted | Swapping render trees (separate div + iframe) destroys the iframe and loses all in-page state (console history, editor content). Keeping Mosaic alive preserves every panel's iframe across fullscreen and drag/move. Exiting fullscreen restores the saved layout tree. | Accepted | Yes |
| SED-019 | The CluicheEditor shell frame must register document-level `dragover`/`drop` listeners that call `preventDefault()` | Without this, files dropped on the shell (toolbar, window border, gaps between panels) trigger CEF's default navigate-frame behaviour, blanking the entire editor window | Accepted | Yes |
| SED-020 | Each editor plugin writes persistent output to `Cluiche/out/CluicheEditor/<PluginName>/` | Per PD-009 all generated output lives under `Cluiche/out/<AppName>/`; subdividing by plugin prevents collisions and makes per-plugin cleanup trivial; mirrors the per-system subdivision DiaCLI uses (`Cluiche/out/DiaCLI/logs/<system>/`) | Accepted | Yes |
| SED-021 | Per-plugin session context via `.context.json` sidecar and `.sessions/` archive | Plugins track session context without encoding it in directory paths. Current session metadata lives in `<PluginName>/.context.json`. On `Initialize()`, if `.context.json` references a stale session, the plugin moves its current data into `.sessions/<old-session-id>/` (with its `.context.json`) before starting fresh. Plugins decide what to archive; `.sessions/` entries mirror the plugin's own internal layout. Boot-time cleanup prunes old sessions per a plugin-defined retention policy. Extensible to personas and other context dimensions by adding fields to `.context.json` — no folder restructure needed. | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

Features within the DiaEditor system (create with `/spec-feature`):

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Plugin Discovery | Auto-registration of editor plugins via macro, runtime discovery | @docs/specs/features/dia/diaeditor/plugin-discovery.md | Approved |
| WebSocket Client | Connection to running games, message handling, reconnection logic | @docs/specs/features/dia/diaeditor/websocket-client.md | Approved |
| CEF Integration | Chromium embedding, multi-process architecture, custom URL schemes | @docs/specs/features/dia/diaeditor/cef-integration.md | Approved |
| Docking Layout | react-mosaic integration, persistence, full-screen vs dockable | @docs/specs/features/dia/diaeditor/docking-layout.md | Approved |
| Command Palette | Search and execute DiaCLI commands from UI | @docs/specs/features/dia/diaeditor/command-palette.md | Approved |
| Output Console | Display command output, game logs, error messages | @docs/specs/features/dia/diaeditor/output-console.md | Approved |
| Live Data Visualization | Subscribe to game state, render updates, read-only monitoring | @docs/specs/features/dia/diaeditor/live-data-visualization.md | Approved |
| Undo/Redo | IEditorCommand + CommandHistory module, save-point tracking, compound commands | @docs/specs/features/dia/diaeditor/undo-redo.md | Approved |
| Home Panel | Built-in landing panel so the docking area is never empty | @docs/specs/features/dia/diaeditor/home-panel.md | Approved |
| Game Connection Panel | UI for the editor's WebSocket connection to a running game | @docs/specs/features/dia/diaeditor/game-connection-panel.md | Draft |
| Plugin Lifecycle Toolbar | Persistent toolbar strip, Plugin Browser panel, runtime load/unload/hide/show of plugins, connection status indicator | @docs/specs/features/dia/diaeditor/plugin-lifecycle-toolbar.md | Done |
| Shared File Dialog | Framework-level native file dialog service (open/save) available to all plugins via WebUIBridge | @docs/specs/features/dia/diaeditor/shared-file-dialog.md | Draft |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should DiaUICEF be a separate system or part of DiaEditor? | Separate peer system that DiaEditor depends on. DiaUICEF implements IUISystem but is editor-only — not for game runtime UI (too heavy, ~100MB). Games use DiaUIUltralight or DiaUIAwesomium. |
| 2 | Plugin System | Should plugins be DLLs or statically linked? | DLLs for system editors (hot-reload, independence); static linking may be optimization later |
| 3 | Dependencies | Does DiaEditor depend on DiaDebugServer? | No - DiaDebugServer runs in games; DiaEditor just consumes its WebSocket protocol |
| 4 | WebSocket | Which WebSocket library to use? | DiaWebSocket wraps websocketpp (per DDS-009); DiaEditor uses DiaWebSocket::Client, not a raw library |
| 5 | Manifest | Where does editor config live? (.diaapp vs separate file) | Embedded in .diaapp "editor" section per CluicheEditor AED-002 |
| 6 | Live Connection | Should same-process connection mode be supported? | Not initially - WebSocket-first per CluicheEditor AED-004; can optimize later |
| 7 | UI Framework | React vs Vue for web UI? | React + TypeScript — all feature specs already assume React; larger ecosystem, better TS support |
| 8 | Threading | Should EditorApplication run multi-threaded? | Yes - inherits ProcessingUnit threading model; can run plugins on separate threads if needed |
| 9 | UI Bridge | How do WebUIBridge (DiaEditor) and CEFMessageBridge (DiaUICEF) relate? | WebUIBridge wraps CEFMessageBridge. CEFMessageBridge owns the IPC transport (`window.dia.sendMessage()`). WebUIBridge adds editor routing: event dispatch to plugins, Observer serialization, command handling. One channel, two abstraction levels. |
| 10 | Architecture | Is DiaUICEF part of DiaEditor or a separate system? | Separate peer system. DiaUICEF implements IUISystem but is editor-only (CEF too heavy for games). DiaEditor depends on it. Games use DiaUIUltralight or DiaUIAwesomium. |
| 11 | Plugin Communication | How do plugins coordinate without tight coupling? | Two-tier observer paths (SED-011). Shared framework paths for editor-wide state, plugin-scoped `"plugin.<id>.<path>"` for visible plugin state. void* data stays private. Isolation by default, coordination opt-in. |
| 12 | Plugin Data Safety | How to prevent wrong-type casts on void* plugin data? | Plugin data structs declare `static const StringCRC kPluginDataTypeId` (SED-013). EditorModel stores type ID alongside void*, debug asserts on mismatch. Matches Dia `kUniqueId` pattern, no RTTI, zero cost in Release. |
| 13 | Project File | What is the project file format? | `.cluicheproj` (JSON) — references `.diaapp` manifests, stores editor state, extensible for future needs. `LoadProject()` loads the project, then calls `LoadEditorManifest()` for each manifest. |

## Status

`Done` - System implemented and operational at `Dia/DiaEditor/`
