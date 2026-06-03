# System Spec: DiaBlueprintEditor

**Mockup:** TBD (reuse relevant parts of diasceneeditor.mockup.html Blueprints section)

## Parent Application
@docs/specs/applications/dia.md

## Summary

DiaBlueprintEditor is a CluicheEditor plugin for authoring entity, camera, and light blueprint files (`.diaentity`, `.diacamera`, `.dialight`). It provides component CRUD, field default editing, and cross-scene usage visibility. Blueprints are standalone reusable assets that define the template for scene placements.

DiaBlueprintEditor is opened from the DiaAssetCatalogueEditor (which acts as a file explorer — create assets there, open them here). DiaSceneEditor links to it via "Open in Blueprint Editor →" when users need to edit template defaults rather than instance overrides.

DiaBlueprintEditor answers: "how do I define what an entity/camera/light IS (its components and default values)?"

## Responsibilities

**Owns:**
- Blueprint file I/O — load and save `.diaentity`, `.diacamera`, `.dialight` files
- Component CRUD — add/remove components from a blueprint; field type metadata from DiaReflect's ComponentTypeRegistry
- Field default editing — set default values for all component fields; type-aware widgets
- Cross-scene usage display — show which scenes and instances reference this blueprint (blast radius visibility)
- Blueprint creation — create new blueprint files (name + pick initial components); triggered from DiaAssetCatalogueEditor's "new asset" flow
- Cascade awareness — when adding/removing components, show affected instance count across all scenes before confirming
- Blueprint list panel — left panel showing all blueprints registered in the asset catalogue for the current game, grouped by type (Entity/Camera/Light)

**Does NOT own:**
- Scene placements or instance overrides — that is DiaSceneEditor
- Runtime entity inspection — that is DiaEntityInspector
- Asset registration/discovery — that is DiaAssetCatalogueEditor (blueprints must be registered there)
- Component type definition — that is DiaReflect
- Blueprint file format definition — owned by DiaEntity (`.diaentity`), DiaCamera2D (`.diacamera`), DiaLighting2D (`.dialight`)

## Public Interfaces

### Plugin Implementation

```cpp
namespace Dia::BlueprintEditor
{
    class DiaBlueprintEditorPlugin final : public Dia::Editor::IEditorPlugin
    {
    public:
        const char* GetName() const override { return "DiaBlueprintEditor"; }
        const char* GetVersion() const override { return "1.0.0"; }
        const char* GetDescription() const override {
            return "Author entity, camera, and light blueprint files";
        }
        const char* GetUIPath() const override;
        Dia::Editor::LayoutMode GetLayoutMode() const override {
            return Dia::Editor::LayoutMode::kDockable;
        }

        void OnLoad(Dia::Editor::EditorModel* model) override;
        void OnUnload() override;
        void OnProjectChanged(const Dia::Editor::ProjectContext& ctx) override;
        void OnUpdate(float deltaTime) override;
        void OnOpenAsset(const Dia::Core::StringCRC& assetId) override;
        void* GetPluginData() override;

    private:
        BlueprintListController    mListController;
        BlueprintPropertyController mPropertyController;
        BlueprintFileHandler       mFileHandler;
    };
}
```

### Controllers

- `BlueprintListController` — left panel: lists all registered blueprints from asset catalogue, grouped by type
- `BlueprintPropertyController` — right panel: component accordion, field editing, usage display
- `BlueprintFileHandler` — `.diaentity` / `.diacamera` / `.dialight` load/save

### Asset Catalogue Integration

- DiaAssetCatalogueEditor registers DiaBlueprintEditor as the handler for `.diaentity`, `.diacamera`, `.dialight` asset types
- "Open" action on a blueprint asset in the catalogue opens DiaBlueprintEditor with that blueprint selected
- "New Asset → Entity Blueprint / Camera Blueprint / Light Blueprint" in the catalogue creates the file and opens DiaBlueprintEditor

## File Formats

### .diaentity (entity blueprint — format owned by DiaEntity)

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

### .diacamera (camera blueprint — format owned by DiaCamera2D)

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

### .dialight (light blueprint — format owned by DiaLighting2D)

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
| SED-BP-001 | One file per blueprint, stored under asset root | Blueprints are shared across scenes; one file = one source of truth; git-friendly |
| SED-BP-002 | Separate extensions per type: `.diaentity`, `.diacamera`, `.dialight` | Clear from filename what you're editing; matches owning modules |
| SED-BP-003 | Blueprint discovery via DiaAssetCatalogue | Canonical list of what's available comes from asset registry, not filesystem scan |
| SED-BP-004 | Cross-scene usage shown on property panel | Before editing, user sees which scenes + instances are affected (blast radius) |
| SED-BP-005 | Adding a component cascades to all instances | New fields appear with defaults on all referencing instances; show count before confirming |
| SED-BP-006 | Removing a component warns about orphaned overrides | Show count of instances with overrides for that component's fields; don't auto-delete orphans |
| SED-BP-007 | Field defaults come from C++ constructors, surfaced via schema | `--dump-schema` serialises a default-constructed component instance; editors read these from `registeredtypes.diaschema`; empty `"fields"` in a blueprint means "use code default" |
| SED-BP-008 | Blueprint creation triggered from DiaAssetCatalogueEditor | Asset catalogue owns "create new asset" flow; blueprint editor owns "edit existing" flow |
| SED-BP-009 | DiaSceneEditor routes to DiaBlueprintEditor via "Open in Blueprint Editor →" | Clean separation: scene editor handles placements, blueprint editor handles templates |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| blueprint-panel | Plugin scaffold + left panel (blueprint list grouped by type) + right panel (component accordion + field editing) + file I/O | [blueprint-panel.md](../../features/dia/diablueprinteditor/blueprint-panel.md) | Draft |
| component-crud | Add/remove components from a blueprint; cascade awareness; confirmation with affected instance count | [component-crud.md](../../features/dia/diablueprinteditor/component-crud.md) | Draft |
| cross-scene-usage | Usage section showing which scenes and instances reference the selected blueprint | [cross-scene-usage.md](../../features/dia/diablueprinteditor/cross-scene-usage.md) | Draft |
| component-picker-and-defaults | Searchable component picker panel; C++ code defaults surfaced from schema; blueprint-level field overrides | [component-picker-and-defaults.md](../../features/dia/diablueprinteditor/component-picker-and-defaults.md) | Draft |

## Dependencies

| System | Reason |
|--------|--------|
| DiaEditor | Provides `IEditorPlugin` framework, `ProjectContext`, `EditorModel`, `OnOpenAsset` routing |
| DiaReflect | ComponentTypeRegistry for enumerating available component types and field metadata |
| DiaAssetCatalogue | Provides blueprint discovery; registers this editor as handler for blueprint asset types |
| DiaEntity | Owns `.diaentity` format |
| DiaCamera2D | Owns `.diacamera` format |
| DiaLighting2D | Owns `.dialight` format |

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | Blueprint IDs, component type IDs all use StringCRC. Compliant. |
| PD-002 | PU/Phase/Module architecture | Pure `IEditorPlugin` subclass. No PU/Phase/Module. Compliant. |
| PD-004 | No STL containers in public APIs | File I/O uses `Json::Value`. Compliant. |
| PD-006 | VS project files are source of truth | New `DiaBlueprintEditor.vcxproj`. Compliant. |
| PD-007 | C++20 required | Compliant. |

## Open Design Questions

1. **Should DiaBlueprintEditor support editing multiple blueprints simultaneously (tabbed)?** Or always one-at-a-time with a list on the left? V1: one at a time with left panel list.

## Status

`Approved`
