# System Spec: DiaSceneEditor

**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Parent Application
@docs/specs/applications/dia.md

## Summary

DiaSceneEditor is a CluicheEditor plugin for authoring `.diascene` files — the spatial layout of game levels. It handles entity/camera/light placements, instance_data overrides, and layer configuration. Blueprints are shown as read-only reference; actual blueprint editing is routed to DiaEntityTemplateEditor.

DiaSceneEditor is an offline tool — it reads and writes `.diascene` files directly. It does not connect to a running game (that is DiaEntityInspector's role). When opened, it derives available stages/scenes from the connected `.diagame` manifest.

DiaSceneEditor answers: "how do I author the spatial content of my game levels?"

## Responsibilities

**Owns:**
- Scene hierarchy panel — collapsible sections for Layers, Cameras, Lights, and Entities; search/filter across all sections
- Property inspector — context-sensitive detail panel for the selected scene item (instance overrides, identity, layer-specific properties)
- Entity placement authoring — add/duplicate/delete entity instances in a scene; assign blueprint (picker from asset catalogue); edit `instance_data` overrides
- Camera placement authoring — add/configure cameras with blueprint + instance overrides; enforce single-active-camera constraint
- Light placement authoring — add/configure lights with blueprint + instance overrides + `affects_layers` assignment
- Layer authoring — add/edit/reorder layers (sort_order, parallax, sort_policy, enabled)
- Template reference — "Template" link on each entity/camera/light routes directly to DiaEntityTemplateEditor; no separate Blueprint Defaults tab
- Change Template operation — reassign an entity instance to a different template with transfer/orphan analysis of existing overrides
- Clone (Duplicate) — copy any scene item with offset position and `_copy` suffix
- Scene file I/O — load `.diascene` from the stage referenced in the `.diagame`; autosave on every mutation (no explicit Save button)
- Dirty state tracking — visual indicator when unsaved changes exist
- Stage/Scene selector — toolbar dropdowns populated from the connected `.diagame` manifest (not a file picker)
- Scene validation — enforce constraints: exactly 1 active camera, default layer present, no duplicate IDs

**Does NOT own:**
- Blueprint authoring (component CRUD, field defaults) — that is DiaEntityTemplateEditor
- 2D spatial viewport — deferred; v1 is list + property panel only (future feature spec)
- Live game connection — this is an offline authoring tool; runtime inspection is DiaEntityInspector
- Scene loading/hydration at runtime — that is DiaScene2D's SceneLoader2D
- Asset creation — new blueprints are created in DiaAssetCatalogueEditor, which routes to DiaEntityTemplateEditor
- Asset pipeline integration — DiaSceneEditor saves files; the pipeline processes them separately
- Undo/redo system — deferred to v2; v1 relies on save/revert workflow
- Component type definitions — comes from DiaReflect's ComponentTypeRegistry

## Public Interfaces

### Plugin Implementation

```cpp
namespace Dia::SceneEditor
{
    class DiaSceneEditorPlugin final : public Dia::Editor::IEditorPlugin
    {
    public:
        const char* GetName() const override { return "DiaSceneEditor"; }
        const char* GetVersion() const override { return "1.0.0"; }
        const char* GetDescription() const override {
            return "Author scene files and entity blueprints";
        }
        const char* GetUIPath() const override;
        Dia::Editor::LayoutMode GetLayoutMode() const override {
            return Dia::Editor::LayoutMode::kDockable;
        }

        void OnLoad(Dia::Editor::EditorModel* model) override;
        void OnUnload() override;
        void OnProjectChanged(const Dia::Editor::ProjectContext& ctx) override;
        void OnUpdate(float deltaTime) override;
        void* GetPluginData() override;

    private:
        SceneHierarchyController   mHierarchyController;
        PropertyInspectorController mPropertyController;
        SceneFileHandler           mFileHandler;
    };
}
```

### Controllers

- `SceneHierarchyController` — manages left panel state (sections, selection, filter)
- `PropertyInspectorController` — drives the right panel detail view based on selection type; shows blueprint defaults as read-only reference
- `SceneFileHandler` — `.diascene` JSON read/write using DiaScene2D's `Scene2D` reflected struct

### Data Flow

```
.diagame (Project Context)
  → lists stages (.diastage files)
    → each stage references a .diascene
      → DiaSceneEditor loads the selected .diascene
        → Scene2D struct (layers, cameras, lights, entities)
        → Blueprint references resolved to .diaentitytemplatetemplate files
```

## File Formats

### .diascene (owned by DiaScene2D — this editor reads/writes it)

```json
{
  "scene2d": {
    "world_bounds": { "min": [0, 0], "max": [1920, 1080] },
    "layers": [...],
    "cameras": [...],
    "lights": [...],
    "entities": [
      {
        "id": "player_spawn",
        "blueprint": "player_entity",
        "enabled": true,
        "instance_data": { "Transform2D.position": [400, 300] }
      }
    ]
  }
}
```

### .diaentitytemplatetemplate (entity blueprint — owned by diaentitytemplate)

```json
{
  "entity_blueprint": {
    "id": "player_entity",
    "components": [
      {
        "type": "Transform2D",
        "fields": { "position": [0, 0], "rotation": 0, "scale": [1, 1] }
      },
      {
        "type": "Health",
        "fields": { "max_hp": 50, "current_hp": 50, "invulnerable": false }
      }
    ]
  }
}
```

### .diacamera (camera blueprint — owned by DiaCamera2D)

```json
{
  "camera_blueprint": {
    "id": "camera_2d_follow",
    "components": [
      {
        "type": "Camera2D",
        "fields": { "position": [0, 0], "zoom": 1.0, "rotation": 0 }
      },
      {
        "type": "FollowBehaviour",
        "fields": { "target": "", "offset": [0, 0], "damping": 0.1 }
      }
    ]
  }
}
```

### .dialight (light blueprint — owned by DiaLighting2D)

```json
{
  "light_blueprint": {
    "id": "point_light_warm",
    "components": [
      {
        "type": "PointLight2D",
        "fields": { "position": [0, 0], "radius": 100, "intensity": 1.0, "color": [1, 0.8, 0.5] }
      }
    ]
  }
}
```

## System-Level Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| SED-SCN-001 | Stage/scene selection via toolbar dropdown, not file picker | Follows DiaAssetCatalogueEditor pattern — all editors derive content from the connected `.diagame` via Project Context |
| SED-SCN-002 | Template editing is a SEPARATE editor plugin (DiaEntityTemplateEditor) | Templates are standalone assets shared across scenes; editing them is not a scene concern. Scene editor shows a "Template →" link that opens DiaEntityTemplateEditor directly. No Blueprint Defaults tab — the link is the navigation mechanism. |
| SED-SCN-003 | No Blueprint Defaults tab — template link replaces it | A dedicated read-only tab added complexity without value. The "Template →" link in the Identity section opens DiaEntityTemplateEditor for the full template view. |
| SED-SCN-004 | Change Template is lossy — transferring overrides are shown, orphaned overrides are warned | Users must see blast radius before confirming; orphaned overrides are not silently deleted |
| SED-SCN-005 | No 2D viewport in v1 — list + property panel only | Viewport is high complexity; property-based editing ships value immediately; viewport is a future feature spec |
| SED-SCN-006 | Blueprint file extensions: `.diaentitytemplatetemplate` (entities), `.diacamera` (cameras), `.dialight` (lights) | Separate extensions per type — clear from filename what you're editing; matches owning modules (diaentitytemplate, DiaCamera2D, DiaLighting2D) |
| SED-SCN-007 | Duplicate offsets position by +50 on both axes | Prevents exact overlap; user adjusts after placement |
| SED-SCN-008 | Undo/redo deferred to v2 | Reduce v1 scope; save/revert workflow is sufficient for initial authoring |
| SED-SCN-009 | No Save button — autosave on every mutation | Every add/delete/rename/override change writes immediately; dirty indicator is informational only |
| SED-SCN-010 | Template discovery via asset_catalogue.query_asset_ids | The "+ Entity/Camera/Light" pickers call `scene_editor.get_available_blueprints` which queries the asset catalogue with typeId filter; not a filesystem scan |
| SED-SCN-011 | Templates must pre-exist before placement | "+ Entity" shows a picker from existing templates; standalone template creation is a separate flow in DiaEntityTemplateEditor via the Asset Catalogue |
| SED-SCN-014 | Instance panel shows all blueprint fields (non-overridden fields dimmed and non-editable) | Gives full context without requiring tab-switch; only overridden fields have purple left-border and are editable; non-overridden fields have muted styling |
| SED-SCN-015 | All field defaults are zero/empty | New blueprints, new components, new overrides all default to zero-values unless explicitly set |
| SED-SCN-016 | Project disconnect loses unsaved changes | Same behaviour as all other editor plugins — no special handling |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| scene-hierarchy-panel | Plugin scaffold + left panel with collapsible sections (Layers/Cameras/Lights/Entities) + search/filter + selection + property panel + blueprint read-only reference | [scene-hierarchy-panel.md](../../features/dia/diasceneeditor/scene-hierarchy-panel.md) | Draft |
| entity-placement-crud | Add/duplicate/delete entity instances; assign blueprint on creation; edit instance_data overrides; context menu | [entity-placement-crud.md](../../features/dia/diasceneeditor/entity-placement-crud.md) | Draft |
| change-blueprint | Reassign entity template with transfer analysis and orphan warning dialog | [change-blueprint.md](../../features/dia/diasceneeditor/change-blueprint.md) | Draft |
| layer-authoring | Add/edit/reorder/delete layers; parallax, sort_order, enabled, sort_policy | [layer-authoring.md](../../features/dia/diasceneeditor/layer-authoring.md) | Draft |
| camera-light-authoring | Add/edit/delete cameras and lights; active camera enforcement; affects_layers checkboxes | [camera-light-authoring.md](../../features/dia/diasceneeditor/camera-light-authoring.md) | Draft |
| scene-validation | Enforce single active camera, default layer present, unique IDs; display validation panel | [scene-validation.md](../../features/dia/diasceneeditor/scene-validation.md) | Draft |

## Dependencies

| System | Reason |
|--------|--------|
| DiaScene2D | Owns `.diascene` format and `Scene2D` struct; SceneEditor reads/writes these files |
| DiaEditor | Provides `IEditorPlugin` framework, `ProjectContext`, `EditorModel` |
| DiaReflect | ComponentTypeRegistry for enumerating available component types and field metadata |
| DiaGame | `.diagame` manifest format; provides stage list to populate toolbar dropdown |
| DiaAssetCatalogue | Provides discovery of available blueprints (`.diaentitytemplatetemplate`, `.diacamera`, `.dialight`) via asset registry queries |
| diaentitytemplate | Owns `.diaentitytemplatetemplate` format definition; provides entity/component type system |
| DiaCamera2D | Owns `.diacamera` format definition |
| DiaLighting2D | Owns `.dialight` format definition |

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | Entity IDs, blueprint names, layer IDs, component type IDs all use StringCRC. Compliant. |
| PD-002 | ProcessingUnit/Phase/Module architecture for app structure | DiaSceneEditor is a pure library (`IEditorPlugin` subclass). No Module/Phase/PU. Compliant. |
| PD-004 | No STL containers in public APIs | File I/O uses `Json::Value` (blessed). Internal STL is private. Compliant. |
| PD-006 | Visual Studio project files are source of truth | New `DiaSceneEditor.vcxproj`. Compliant. |
| PD-007 | C++20 required | All new code under `/std:c++20`. Compliant. |

## Open Design Questions

1. **Should the `.diaentitytemplatetemplate` format spec live in diaentitytemplate's system spec or get its own mini-spec?** Format is defined inline here for now; may need promotion to diaentitytemplate system spec when other tools (CLI schema export, runtime loader, DiaEntityTemplateEditor) also need to reference it.

## Status

`Done` — [Plan](diasceneeditor.plan.md)
