# Feature Spec: app-editor-actions

**Parent:** @docs/specs/systems/cluicheeditor/diaeditorapi.md
**Status:** Approved

## Summary

Introduces the `app_editor.*` action namespace — a small set of new request handlers owned by `AppEditorController` that give scripts, automation tests, and the DiaChatPlugin the ability to query the editor's current context and drive navigation. All C++ internal navigation is rerouted through `DiaEditorAPI::ExecuteAction()` so there is a single code path regardless of caller.

## Problem

Scripts and the chat agent have no way to know what the editor is currently showing, what is selected, or what is being edited. Navigation is only possible from inside the JS layer (via the existing `app_editor.navigate_to_stage` push). There is no inbound handler, so Python and AI callers are blind and cannot drive the editor.

## Goals

1. Expose current editor context (project, active plugin, edit target, selection) as a queryable action.
2. Provide inbound navigation actions for plugin, asset, stage, and entity targets.
3. Establish a single code path: all navigation — whether triggered by JS, Python, MCP, or C++ internally — routes through `DiaEditorAPI::ExecuteAction()`. The outgoing WebUIBridge push becomes an implementation detail inside the handler.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `app_editor.get_active_context` returns a JSON object with `project`, `focus`, `edit_target`, and `selection` fields (see Data Model) |
| AC2 | `app_editor.navigate_to_plugin(plugin_id)` focuses the named panel; returns `{ "success": true }` or `{ "success": false, "reason": "..." }` |
| AC3 | `app_editor.navigate_to_asset(asset_id)` opens the asset in its owning plugin editor; returns success/failure JSON. **Phase 1 limitation:** returns `{ "success": false, "reason": "not_implemented" }` until a plugin→asset-type registration API exists (see ODQ-2). |
| AC4 | `app_editor.navigate_to_stage(stage_id)` routes to the stage in the AppFlow editor; returns success/failure JSON |
| AC5 | `app_editor.navigate_to_entity(entity_id)` opens the entity in the Entity Template editor; returns success/failure JSON |
| AC6 | All 5 actions are registered in DiaEditorAPI and appear in `GetManifest()` output |
| AC7 | All 5 actions are callable from Python via `dia_editor.app_editor.*` |
| AC8 | C++ code that previously called `WebUIBridge.Push("app_editor.navigate_to_stage", ...)` directly now calls `DiaEditorAPI::ExecuteAction("app_editor.navigate_to_stage", ...)` instead. No direct push calls remain for `app_editor.*` topics. |
| AC9 | `app_editor.navigate_to_*` actions all share a single `AppEditorController::HandleNavigateTo(type, id)` implementation — no duplicated navigation logic |
| AC10 | Unknown `plugin_id`, `asset_id`, `stage_id`, or `entity_id` returns `{ "success": false, "reason": "not_found" }` — no crash |

## Data Model

### `app_editor.get_active_context` response

```json
{
  "project": {
    "id": "cluichetest",
    "state": "open"
  },
  "focus": {
    "plugin_id": "scene_editor",
    "panel": "scene_hierarchy"
  },
  "edit_target": {
    "type": "scene",
    "id": "scene_test",
    "name": "SceneTest",
    "dirty": false
  },
  "selection": {
    "type": "entity",
    "id": "entity_001",
    "name": "PlayerSpawn"
  }
}
```

- `project` — currently open project (null if none open)
- `focus` — which plugin panel is currently active/focused (null if none; cleared via `ClearFocus()` on project close or all-plugins-unloaded)
- `edit_target` — what is currently open for mutation (null if none); `dirty` indicates unsaved changes
- `selection` — what is highlighted within the edit target (null if nothing selected)

### Navigation action params

```json
{ "plugin_id": "scene_editor" }
{ "asset_id": "mesh_player_001" }
{ "stage_id": "rigid_body_2d" }
{ "entity_id": "entity_player_template" }
```

### Navigation action response

```json
{ "success": true }
{ "success": false, "reason": "not_found" }
{ "success": false, "reason": "no_project_open" }
{ "success": false, "reason": "plugin_not_loaded" }
```

## Architecture

### AppEditorController

A new controller class (`Dia/DiaEditor/AppEditor/AppEditorController`) owns:
- The `app_editor.*` action registrations
- Tracking of `focus`, `edit_target`, and `selection` state (updated by plugins via `SetFocus()`, `SetEditTarget()`, `SetSelection()`)
- `HandleNavigateTo(StringCRC type, StringCRC id)` — single implementation behind all 4 navigate actions
- The outgoing WebUIBridge push (`app_editor.context_changed`) that notifies JS when context changes

```cpp
class AppEditorController {
public:
    void Initialize(WebUIBridge* bridge, DiaEditorAPI* api, IEditorContext* context);
    void Shutdown();

    // Called by plugins to update context state
    void SetFocus(StringCRC pluginId, const char* panel);
    void ClearFocus();
    void SetEditTarget(StringCRC type, StringCRC id, const char* name, bool dirty);
    void SetDirty(bool dirty);
    void SetSelection(StringCRC type, StringCRC id, const char* name);
    void ClearEditTarget();
    void ClearSelection();

private:
    Json::Value HandleGetActiveContext(const Json::Value& params);
    Json::Value HandleNavigateTo(StringCRC type, StringCRC id);  // shared impl

    // State fields use fixed char buffers — callers pass transient const char*
    static const unsigned int kMaxNameLength = 128;
    char mFocusPanel[kMaxNameLength];
    char mEditTargetName[kMaxNameLength];
    char mSelectionName[kMaxNameLength];
};
```

### Plugin access to AppEditorController

Plugins get a pointer to `AppEditorController` via `PluginServiceLocator` — the existing service bag already passed to every plugin at load time. This avoids modifying `EditorPluginContext` (which is a POD struct visible to all plugins). `PluginServiceLocator` is the right home for optional cross-cutting services.

```cpp
// In PluginServiceLocator:
AppEditorController* GetAppEditorController() const;

// In plugin code:
GetServices()->GetAppEditorController()->SetFocus(...);
```

### Single code path rule

Any C++ code that needs to trigger navigation calls `DiaEditorAPI::ExecuteAction()` — not `WebUIBridge::PushEvent()` directly. The WebUIBridge push is the **output** of the handler, not the trigger. This applies to all `app_editor.*` topics.

```
Before:  C++ → WebUIBridge.Push("app_editor.navigate_to_stage", payload) → JS
After:   C++ → DiaEditorAPI::ExecuteAction("app_editor.navigate_to_stage", params)
                  → AppEditorController::HandleNavigateTo("stage", id)
                    → resolve target plugin
                    → WebUIBridge.Push("app_editor.navigate_to_stage", payload) → JS
                    → return { "success": true }
```

## Action Manifest

Descriptions and param schemas are the source of truth for what gets written into `EditorActionDescriptor::description` and `EditorActionParamSchema`. They flow directly into MCP `tools/list` responses, `.pyi` docstrings, and any generated documentation. Write them here; copy verbatim into code.

---

### `app_editor.get_active_context`

```
description:
  Returns the full editor context snapshot: which project is open, which plugin panel
  is currently focused, what asset or scene is open for editing (the edit target), and
  what is selected within it. Call this first on every chat or script session to orient
  yourself before issuing any navigation or mutation commands. Returns null for fields
  that are not currently set (e.g. no project open, nothing selected).

category:  app_editor
owner:     AppEditorController
dispatch:  kMainThread
params:    none
returns:   { project, focus, edit_target, selection } — see Data Model
```

---

### `app_editor.navigate_to_plugin`

```
description:
  Focuses and brings to front the named plugin panel in the editor. Use this to direct
  the user's attention to a specific tool — for example, before explaining how to use
  the Scene Editor, navigate to it first. The plugin_id is the instance ID declared in
  the .diaapp manifest (e.g. "scene_editor", "app_flow_editor", "entity_template_editor").
  Returns plugin_not_loaded if the plugin is not currently active.

category:  app_editor
owner:     AppEditorController
dispatch:  kMainThread
params:
  plugin_id  string  required  "Instance ID of the plugin panel to focus, e.g. 'scene_editor'.
                                 Use get_active_context to see what is currently focused."
returns:   { success: true } | { success: false, reason: "plugin_not_loaded" | "no_project_open" }
```

---

### `app_editor.navigate_to_asset`

```
description:
  Opens an asset in its owning editor plugin and focuses that panel. Use this when the
  user references a specific asset by ID and you want to show it to them in the editor.
  NOTE: Phase 1 limitation — returns not_implemented until the plugin asset-type
  registration API is available. Record the asset_id from get_active_context or from
  asset_catalogue queries.

category:  app_editor
owner:     AppEditorController
dispatch:  kMainThread
params:
  asset_id  string  required  "The unique asset ID from the asset catalogue,
                                e.g. 'mesh_player_001'. Obtain from asset_catalogue.get_record
                                or asset_catalogue.query_by_type."
returns:   { success: true } | { success: false, reason: "not_found" | "not_implemented" | "no_project_open" }
```

---

### `app_editor.navigate_to_stage`

```
description:
  Navigates the Application Flow Editor to the named processing stage and highlights it.
  Use this when explaining or modifying a specific stage — navigate first so the user
  can see exactly what you are referring to. The stage_id matches the stage name declared
  in the .diaapp manifest (e.g. "rigid_body_2d", "boot", "running"). Returns not_found
  if no stage with that ID exists in the currently open manifest.

category:  app_editor
owner:     AppEditorController
dispatch:  kMainThread
params:
  stage_id  string  required  "The stage name from the .diaapp manifest,
                                e.g. 'rigid_body_2d'. Obtain from manifest.getState
                                or app_editor.get_active_context."
returns:   { success: true } | { success: false, reason: "not_found" | "no_project_open" }
```

---

### `app_editor.navigate_to_entity`

```
description:
  Opens the named entity template in the Entity Template Editor and focuses that panel.
  Use this when the user asks about a specific entity type or when you are about to
  explain or modify a template's components. The entity_id is the template's unique ID
  in the asset catalogue (e.g. "entity_player_template"). Returns not_found if no
  matching template exists in the current project.

category:  app_editor
owner:     AppEditorController
dispatch:  kMainThread
params:
  entity_id  string  required  "The entity template ID from the asset catalogue,
                                 e.g. 'entity_player_template'. Obtain from
                                 entity_template_editor.get_list."
returns:   { success: true } | { success: false, reason: "not_found" | "no_project_open" }
```

---

### Action registration (thin wrappers, one implementation)

```cpp
void AppEditorController::RegisterActions(DiaEditorAPI& api) {
    api.RegisterAction({ StringCRC("app_editor.get_active_context"), ...,
        [this](const Json::Value& p) { return HandleGetActiveContext(p); } });

    api.RegisterAction({ StringCRC("app_editor.navigate_to_plugin"), ...,
        [this](const Json::Value& p) { return HandleNavigateTo(StringCRC("plugin"), StringCRC(p["plugin_id"].asCString())); } });

    api.RegisterAction({ StringCRC("app_editor.navigate_to_asset"), ...,
        [this](const Json::Value& p) { return HandleNavigateTo(StringCRC("asset"), StringCRC(p["asset_id"].asCString())); } });

    api.RegisterAction({ StringCRC("app_editor.navigate_to_stage"), ...,
        [this](const Json::Value& p) { return HandleNavigateTo(StringCRC("stage"), StringCRC(p["stage_id"].asCString())); } });

    api.RegisterAction({ StringCRC("app_editor.navigate_to_entity"), ...,
        [this](const Json::Value& p) { return HandleNavigateTo(StringCRC("entity"), StringCRC(p["entity_id"].asCString())); } });
}
```

## Tasks

| # | Task | Test |
|---|------|------|
| 1 | Create `AppEditorController` — state fields, `SetFocus/SetEditTarget/SetSelection/Clear*`, `HandleGetActiveContext`, `HandleNavigateTo`. Add to DiaEditor.vcxproj. | Unit: get_active_context returns correct JSON after SetFocus + SetEditTarget + SetSelection; null fields when not set |
| 2 | Register all 5 actions in DiaEditorAPI via `AppEditorController::RegisterActions()`. Call from editor startup after DiaEditorAPI is initialised. | `GetManifest()` lists all 5 `app_editor.*` actions |
| 3 | Implement `HandleNavigateTo` routing — resolve type → owning plugin → push WebUIBridge event. Return `not_found` / `plugin_not_loaded` / `no_project_open` as appropriate. | Unit: known stage_id → success + push fired; unknown id → not_found; no project → no_project_open |
| 4 | Migrate all existing C++ callers of `WebUIBridge.Push("app_editor.navigate_to_stage", ...)` to `DiaEditorAPI::ExecuteAction(...)`. Grep for direct `app_editor.*` push calls and remove them. | AC8: zero direct `app_editor.*` push calls remain in C++ outside AppEditorController |
| 5 | Wire plugins to call `AppEditorController::SetFocus/SetEditTarget/SetSelection` on their focus/open/select events so `get_active_context` reflects live state. | Manual: open a scene in scene editor → `app_editor.get_active_context` returns correct edit_target |
| 6 | Python smoke test: `dia_editor.app_editor.get_active_context()`, `dia_editor.app_editor.navigate_to_plugin("scene_editor")` — assert returns expected shape. | Smoke test passes; no DIA_LOG_ERROR in output |

## Modules Touched

| Module | Change |
|--------|--------|
| `Dia/DiaEditor` | New `AppEditorController` class |
| `Dia/DiaEditor` | Existing C++ navigation callers migrated off direct push |
| `Cluiche/CluicheEditor` | Wire `AppEditorController` into editor startup |
| `Dia/DiaApplicationFlowEditor` | Call `SetEditTarget` on manifest open; `SetFocus` on panel focus |
| `Dia/DiaSceneEditor` | Call `SetEditTarget` on scene open; `SetSelection` on entity select |
| `Dia/DiaEntityTemplateEditor` | Call `SetEditTarget` on template load; `SetSelection` on selection |

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform | PD-001 | StringCRC for all IDs | Action names, plugin IDs, type discriminators, and navigation targets all use StringCRC constants |
| Platform | PD-002 | PU/Phase/Module architecture | `AppEditorController` is owned by `EditorViewControllerModule`; `RegisterActions()` called in `DoStart()` |
| Platform | PD-004 | No STL in public APIs | `AppEditorController` public methods use `const char*` and `StringCRC`; Json::Value only in handler return types (internal) |
| CluicheEditor | AED-001 | DiaEditor is a pure library; CluicheEditor owns application flow | `AppEditorController` lives in DiaEditor (pure library); wiring into `EditorViewControllerModule` lives in CluicheEditor |
| DiaEditorAPI | EAPI-004 | Dispatch policy declared per action | All 5 actions → `kMainThread`. `get_active_context` reads state written by plugins on the main thread; running it on kCallerThread would be a data race. One frame of latency is acceptable for a context query. |
| DiaEditorAPI | EAPI-005 | CEF-entangled actions excluded | `navigate_to_*` actions drive the JS layer but are safe headless (return `not_found` gracefully if no UI) |

## Open Design Questions

| # | Question | Resolution |
|---|----------|------------|
| ODQ-1 | Should `app_editor.context_changed` be a push notification fired whenever `SetFocus/SetEditTarget/SetSelection` is called, so the chat UI can subscribe and stay in sync without polling? Or is polling `get_active_context` sufficient? | Unresolved — polling is sufficient for Phase 1; subscribe push can be added later if chat needs reactive updates |
| ODQ-2 | When `navigate_to_asset` is called, how does `AppEditorController` know which plugin handles which asset type? Does it query `DiaAssetCatalogueEditorPlugin` for the type, then look up the owning plugin by type? Or is there a static plugin→asset-type registration table? | Unresolved — needs a plugin asset-type registration API (plugins declare what asset types they handle at load time). Out of scope for this feature; `navigate_to_asset` can return `not_implemented` in Phase 1 and be completed when the registration API exists. |
| ODQ-3 | `edit_target.dirty` requires plugins to notify `AppEditorController` when unsaved state changes. Is it safe to have plugins call `SetEditTarget(...)` on every mutation, or should there be a lighter `SetDirty(bool)` call? | Unresolved — `SetDirty(bool)` is the lighter and safer option; avoids re-resolving type/id/name on every keystroke |
