**Spec:** @docs/specs/features/cluicheeditor/diaeditorapi/app-editor-actions.md
**Status:** In Progress

## Implementation Patterns

### Controller pattern (mirrors ProjectContextController)

```cpp
// Dia/DiaEditor/AppEditor/AppEditorController.h
class AppEditorController {
public:
    AppEditorController();
    void Initialize(WebUIBridge* bridge, DiaEditorAPI* api, IEditorContext* context);
    void Shutdown();

    // Called by plugins to update tracked state (main thread only)
    void SetFocus(Dia::Core::StringCRC pluginId, const char* panel);
    void ClearFocus();
    void SetEditTarget(Dia::Core::StringCRC type, Dia::Core::StringCRC id,
                       const char* name, bool dirty);
    void SetDirty(bool dirty);
    void SetSelection(Dia::Core::StringCRC type, Dia::Core::StringCRC id,
                      const char* name);
    void ClearEditTarget();
    void ClearSelection();

private:
    Json::Value HandleGetActiveContext(const Json::Value& params);
    Json::Value HandleNavigateTo(Dia::Core::StringCRC type,
                                 Dia::Core::StringCRC id);

    WebUIBridge*    mBridge  = nullptr;
    DiaEditorAPI*   mApi     = nullptr;
    IEditorContext* mContext = nullptr;  // for project state

    // Tracked state — fixed char buffers own the string data (callers may pass temporaries)
    static const unsigned int kMaxNameLength = 128;

    Dia::Core::StringCRC mFocusPluginId;
    char                 mFocusPanel[kMaxNameLength] = {};

    Dia::Core::StringCRC mEditTargetType;
    Dia::Core::StringCRC mEditTargetId;
    char                 mEditTargetName[kMaxNameLength] = {};
    bool                 mEditTargetDirty = false;

    Dia::Core::StringCRC mSelectionType;
    Dia::Core::StringCRC mSelectionId;
    char                 mSelectionName[kMaxNameLength] = {};
};
```

**String lifetime rule:** All `const char*` params passed into `Set*()` are copied into fixed buffers via `strncpy_s`. Callers are free to pass stack strings or temporaries.

### Action registration — thin wrappers, one navigate implementation

```cpp
void AppEditorController::Initialize(WebUIBridge* bridge, DiaEditorAPI* api, IEditorContext* context) {
    mBridge  = bridge;
    mApi     = api;
    mContext = context;

    // All actions kMainThread — state is written on main thread, reading from another
    // thread without synchronization would be a data race. One frame latency is fine.
    api->RegisterAction({ StringCRC("app_editor.get_active_context"), ..., kMainThread,
        [this](const Json::Value& p) { return HandleGetActiveContext(p); } });

    api->RegisterAction({ StringCRC("app_editor.navigate_to_plugin"), ..., kMainThread,
        [this](const Json::Value& p) {
            return HandleNavigateTo(StringCRC("plugin"), StringCRC(p["plugin_id"].asCString()));
        }});
    // ... navigate_to_asset, navigate_to_stage, navigate_to_entity same pattern
}
```

### Navigate implementation — resolves type → validates → pushes to JS

```cpp
Json::Value AppEditorController::HandleNavigateTo(StringCRC type, StringCRC id) {
    if (!mContext || !mContext->HasOpenProject())
        return ErrorJson("no_project_open");
    if (!mBridge)
        return ErrorJson("no_project_open");
    if (id == StringCRC::kEmpty)
        return ErrorJson("not_found");

    // Type-specific resolution — validate target exists in C++ before pushing to JS
    if (type == StringCRC("plugin")) {
        // Query plugin loader for known plugin
        if (!mPluginLoader || !mPluginLoader->IsPluginLoaded(id))
            return ErrorJson("plugin_not_loaded");
    }
    else if (type == StringCRC("stage")) {
        // Validate stage exists in current manifest (via IEditorContext)
        if (!mContext->HasStage(id))
            return ErrorJson("not_found");
    }
    else if (type == StringCRC("entity")) {
        // Validate entity template exists (via asset catalogue or context)
        if (!mContext->HasEntityTemplate(id))
            return ErrorJson("not_found");
    }
    else if (type == StringCRC("asset")) {
        // Phase 1: no plugin→asset-type registry yet
        return ErrorJson("not_implemented");
    }

    // Resolution passed — push to JS for UI navigation
    Json::Value payload;
    payload["type"] = type.GetString();
    payload["id"]   = id.GetString();
    mBridge->NotifyUIDataChanged("app_editor.navigate_to", payload);

    Json::Value result;
    result["success"] = true;
    return result;
}
```

**Resolution responsibilities:** C++ validates that the target *exists* (no blind fire to JS). JS handles the actual panel focus/scroll. `navigate_to_asset` returns `not_implemented` in Phase 1 per AC3.

### Single code path rule — how to migrate an existing push caller

```cpp
// Before (DiaApplicationFlowEditorPlugin.cpp):
GetBridge()->NotifyUIDataChanged("app_editor.navigate_to_stage", payload);

// After:
Json::Value params;
params["stage_id"] = stageId.GetString();
mAppEditorController->HandleNavigateTo(StringCRC("stage"), stageId);
// OR via DiaEditorAPI if controller reference isn't available:
mApi->ExecuteAction(StringCRC("app_editor.navigate_to_stage"), params);
```

### How plugins report context state

```cpp
// In a plugin's OnLoad() or on relevant events:
mAppEditorController->SetFocus(StringCRC("scene_editor"), "scene_hierarchy");
mAppEditorController->SetEditTarget(StringCRC("scene"), StringCRC("scene_test"),
                                    "SceneTest", false);
mAppEditorController->SetSelection(StringCRC("entity"), StringCRC("entity_001"),
                                   "PlayerSpawn");
```

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `AppEditorController` — header + cpp in `Dia/DiaEditor/AppEditor/`. Fixed char buffers for name fields (kMaxNameLength=128, `strncpy_s` in setters). Implement `SetFocus`, `ClearFocus`, `SetEditTarget`, `SetDirty`, `SetSelection`, `ClearEditTarget`, `ClearSelection`. Implement `HandleGetActiveContext` — serialize tracked state to JSON matching data model in spec; `project` field read from `mContext->GetDiagameProject()`. Add to `DiaEditor.vcxproj`. | Unit: `get_active_context` returns correct JSON after each Set call; null fields when not set; dirty flag toggled by `SetDirty`; ClearFocus resets focus to null; long strings (>128) truncated without crash | Done | sonnet | 11/11 tests pass. Fixed char buffers + strncpy_s. HandleGetActiveContext serializes all 4 context fields correctly. |
| 2 | Implement `HandleNavigateTo(type, id)` — validate target exists in C++ before pushing to JS. `plugin` → check `IPluginLoader::IsPluginLoaded(id)`. `stage` → check `IEditorContext::HasStage(id)`. `entity` → check `IEditorContext::HasEntityTemplate(id)`. `asset` → return `not_implemented` (Phase 1). Push `app_editor.navigate_to` to bridge only after resolution passes. Return `not_found`/`plugin_not_loaded`/`no_project_open`/`not_implemented` as appropriate. Add `DIA_LOG_INFO` on navigation, `DIA_LOG_WARN` on failure. | Unit: known plugin_id → success + bridge push fired; unknown plugin_id → `plugin_not_loaded`; empty id → `not_found`; nonexistent stage_id → `not_found`; asset type → `not_implemented`; null context → `no_project_open` | Done | sonnet | 16/16 tests pass. Stage/entity deferred to JS validation (HasStage/HasEntityTemplate not on IEditorContext yet). DIA_LOG_WARNING channel "AppEditor" used. |
| 3 | Implement `Initialize(bridge, api, context)` — register all 5 actions via `api->RegisterAction()`. All 5 → `kMainThread` (avoids data race on state fields written by plugins on main thread). Each navigate wrapper extracts the relevant id param and calls `HandleNavigateTo`. Store `mPluginLoader` from context. Implement `Shutdown()` — call `api->DeregisterActionsForOwner(StringCRC("AppEditorController"))`. | `GetManifest()` lists all 5 `app_editor.*` actions after Initialize; Shutdown removes them; duplicate Initialize returns false + logs warn | Blocked | sonnet | BLOCKED: requires `DiaEditorAPI::RegisterAction()` — DiaEditorAPI Phase 1 tasks 1–3 must be Done first. |
| 4 | Wire `AppEditorController` into `EditorViewControllerModule` — add as member, call `Initialize(bridge, api, context)` in `DoStart()`, `Shutdown()` in `DoStop()`. Pass bridge + DiaEditorAPI + IEditorContext refs. Also register `AppEditorController*` on `PluginServiceLocator` so plugins can access it. | Editor starts without crash; `GetManifest()` includes `app_editor.*` after startup; `GetServices()->GetAppEditorController()` returns non-null; Shutdown cleans up without assert | Blocked | sonnet | BLOCKED: requires Task 3 (DiaEditorAPI integration) and DiaEditorAPI Phase 1 to exist. |
| 5 | Migrate existing push caller — `DiaApplicationFlowEditorPlugin.cpp:1015` calls `GetBridge()->NotifyUIDataChanged("app_editor.navigate_to_stage", payload)` directly. Replace with `GetServices()->GetAppEditorController()->HandleNavigateTo(StringCRC("stage"), stageId)`. Grep for any other direct `app_editor.*` push calls and migrate them. | AC8: `grep -r "NotifyUIDataChanged.*app_editor" Dia/ Cluiche/` returns zero hits outside `AppEditorController.cpp`; existing AppFlow editor navigation still works end-to-end | Blocked | sonnet | BLOCKED: requires Task 4 (PluginServiceLocator wiring) so plugins can call GetServices()->GetAppEditorController(). |
| 6 | Wire 3 plugins to report context state — `DiaApplicationFlowEditorPlugin` calls `SetEditTarget` on manifest open, `SetFocus` on panel activation; `DiaSceneEditorPlugin` calls `SetEditTarget` on scene open + `SetSelection` on entity select; `DiaEntityTemplateEditorPlugin` calls `SetEditTarget` on template load. All access via `GetServices()->GetAppEditorController()`. | Manual: open a scene → `app_editor.get_active_context` returns correct `edit_target`; select entity → `selection` updates; switch plugin focus → `focus` updates | Blocked | sonnet | BLOCKED: requires Task 4 (PluginServiceLocator wiring). |
| 7 | Python smoke test — call `dia_editor.app_editor.get_active_context()` against live editor, assert returns dict with `project`, `focus`, `edit_target`, `selection` keys. Call `dia_editor.app_editor.navigate_to_plugin("app_flow_editor")` — assert `{ "success": true }`. Call with unknown plugin_id — assert `{ "success": false, "reason": "plugin_not_loaded" }`. Call `dia_editor.app_editor.navigate_to_asset("x")` — assert `{ "success": false, "reason": "not_implemented" }`. | All 4 assertions pass; no `DIA_LOG_ERROR` in output | Blocked | haiku | BLOCKED: requires DiaEditorAPI Phase 1 tasks 7–8 (Python adapter + stub generation). |

## Validation Checkpoints

| After task | Check |
|------------|-------|
| 1 | `dia run googletest --filter="AppEditorController*"` — all unit tests green |
| 3 | `GetManifest()` lists all 5 `app_editor.*` actions; Shutdown removes them |
| 5 | `grep -r "NotifyUIDataChanged.*app_editor" Dia/ Cluiche/` → zero hits outside AppEditorController.cpp |
| 7 | Smoke script passes; `DIA_LOG_ERROR` count = 0 |
