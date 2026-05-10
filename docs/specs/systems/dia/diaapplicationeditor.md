# System Spec: DiaApplicationEditor

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaApplicationEditor is a comprehensive editor system for both static .diaapp manifest editing and live runtime debugging of DiaApplicationFlow-based games. It implements the IEditorPlugin interface (from DiaEditor framework) and provides:

1. **Static Editing** - Load, visually edit, and save .diaapp manifest files with graph-based phase/module connection editing
2. **Live Debugging** - Connect to running games via WebSocket to visualize real-time phase transitions and runtime state
3. **Hot Reload Integration** - Trigger hot reload of modified manifests on connected games
4. **Runtime Configuration** - Change ProcessingUnit/Phase/Module configuration on live games and see immediate effects

**Location:** `Dia/DiaApplicationEditor/` - separate system at Dia level, not under DiaApplicationFlow.

## Responsibilities

### Static Editing (File-Based)
- **Manifest Editing** - Load, edit, and save .diaapp manifest files
- **Visual Graph Editor** - Display and edit phase/module connections as interactive graph
- **Module Configuration** - Inline editing of module config JSON with validation
- **Validation** - Real-time validation using ApplicationManifestValidator
- **Type Discovery** - Query ApplicationTypeRegistry for available module/phase/PU types
- **Manifest Composition** - Support import statements and manifest merging

### Live Debugging (Runtime Connection)
- **Game Connection** - Connect to running game via DiaEditor's GameConnectionManager
- **Live Phase Visualization** - Display current phase, visualize transitions as they happen
- **Runtime State Inspection** - View current ProcessingUnit/Phase/Module state
- **Hot Reload Triggering** - Send hot reload command to connected game
- **Runtime Config Changes** - Push configuration changes to live game, see immediate effects
- **Performance Monitoring** - Display FPS, frame time, phase timing

### Integration
- **Reference Implementation** - Demonstrate best practices for building editor plugins that combine static + live functionality

## Public Interfaces

### Plugin Implementation

**DiaApplicationEditor (implements IEditorPlugin):**
```cpp
namespace Dia::Application::Editor {
    class DiaApplicationEditor : public Dia::Editor::IEditorPlugin {
    public:
        DiaApplicationEditor();
        virtual ~DiaApplicationEditor();
        
        // IEditorPlugin interface
        const char* GetName() const override { return "DiaApplicationEditor"; }
        const char* GetVersion() const override { return "1.0.0"; }
        const char* GetDescription() const override { 
            return "Visual editor for .diaapp manifest files"; 
        }
        const char* GetUIPath() const override;  // Returns path to React components
        Dia::Editor::LayoutMode GetLayoutMode() const override { 
            return Dia::Editor::LayoutMode::kDockable; 
        }
        
        void OnLoad(Dia::Editor::EditorModel* model) override;
        void OnUnload() override;
        void OnUpdate(float deltaTime) override;
        void* GetPluginData() override;  // Returns ManifestEditorData*
        
        // Static editing operations
        void OpenManifest(const char* path);
        void SaveManifest(const char* path);
        void SaveManifestAs(const char* path);
        void CloseManifest();
        
        void ValidateManifest();
        const ValidationResult& GetValidationResult() const;
        
        bool IsDirty() const;
        void MarkClean();
        
        // Live debugging operations
        void ConnectToGame(const char* address, uint16_t port);
        void DisconnectFromGame();
        bool IsConnectedToGame() const;
        
        void TriggerHotReload();
        void PushConfigChange(const StringCRC& puId, const StringCRC& moduleId, const Json::Value& config);
        
        const RuntimeState& GetLiveRuntimeState() const;  // Current phase, module states, etc.
        
    private:
        Dia::Editor::EditorModel* mEditorModel;
        ManifestEditorData* mPluginData;
    };
}

// Auto-registration (in .cpp file):
// REGISTER_EDITOR_PLUGIN(DiaApplicationEditor, "DiaApplicationEditor")
```

### Plugin Data

**ManifestEditorData:**
```cpp
namespace Dia::Application::Editor {
    struct ManifestEditorData {
        // Static editing state
        ApplicationManifest* manifest;
        const char* filePath;
        bool isDirty;
        
        // Validation state
        ValidationResult validationResult;
        bool autoValidate;
        
        // Type discovery (for dropdowns)
        DynamicArrayC<StringCRC, 64> availableModuleTypes;
        DynamicArrayC<StringCRC, 32> availablePhaseTypes;
        DynamicArrayC<StringCRC, 16> availableProcessingUnitTypes;
        
        // Selection state (for UI highlighting)
        StringCRC selectedProcessingUnit;
        StringCRC selectedPhase;
        StringCRC selectedModule;
        
        // Live debugging state
        bool isConnectedToGame;
        RuntimeState liveState;  // Current game state from WebSocket
        ConnectionInfo connectionInfo;
    };
    
    struct RuntimeState {
        // Current execution state
        StringCRC currentProcessingUnit;
        StringCRC currentPhase;
        DynamicArrayC<StringCRC, 32> activeModules;
        
        // Performance metrics
        float fps;
        float frameTimeMs;
        float phaseTimeMs;
        
        // Phase transition history (for visualization)
        DynamicArrayC<PhaseTransition, 64> recentTransitions;
    };
    
    struct PhaseTransition {
        StringCRC fromPhase;
        StringCRC toPhase;
        uint64_t timestamp;
    };
    
    struct ValidationResult {
        bool isValid;
        DynamicArrayC<ValidationError, 32> errors;
        DynamicArrayC<ValidationWarning, 32> warnings;
    };
    
    struct ValidationError {
        const char* message;
        const char* path;  // JSON path (e.g., "processing_units[0].modules[2]")
        int lineNumber;
    };
}
```

### UI Components (React/TypeScript)

**Web UI Structure:**
```
Dia/DiaApplicationEditor/UI/
├── ManifestEditor.tsx           # Root component, handles load/save, view toggling
├── GraphViews/
│   ├── FlowView.tsx             # State machine diagram (phases + transitions)
│   └── TreeView.tsx             # Hierarchical tree (PU → Phases → Modules)
├── ModuleInspector.tsx          # Detail panel for module config (debounced save)
├── ValidationPanel.tsx          # Display errors/warnings inline
├── TypeSelector.tsx             # Dropdown for selecting types from registry
├── LiveDebugger/
│   ├── ConnectionPanel.tsx      # WebSocket connection controls
│   ├── PhaseTransitionVisualizer.tsx # Real-time phase transition animation
│   ├── RuntimeStateInspector.tsx    # Live module/phase state display
│   ├── PerformanceMonitor.tsx   # FPS, frame time, phase timing graphs
│   └── HotReloadButton.tsx      # Trigger hot reload with confirmation
└── ConflictResolver.tsx         # File conflict detection/resolution UI
```

**Key React Components:**
```typescript
// ManifestEditor.tsx - Root component with view toggling
interface ManifestEditorProps {
    manifestPath: string;
    onSave: (path: string) => void;
    onValidate: () => void;
}

export const ManifestEditor: React.FC<ManifestEditorProps> = (props) => {
    const [viewMode, setViewMode] = useState<'flow' | 'tree'>('flow');
    const [isDirty, setIsDirty] = useState(false);
    // Toggle between FlowView and TreeView
};

// FlowView.tsx - State machine diagram
interface FlowViewProps {
    processingUnits: ProcessingUnit[];
    onAddTransition: (fromPhase: string, toPhase: string) => void;
    onRemoveTransition: (fromPhase: string, toPhase: string) => void;
    onSelectPhase: (phaseId: string) => void;
}
// Uses react-flow library for interactive graph

// TreeView.tsx - Hierarchical tree
interface TreeViewProps {
    processingUnits: ProcessingUnit[];
    onAddPhase: (puId: string, phaseType: string) => void;
    onAddModule: (phaseId: string, moduleType: string) => void;
    onRemovePhase: (phaseId: string) => void;
    onRemoveModule: (moduleId: string) => void;
}
// Tree for adding/organizing, NOT for editing transitions

// ModuleInspector.tsx - Config editing with debounce
interface ModuleInspectorProps {
    module: ModuleConfig;
    onConfigChange: (config: Json) => void;  // Debounced 500ms
    isConnectedToGame: boolean;  // Show "Apply to Game" if true
}

// LiveDebugger/PhaseTransitionVisualizer.tsx
interface PhaseTransitionVisualizerProps {
    currentPhase: string;
    transitions: PhaseTransition[];  // Recent history
    onTransition: (from: string, to: string) => void;  // Animate
}
```

### C++ ↔ JavaScript Bridge

**JavaScript → C++ (user actions):**
```javascript
// User clicks "Save" in UI (manual save, not auto)
window.DiaEditor.executeCommand("diaapp-editor", "save", { path: "/path/to/file.diaapp" });

// User modifies module config (debounced 500ms before sending)
const debouncedConfigChange = debounce((config) => {
    window.DiaEditor.notifyDataChanged("module_config", {
        processing_unit: "MainProcessingUnit",
        module: "RenderModule",
        config: config
    });
}, 500);

// User clicks "Hot Reload" in live debugger
window.DiaEditor.executeCommand("diaapp-editor", "hot_reload", {
    manifest_path: "C:/GitHub/Cluiche/example.diaapp"
});

// User adds phase transition in FlowView
window.DiaEditor.notifyDataChanged("phase_transition_added", {
    from_phase: "InitPhase",
    to_phase: "UpdatePhase"
});
```

**C++ → JavaScript (data updates):**
```cpp
// Notify UI that validation completed
mEditorModel->NotifyObservers(StringCRC("validation_result"));
// WebUIBridge serializes ValidationResult to JSON and forwards to JavaScript

// Notify UI of live phase transition from WebSocket
mEditorModel->NotifyObservers(StringCRC("live_phase_transition"));
// WebUIBridge forwards: { from: "UpdatePhase", to: "RenderPhase", timestamp: 12345 }

// Notify UI of file conflict detected
mEditorModel->NotifyObservers(StringCRC("file_conflict"));
// WebUIBridge forwards: { localChanges: [...], diskChanges: [...], diff: "..." }
```

## Workflows

### Static Editing Workflow

**1. Open and Edit Manifest:**
```
User: File → Open → select example.diaapp
Editor: Load via ApplicationManifestLoader
Editor: Query ApplicationTypeRegistry for available types
Editor: Display in Tree View (default)

User: Click "Flow View" toggle
Editor: Render state machine diagram with phases and transitions

User: Drag from InitPhase to UpdatePhase (add transition)
Editor: Mark dirty (example.diaapp *)
Editor: Validate via ApplicationManifestValidator
Editor: Show validation errors inline if any

User: Click phase in Flow View → opens ModuleInspector
User: Edit module config JSON
Editor: Debounce 500ms → mark dirty → validate

User: Ctrl+S (save)
Editor: Write to disk
Editor: Mark clean (remove *)
```

**2. File Conflict Resolution:**
```
User: Edits manifest in editor (not saved yet)
External: Another tool modifies same file on disk
User: Clicks Save
Editor: Detects file modified externally
Editor: Shows ConflictResolver UI with diff
User: Chooses [Keep My Changes] or [Use Disk Version] or [Show Diff]
Editor: Applies choice, saves
```

### Live Debugging Workflow

**3. Connect to Running Game:**
```
User: Click "Connect to Game" in LiveDebugger panel
User: Enter address (localhost:8080)
Editor: GameConnectionManager.ConnectRemote("localhost", 8080)
Editor: WebSocket handshake with DiaDebugServer
Game: Sends initial state (current PU/Phase/Module)
Editor: Display in RuntimeStateInspector
Editor: Subscribe to "processing_unit_state" and "phase_transition" events

Game: Phase transition occurs (Update → Render)
Game: Broadcasts event via DiaDebugServer
Editor: Receives WebSocket message
Editor: Animates transition in PhaseTransitionVisualizer
Editor: Updates RuntimeStateInspector
```

**4. Hot Reload Workflow:**
```
User: Edits manifest in editor
User: Saves file (Ctrl+S)
User: Clicks "Hot Reload" button in LiveDebugger
Editor: Shows confirmation dialog
User: Confirms
Editor: Sends WebSocket command:
  { "type": "command", "command": "hot_reload", 
    "payload": { "manifest_path": "C:/path/to/example.diaapp" } }
    
Game: Receives command
Game: HotReloadManager loads from disk
Game: Validates manifest
Game: Applies changes (adds/removes modules, etc.)

Success path:
  Game: Sends { "type": "command_response", "success": true }
  Editor: Shows "Hot reload successful" notification
  Editor: Reloads file from disk (auto-refresh)
  Editor: Updates UI to match new state

Error path:
  Game: Sends { "type": "command_response", "success": false, 
                "error_code": "VALIDATION_ERROR", 
                "message": "Module 'PhysicsModule' not found" }
  Editor: Shows error notification with details
  Editor: Highlights problematic line in manifest
  Editor: Does NOT reload file (keeps showing broken manifest)
```

**5. Runtime Config Change Workflow:**
```
User: Connected to game
User: Clicks RenderModule in RuntimeStateInspector
Editor: Shows module config in ModuleInspector
User: Changes resolution from 1920x1080 to 3840x2160
Editor: Debounces 500ms
Editor: Shows "pending" indicator
Editor: Sends WebSocket command:
  { "type": "command", "command": "update_config",
    "payload": { "pu_id": "MainProcessingUnit", 
                 "module_id": "RenderModule",
                 "config": { "resolution": { "width": 3840, "height": 2160 } } } }

Game: Receives command
Game: Checks if RenderModule.CanHotUpdateConfig() == true
Game: Calls RenderModule.UpdateConfig(config)
Game: Module resizes window immediately
Game: Broadcasts updated state to all editors

Editor: Receives updated state
Editor: Updates RuntimeStateInspector
Editor: Marks manifest dirty (example.diaapp *)
User: Saves later if they want to keep the change
```

**6. Risky Change Warning Workflow:**
```
User: Connected to game, viewing Tree View
User: Right-click UpdatePhase → "Add Module" → selects PhysicsModule
Editor: Detects risky change (adding module at runtime)
Editor: Shows warning dialog:
  "⚠️ Adding a module at runtime is risky and may crash the game.
   This requires restarting the ProcessingUnit.
   
   [Apply Anyway] [Cancel]"
   
User: Clicks [Apply Anyway]
Editor: Sends update_config command
Game: Attempts to add module (may succeed or fail)
```

## Dependencies

### Required Systems (from Dia)
- **DiaEditor** - IEditorPlugin interface, EditorModel, plugin registry, WebUIBridge
- **DiaApplicationFlow** - ApplicationManifest, ApplicationManifestLoader, ApplicationManifestValidator, ApplicationTypeRegistry
- **DiaCore** - Containers, StringCRC, JSON parsing (jsoncpp wrapper)

### UI Dependencies
- **React** - UI component framework (chosen over Vue for ecosystem and TypeScript support)
- **TypeScript** - Type-safe JavaScript
- **react-flow** - Interactive graph library for FlowView state machine diagram
- **react-mosaic** - Docking (provided by DiaEditor framework)
- **Monaco Editor** (optional) - Code editor for raw JSON editing

## Non-Responsibilities

What DiaApplicationEditor explicitly does NOT handle:

- **Runtime execution** - Doesn't run the application, just edits config
- **Asset pipelines** - Use DiaAPI commands for asset processing
- **Version control** - Saves files but doesn't manage git
- **Manifest generation** - Use DiaCLI scaffolding commands for templates
- **Other file types** - Only edits .diaapp; use separate plugins for other configs

## Related Systems

| System | Relationship | Interface |
|--------|--------------|-----------|
| DiaEditor | Framework - provides plugin infrastructure | IEditorPlugin interface |
| DiaApplicationFlow | Parent system - provides manifest schema and validation | ApplicationManifest, ApplicationManifestValidator |
| CluicheEditor | Host application - loads and runs this plugin | Plugin discovery via EditorPluginRegistry |

## Inherited Binding Decisions

These decisions from parent platform and application specs are binding constraints on DiaApplicationEditor:

| Source | ID | Decision | Impact on DiaApplicationEditor |
|--------|----|----------|-------------------------------|
| Platform | PD-001 | C++ as primary language | Plugin core in C++; UI in TypeScript via CEF bridge |
| Platform | PD-002 | Windows as primary platform | Test on Windows; React UI cross-platform by nature |
| Platform | PD-003 | Visual Studio + MSBuild | Built as part of DiaApplicationFlow .vcxproj |
| Platform | PD-004 | Spec-driven development | This spec approved before implementation |
| Dia | AD-001 | Module system with YAML frontmatter | Document as `dia.diaapplication.editor.architecture.module.md` |
| Dia | AD-002 | No STL in public APIs | IEditorPlugin methods use `const char*`, not std::string |
| Dia | AD-003 | Namespace convention: `Dia::<Module>::` | Classes in `Dia::Application::Editor::` namespace |
| DiaEditor | SED-001 | Plugin interface minimal and stable | Implement only IEditorPlugin; avoid custom extensions |
| DiaEditor | SED-002 | Plugins register via macro | Use `REGISTER_EDITOR_PLUGIN(DiaApplicationEditor, "DiaApplicationEditor")` |
| DiaEditor | SED-003 | Each editor plugin lives at `Dia/Dia<System>Editor/` | Lives in `Dia/DiaApplicationEditor/` ✅ |
| DiaEditor | SED-008 | EditorModel uses Observer pattern | Subscribe to model changes; don't poll |

## System-Specific Decisions

Decisions specific to DiaApplicationEditor. Binding decisions constrain all features within this plugin.

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| DAED-001 | Reuse ApplicationManifestValidator, don't reimplement | Single source of truth for validation rules; consistency with loader | Proposed | Yes |
| DAED-002 | Two view modes: Flow (state machine) and Tree (hierarchy) | Flow shows transitions, Tree shows organization; toggle between them | Proposed | Yes |
| DAED-003 | Phase transitions only editable in Flow View | Clear separation: Tree adds/organizes, Flow edits transitions | Proposed | Yes |
| DAED-004 | Query ApplicationTypeRegistry for available types | Dropdowns show only registered types; prevents typos; auto-discovery | Proposed | Yes |
| DAED-005 | Display validation errors inline (at source location) | Better UX than error list; user sees problem in context | Proposed | No |
| DAED-006 | Runtime config changes debounced (500ms) | Balance responsiveness and network traffic; visual pending indicator | Proposed | Yes |
| DAED-007 | Hybrid hot-update: modules opt-in, else reload | Flexible; modules choose if they support hot config changes | Proposed | Yes |
| DAED-008 | Manual save workflow: mark dirty, user saves (Ctrl+S) | Standard editor workflow; prevents accidental overwrites | Proposed | Yes |
| DAED-009 | Detect file conflicts, show diff, let user resolve | Prevents silent data loss; educates user about external changes | Proposed | Yes |
| DAED-010 | Warn on risky runtime changes (add module, change transition) | Educate user; allow proceed with confirmation | Proposed | Yes |
| DAED-011 | Hot reload sends path only, game loads from disk | Simpler protocol; guarantees game sees saved file | Proposed | Yes |
| DAED-012 | Document-level `dragover`/`drop` prevention required in the plugin iframe | CEF executes its default behaviour (navigate the frame to the dropped file URL) when no `preventDefault()` is called, turning the panel white; must block at `document` level not just in the drop zone element | Accepted | Yes |
| DAED-013 | All inter-process (editor ↔ game) communication must use Protobuf | Type-safe, versioned wire format; consistent with DiaDebugProtocol; no ad-hoc JSON over the WebSocket connection | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

Features within the DiaApplicationEditor system (create with `/spec-feature`):

### Static Editing Features
| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Manifest Load/Save | Open .diaapp files, parse, save changes with .bak backup | @docs/specs/features/dia/diaapplicationeditor/manifest-load-save.md | Approved |
| Flow View | State machine diagram showing phases and transitions (react-flow) | @docs/specs/features/dia/diaapplicationeditor/flow-view.md | Approved |
| Tree View | Hierarchical tree for adding PUs, phases, modules (react-arborist) | @docs/specs/features/dia/diaapplicationeditor/tree-view.md | Approved |
| View Toggle | Switch between Flow and Tree views with shared state (Zustand) | @docs/specs/features/dia/diaapplicationeditor/view-toggle.md | Approved |
| Phase Transition Editor | Add/remove transitions by dragging in Flow View | @docs/specs/features/dia/diaapplicationeditor/phase-transition-editor.md | Approved |
| Module Config Editor | Hybrid form + CodeMirror 6 JSON editing with debounced save (500ms) | @docs/specs/features/dia/diaapplicationeditor/module-config-editor.md | Approved |
| Type Discovery | Hybrid: query game registry + static fallback for available types | @docs/specs/features/dia/diaapplicationeditor/type-discovery.md | Approved |
| Real-time Validation | Debounced 500ms validation with inline annotations + error panel | @docs/specs/features/dia/diaapplicationeditor/real-time-validation.md | Approved |
| File Conflict Detection | Filesystem watcher + non-modal banner for external changes | @docs/specs/features/dia/diaapplicationeditor/file-conflict-detection.md | Approved |
| Undo/Redo | Ctrl+Z/Y with 8-entry snapshot history, resets on view switch | @docs/specs/features/dia/diaapplicationeditor/undo-redo.md | Approved |
| PU/Phase Inspector | Reuses inspector panel for PU and Phase property editing (frequency, initial_phase, etc.) | @docs/specs/features/dia/diaapplicationeditor/pu-phase-inspector.md | Approved |
| Editable Lifecycle Grid | Click cells to toggle module↔phase associations; instant apply | @docs/specs/features/dia/diaapplicationeditor/editable-lifecycle-grid.md | Approved |
| Import Management | Visualize, edit inline, add/remove manifest imports with provenance badges | @docs/specs/features/dia/diaapplicationeditor/import-management.md | Approved |
| Validation Navigation | Click validation error → select and scroll to offending node | @docs/specs/features/dia/diaapplicationeditor/validation-navigation.md | Approved |
| New Manifest | Ctrl+N creates blank manifest (1 PU, 1 Phase); Save triggers Save As | @docs/specs/features/dia/diaapplicationeditor/new-manifest.md | Approved |

### Live Debugging Features
| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| WebSocket Connection | Auto-connect to DiaDebugServer via shared GameConnectionManager | @docs/specs/features/dia/diaapplicationeditor/websocket-connection.md | Approved |
| Live Phase Visualization | Animate phase + edge transitions in Flow View with history | @docs/specs/features/dia/diaapplicationeditor/live-phase-visualization.md | Approved |
| Runtime State Inspector | Dual pane (Flow + Inspector panel) showing live game state | @docs/specs/features/dia/diaapplicationeditor/runtime-state-inspector.md | Approved |
| Hot Reload Trigger | No confirmation, immediate reload command to game | @docs/specs/features/dia/diaapplicationeditor/hot-reload-trigger.md | Approved |
| Runtime Config Push | Config values only (debounced 500ms), structural changes need reload | @docs/specs/features/dia/diaapplicationeditor/runtime-config-push.md | Approved |
| Risky Change Warnings | Warning on hot reload only, show change summary | @docs/specs/features/dia/diaapplicationeditor/risky-change-warnings.md | Approved |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should DiaApplicationEditor be a DLL or statically linked? | DLL for hot-reload and independence from CluicheEditor build |
| 2 | UI | Flow View vs Tree View - why both? | Flow shows phase transitions (state machine), Tree shows hierarchy (add/organize). Toggle between them (DAED-002) |
| 3 | Validation | When to validate? | Auto-validate on change; also on save. Display errors inline at source location |
| 4 | Live Debugging | How does editor connect to game? | Via DiaEditor's GameConnectionManager → DiaDebugServer (WebSocket) → game broadcasts state |
| 5 | Hot Reload | What does editor send? | Just manifest path (DAED-011); game loads from disk via HotReloadManager |
| 6 | Runtime Config | When are changes sent to game? | Debounced 500ms after user stops typing (DAED-006) |
| 7 | File Conflicts | How to handle external file modifications? | Detect via file watcher, show diff in ConflictResolver UI, let user choose (DAED-009) |
| 8 | Risky Changes | Can user add modules at runtime? | Yes with warning (DAED-010) - educate user about risks, require confirmation |
| 9 | Protocol | What WebSocket library? | websocketpp (header-only, easy integration) for Phase 5 |
| 10 | Graph Library | What library for Flow View? | react-flow (React graph library for interactive state machine diagram) |

## Status

`Superseded` — Superseded by [diaapplicationeditor-v2.md](diaapplicationeditor-v2.md) (2026-05-08). Redesigned for PU/Stage/Module v2 model.

**Plan:** @docs/specs/systems/dia/diaapplicationeditor.plan.md

## Notes

**Design Validation:** DiaApplicationEditor serves as the reference implementation proving the DiaEditor plugin architecture works. If this plugin feels awkward to implement, the framework (IEditorPlugin, EditorModel, WebUIBridge) needs refinement before proceeding to other plugins.

**Implementation Order:** Build DiaApplicationEditor in Phase 4 alongside CluicheEditor host application. This validates the full stack: framework → host → plugin.
