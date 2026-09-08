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

## DiaEditorAPI Action Catalog

All actions are called via `ExecuteAction(action_id, params)` from Python or `editor.executeAction()` from JS. Actions are dispatched on either `kMainThread` (mutating) or `kCallerThread` (read-only queries).

### project (3 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `project.open_path` | Open .diagame project | `path` (string, required) |
| `project.close` | Close current project | — |
| `project.get_state` | Return project state (name, diagamePath, source) | — |

### game_connection (3 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `game_connection.connect` | WebSocket connect to running game | `url` (string, required) |
| `game_connection.disconnect` | Disconnect from game | — |
| `game_connection.get_state` | Return connection state | — |

### app_editor (5 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `app_editor.get_active_context` | Cross-cutting editor state (project, focus, selection) | — |
| `app_editor.navigate_to_plugin` | Navigate to plugin panel | `plugin_id` (string) |
| `app_editor.navigate_to_stage` | Navigate to stage | `stage_id` (string) |
| `app_editor.navigate_to_entity` | Navigate to entity template | `entity_id` (string) |
| `app_editor.navigate_to_asset` | Navigate to asset | — |

### manifest (4 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `manifest.load` | Load .diaapp manifest | `path` (string, required) |
| `manifest.save` | Save manifest to disk | — |
| `manifest.getState` | Full manifest state (stages, PUs, streams, initialStage) | — |
| `manifest.applyCommand` | Execute manifest edit (25+ command types) | `commandType` (string, required) |

### history (3 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `history.undo` | Undo last manifest command | — |
| `history.redo` | Redo last undone command | — |
| `history.getState` | Undo/redo state (canUndo, canRedo, count) | — |

### validation (1 action)
| Action | Description | Params |
|--------|-------------|--------|
| `validation.run` | Validate loaded manifest | — |

### types (2 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `types.get` | Available module and PU types | — |
| `types.refresh` | Reload types from schema | `path` (string, optional) |

### scene_editor (40 actions)

**Read-only (kCallerThread):**
| Action | Description | Key Params |
|--------|-------------|------------|
| `get_project_state` | Project validity | — |
| `get_stage_list` | Stages from manifest | — |
| `get_hierarchy` | Entity hierarchy from path | — |
| `get_hierarchy_filtered` | Filtered hierarchy | `filter` (string) |
| `get_properties` | Full property set for item | `selectionType`, `selectionId` |
| `get_entity_template_defaults` | Defaults for template | `entityTemplateId`, `itemType` |
| `get_available_entity_templates` | Template list | `itemType` (optional) |
| `get_dirty_state` | Dirty flag | — |
| `get_entities` | Flat entity list | `type` (optional: entity/camera/light) |
| `get_scene_properties` | Scene-level properties | — |
| `validate` | Validate scene | — |

**Mutating (kMainThread):**
| Action | Description | Key Params |
|--------|-------------|------------|
| `load_stage_scene` | Load scene for stage | stage ID |
| `load_scene` / `save_scene` | Scene I/O | path |
| `add_item` / `duplicate_item` / `delete_item` | Entity CRUD | item ID |
| `rename_item` / `set_enabled` | Entity properties | item ID, value |
| `place_entity` / `remove_entity` | Position-aware add/remove | entity ID, position |
| `add_layer` / `delete_layer` / `reorder_layer` / `update_layer` | Layer management | layer params |
| `set_camera_active` | Camera control | camera ID |
| `add_override` / `remove_override` / `update_override` | Component overrides | entity, component, field |
| `change_entity_template` | Swap template | entity ID, new template |
| `create_scene` / `create_asset` | Asset creation | path |

### asset_catalogue (34 actions)

**Read-only (kCallerThread):**
| Action | Description | Key Params |
|--------|-------------|------------|
| `get_state` | Full catalogue (path, dirty, records) | — |
| `get_record` | Record for asset | `id` (string) |
| `get_forward_refs` / `get_reverse_refs` | Relationship queries | asset ID |
| `query_by_type` / `query_by_tag` | Asset queries | type/tag |
| `discover_files` | Find files by type | — |
| `validate` | Validate manifest | — |
| `get_available` | Is catalogue loaded? | — |

**Mutating (kMainThread):**
| Action | Description | Key Params |
|--------|-------------|------------|
| `load_manifest` / `save_manifest` / `new_manifest` | Manifest I/O | path |
| `create_record` / `update_record` / `delete_record` | Record CRUD | record fields |
| `bulk_create_records` | Batch creation | records array |
| `add_relationship` / `remove_relationship` | References | source, target, type |
| `infer_relationships` | Auto-discover refs | — |
| `create_scene` / `create_asset` | Asset creation | path |
| `open_in_editor` / `open_in_file` | Navigation | asset ID |

### entity_template_editor (9 actions)
| Action | Description | Key Params |
|--------|-------------|------------|
| `get_project_state` | Project validity | — |
| `get_list` | All templates in catalogue | — |
| `get_available_components` | Addable components | `path` (string) |
| `get_usage` | Reverse references | `assetId` (string) |
| `load` / `save` | Blueprint I/O | `path`, `blueprint` |
| `update_field` | Modify component field | `path`, `componentType`, `fieldName`, `value` |
| `add_component` / `remove_component` | Component management | `path`, `componentType` |

### plugin_browser (2 actions)
| Action | Description | Params |
|--------|-------------|--------|
| `plugin_browser.load` | Load plugin by type | `typeId` (string) |
| `plugin_browser.unload` | Unload plugin by type | `typeId` (string) |
