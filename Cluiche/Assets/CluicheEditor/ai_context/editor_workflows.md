# CluicheEditor Workflows

## Editor Overview

**CluicheEditor** is a standalone application (not a game mode) for creating and managing Cluiche games. It consists of:
- Main window with a menu bar and status panel
- Dockable panels (AI Assistant, Asset Browser, Inspector, Scene View, etc.)
- WebUI-based plugin system for extensibility
- `DiaEditorAPI` registry for programmatic actions (script/plugin execution)

## Plugin Panel System

Editor extensions are **plugins** that provide UI panels:

- **Panel layout modes:**
  - `Dockable` — can be dragged to dock areas (left, right, bottom)
  - `Floating` — independent window
  - `Modal` — blocks interaction until closed

- **Panel registration:**
  - Implement `IEditorPlugin` interface
  - Call `REGISTER_EDITOR_PLUGIN` macro
  - Plugins discovered at editor startup

- **UI delivery:**
  - Plugin provides path to HTML/CSS/JS via `GetUIPath()`
  - Editor loads UI in `dia://` iframe (sandboxed origin)
  - Return layout mode from `GetLayoutMode()` (dockable / floating)

## WebUIBridge: JavaScript ↔ C++

Plugins communicate with editor C++ code via the **WebUIBridge**:

### JS → C++: Call C++ Functions
```javascript
editor.sendMessage({
  action: "create_entity",
  data: { name: "Player", x: 0, y: 0 }
});
```
C++ handler registered via `RegisterEventHandler("create_entity", callback)`.

### C++ → JS: Update UI
```cpp
// C++ code
NotifyUIDataChanged("entity_list", json_data);
```
JS receives in `window.addEventListener("entity_list_changed", handler)`.

## DiaEditorAPI Action Registry

The editor maintains a registry of **actions** — callable operations that can be triggered from scripts or the AI assistant:

- **Built-in actions:** `history.undo`, `history.redo`, `file.open`, `file.save`, `scene.add_entity`, `inspect.entity`, etc.
- **Dynamic:** New actions registered by plugins via manifest or direct registration
- **Execution:** `ExecuteAction(action_id, parameters)` from Python or C++

Access via `editor.executeAction()` in JS or the Python orchestrator in Python code.

## Common Editor Workflows

### Adding a New Entity to the Scene
1. Open **Scene View** panel (dockable)
2. Right-click in viewport → **Create Entity**
3. Drag entity to position (or use Inspector to set position)
4. Add components via **Inspector** → **Add Component** dropdown
5. Inspector shows all attached components; edit properties inline
6. Changes auto-save to stage manifest

### Validating Manifests
1. Open asset root in file browser (e.g., `Cluiche/Assets/CluicheTest/`)
2. Right-click on `.diagame` or `.diastage` file → **Validate Manifest**
3. Editor runs `dia validate manifest` and shows errors/warnings
4. Fix YAML errors or missing required fields
5. Re-save manifest and validate again

### Building and Running from Editor
1. **Menu → Build** (or Ctrl+B) — runs `dia run cluichetest` in background
2. Output goes to **Build Output** panel (shows compile errors, warnings)
3. After build succeeds, **Menu → Run** launches the game in preview window
4. Pause/resume/step through phases in debugger if attached

### Viewing and Filtering Logs
1. Open **Log Viewer** panel
2. Filter by module (e.g., `DiaCore`, `DiaGraphics`)
3. Filter by severity: Error, Warning, Info, Verbose
4. Search by text
5. Logs sourced from `dia_observation.log` written by running game/editor

## dia:// URL Scheme

Plugin UIs are loaded via custom URL scheme:

```
dia://plugin-name/path/to/panel.html
```

- `plugin-name` — registered plugin ID
- `/path/to/panel.html` — relative path in plugin's UI directory
- Loaded in iframe with `https://cluiche-internal` origin (sandboxed)
- No access to local filesystem directly; must use WebUIBridge events

## Registering a New Plugin

### 1. Create Plugin Class
```cpp
class MyPlugin : public IEditorPlugin {
  void OnLoad() override;
  void OnUnload() override;
  void OnUpdate(float dt) override;
  const char* GetUIPath() override { return "ui/my_panel.html"; }
  ELayoutMode GetLayoutMode() override { return ELayoutMode::Dockable; }
};

REGISTER_EDITOR_PLUGIN(MyPlugin, "my_plugin");
```

### 2. Add UI Files
Create `Cluiche/Assets/CluicheEditor/plugins/my_plugin/ui/my_panel.html` and related CSS/JS.

### 3. Register Actions
In `OnLoad()`, call:
```cpp
EditorActionRegistry::Register("my_action", [](const JsonValue& params) {
  // Handle action
});
```

### 4. Build and Deploy
```bash
dia pipeline --target cluicheeditor
```
This compiles C++, builds plugin UIs, and deploys to the editor's plugin directory.

## Keyboard Shortcuts

- **Ctrl+B** — Build
- **Ctrl+R** — Run (after build)
- **Ctrl+Z** — Undo
- **Ctrl+Y** — Redo (or Ctrl+Shift+Z)
- **Ctrl+S** — Save
- **F5** — Step through phase in debugger
- **Tab** — Toggle between panels / cycle focus
