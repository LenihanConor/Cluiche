# Feature Spec: editor-deep-link

**Parent:** @docs/specs/applications/dia/systems/diaeditor/diaeditor.md
**Status:** Approved

## Summary

Opening an asset from the catalogue opens the target editor but drops the user at the default view; they have to manually find the specific asset. This feature adds `OnNavigate(instanceId)` to `IEditorPlugin` and wires it into `IPluginLoader::LoadPlugin` so that every cross-editor link delivers the user directly to the relevant asset — regardless of whether the target plugin is already open or freshly loaded.

## Goals

- Deep-link from any catalogue double-click to the specific asset in the target editor
- Mechanism is generic — any future editor plugin gets it for free by overriding `OnNavigate`
- If the target plugin is already loaded, navigate without reload; if freshly loaded, navigate after `OnLoad`
- Default implementation is a no-op so existing plugins are unaffected until they opt in

## Acceptance Criteria

### Framework (`IEditorPlugin` + `IPluginLoader`)

- `IEditorPlugin` gains a new virtual method with a default no-op:
  ```cpp
  virtual void OnNavigate(const Dia::Core::StringCRC& instanceId) {}
  ```
- `IPluginLoader::LoadPlugin` contract updated: after loading (or if already loaded), the framework calls `OnNavigate(instanceId)` on the plugin — always, unconditionally
- `instanceId` is the asset CRC passed by the caller (already threaded through from `open_asset`)

### DiaApplicationEditor — stage deep link

- `OnNavigate` receives the `stage.*` asset CRC
- Looks up the matching stage in the loaded manifest by CRC-matching against stage IDs
- Pushes `app_editor.navigate_to_stage` notification via `WebUIBridge` with `{ "stageId": "<id>" }`
- UI scrolls the stage list and highlights the matching stage row
- If no manifest is loaded, the notification is a no-op (no error)

### DiaEntityTemplateEditor — blueprint file deep link

- `OnNavigate` receives the `diaentitytemplate`/`diacamera`/`dialight` asset CRC
- Looks up the source path from the catalogue registry via `InvokeRequestHandler("asset_catalogue.get_record", { id })`
- If found, calls the existing `entity_template_editor.load` handler internally
- The blueprint file is loaded and the property panel populated, same as if the user had clicked it in the list

### DiaSceneEditor — scene file deep link

- `OnNavigate` receives the `diascene` asset CRC
- Looks up the source path via `asset_catalogue.get_record`
- If found, loads the scene directly (same as `scene_editor.load_scene`)
- Stage selector updates to show the matching stage if applicable

### DiaAssetCatalogueEditor — record highlight deep link

- `OnNavigate` receives any asset CRC
- Pushes `asset_catalogue.navigate_to_record` with `{ "id": "<id>" }` via `WebUIBridge`
- UI scrolls the asset list to the matching record and selects it

### All editors — already-open case

- If the plugin is already loaded when `LoadPlugin` is called, `OnNavigate` is called immediately — `OnLoad` is NOT called again
- The plugin's current state (dirty edits, open file) is preserved; only the selection/focus changes

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaEditor/Plugin/IEditorPlugin.h` | Add `virtual void OnNavigate(const Dia::Core::StringCRC& instanceId) {}` |
| `Dia/DiaEditor/Plugin/IPluginLoader.h` | Document that `LoadPlugin` always calls `OnNavigate` after load-or-find |
| `Dia/DiaEditor/Plugin/PluginLoader.cpp` (or equivalent framework impl) | Call `OnNavigate(instanceId)` after `OnLoad` on fresh load, and directly on already-loaded plugin |
| `Dia/DiaApplicationEditor/DiaApplicationFlowEditorPlugin.cpp` | Override `OnNavigate` — push `app_editor.navigate_to_stage` |
| `Dia/DiaApplicationEditor/UI/index.html` | Handle `app_editor.navigate_to_stage` — scroll + highlight stage row |
| `Dia/DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.cpp` | Override `OnNavigate` — look up source path, load blueprint |
| `Dia/DiaSceneEditor/DiaSceneEditorPlugin.cpp` | Override `OnNavigate` — look up source path, load scene |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.cpp` | Override `OnNavigate` — push `asset_catalogue.navigate_to_record` |
| `Dia/DiaAssetCatalogueEditor/UI/index.html` | Handle `asset_catalogue.navigate_to_record` — scroll + select row |

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| SED-001 | `IEditorPlugin` is minimal and stable | `OnNavigate` has a default no-op — zero breaking changes to existing plugins. Compliant. |
| PD-001 | StringCRC for all IDs | `instanceId` is `StringCRC`. Navigation payloads carry the string ID alongside the CRC for display. Compliant. |
| PD-004 | No STL in public APIs | `OnNavigate` takes `const StringCRC&`. No STL at the interface boundary. Compliant. |
| SED-015 | DiaEditor is a pure library | `OnNavigate` is a pure virtual method addition — no Module/Phase dependency. Compliant. |

## Open Design Questions

1. **PluginLoader implementation location** — `IPluginLoader` is an interface; the concrete impl lives in CluicheEditor. Does the `OnNavigate` call-after-load live in the concrete `PluginLoader` in CluicheEditor, or should there be a helper in the DiaEditor framework? The concrete impl is the right place — keeps the framework as a pure interface layer.

2. **CRC → string ID round-trip** — `instanceId` is a CRC of the asset ID string (e.g. `StringCRC("stage.my_stage")`). DiaEntityTemplateEditor and DiaSceneEditor need the source path, which requires looking up the record. If the catalogue plugin is not loaded, the lookup fails silently. Acceptable for v1 — document as a known limitation.
