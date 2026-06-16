# Feature Spec: new-scene

**Parent:** @docs/specs/applications/dia/systems/diaassetcatalogueeditor/diaassetcatalogueeditor.md
**Status:** Approved

## Summary

Users have no way to create a new `.diascene` file from within the editor; they must hand-create files outside the tool. This feature adds a "New Scene" creation flow to DiaAssetCatalogueEditor and a shortcut button in DiaSceneEditor that delegates to the catalogue — no logic duplication.

## Goals

- Create a blank `.diascene` file and register it as an asset record in one action
- Immediately open the new scene in DiaSceneEditor via the existing `open_asset` routing
- DiaSceneEditor toolbar provides a "New Scene" shortcut that delegates entirely to the catalogue

## Acceptance Criteria

### Catalogue side

- DiaAssetCatalogueEditor's New Record dialog gains a **"Scene"** type entry for asset type `diascene`
- On confirm, a blank `.diascene` file is written to the specified path with the canonical empty-scene schema:
  ```json
  { "version": 1, "entities": [], "cameras": [], "lights": [], "layers": [] }
  ```
- A catalogue record (`id`, `typeId = "diascene"`, `sourcePath`) is created via `CreateRecordCommand` and committed to the registry (undo/redo enabled per SD-ACE-004)
- `assets.catalogue.json` is saved after record creation
- The new scene is immediately opened in DiaSceneEditor via `asset_catalogue.open_asset`
- `diascene` type is seeded in `AssetTypeEditorRegistry` → `DiaSceneEditor` at `OnLoad` (same pattern as `diaentitytemplate`/`diacamera`/`dialight` → `DiaEntityTemplateEditor`)
- `diascene` is added to the `get_asset_types` known-types list with display name `"Scene"`

### DiaSceneEditor shortcut

- DiaSceneEditor toolbar shows a **"New Scene"** button
- Clicking it sends `asset_catalogue.new_scene_shortcut` to C++; the plugin invokes `asset_catalogue.create_record` + `asset_catalogue.open_asset` via `InvokeRequestHandler` — no file-writing logic in DiaSceneEditor
- If the catalogue plugin is not loaded / handler not available, the button shows a toast: _"Open the Asset Catalogue panel first"_

### Out of scope

- Stage → scene association (manifest `sceneFile` field) — separate feature
- Scene templates or non-empty defaults

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.cpp` | Seed `diascene → DiaSceneEditor`; add `diascene` to `get_asset_types`; register `asset_catalogue.create_scene` handler (writes file + CreateRecordCommand + open_asset) |
| `Dia/DiaAssetCatalogueEditor/UI/index.html` | Add "Scene" option to New dialog; wire to `asset_catalogue.create_scene` |
| `Dia/DiaSceneEditor/DiaSceneEditorPlugin.cpp` | Register `asset_catalogue.new_scene_shortcut` handler |
| `Dia/DiaSceneEditor/UI/index.html` | Add "New Scene" toolbar button |

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| SD-ACE-001 | DiaAssetCatalogueEditor owns `assets.catalogue.json` authoring | Scene file creation and record registration happen entirely in the catalogue plugin. DiaSceneEditor does not write any files. Compliant. |
| SD-ACE-003 | Asset type routing uses OS default unless type-specific editor registered | `diascene → DiaSceneEditor` seeded at `OnLoad` so `open_asset` routes correctly without load-order dependency. Compliant. |
| SD-ACE-004 | All manifest mutations are `IEditorCommand` instances | Record creation uses `CreateRecordCommand`. Compliant. |
| SD-ACE-006 | No live game connection | Creation is build-time only. Compliant. |

## Open Design Questions

1. **Path picker UX** — Follow the entity blueprint convention: the New Scene dialog asks for a relative path typed by the user (e.g. `../Stages/MyStage/my_scene.diascene`), relative to the `.diagame` file's directory. No auto-placement — mirrors how other assets are placed.

2. **Catalogue-not-open case** — If the user clicks "New Scene" in DiaSceneEditor but the catalogue panel isn't loaded, the shortcut triggers the catalogue panel to load first (via `IPluginLoader::LoadPlugin`), then invokes the create flow.
