# Feature Spec: shared-asset-creation

**Parent:** @docs/specs/applications/dia/systems/diaassetcatalogueeditor/diaassetcatalogueeditor.md
**Status:** Done

## Summary

Asset creation is currently duplicated: DiaSceneEditor has its own "+New Scene" that delegates ad-hoc to the Asset Catalogue, the Blueprint Editor creates templates only as a nav-failure fallback, and the Asset Catalogue has its own `create_scene` handler. This feature unifies asset creation behind a single **`asset_catalogue.create_asset`** contract that any editor can invoke, with editor-specific follow-up handled locally by the caller.

## Goals

- **One code path** for creating any asset type (scene, entity template, camera/light blueprint)
- **Any editor** can invoke creation via `asset_catalogue.create_asset` and receive a result to do follow-up work
- **Asset Catalogue dropdown** and **editor shortcut buttons** invoke the same handler
- **Scene Editor** "+New Scene" also writes the `scene` field into the selected `.diastage` (stage association)
- **Blueprint Editor** gains a "+New Template" button using the same contract
- **Stage dropdown** visually distinguishes stages with vs without scenes

## Acceptance Criteria

### AC-1: Generalized `asset_catalogue.create_asset` handler

- Replaces the type-specific `asset_catalogue.create_scene` handler
- Request: `{ assetType: string, id: string, source_path: string }`
- Looks up a registered **file template** by `assetType` (blank scene JSON, blank entity template JSON, etc.)
- Writes the template file to the resolved path
- Creates a catalogue record via `CreateRecordCommand`
- Auto-saves the manifest
- Response: `{ success: bool, id: string, absPath: string }`
- Does NOT auto-open the asset in any editor (callers decide)

### AC-2: Asset type template registry

- `DiaAssetCatalogueEditorPlugin` maintains a map of `assetType → { templateContent, fileExtension }`
- Seeded at `OnPluginLoad` with:
  - `diascene` → blank scene JSON
  - `diaentitytemplate` → blank entity template JSON (using BlueprintFileHandler's format)
  - `diacamera` → blank camera blueprint JSON
  - `dialight` → blank light blueprint JSON
- Extensible: other plugins could register templates via `asset_catalogue.register_template` in future

### AC-3: Asset Catalogue UI "New" dropdown uses `create_asset`

- Existing `create_record` path in the New dialog switches to `create_asset` for types that have templates registered
- For types without templates (texture, audio, config, folder) the old `create_record` path remains (metadata-only, no file written)

### AC-4: DiaSceneEditor "+New Scene" uses shared contract + stage association

- Calls `asset_catalogue.create_asset` with `assetType: "diascene"`
- On success: writes `"scene": "<relative_path>"` into the currently selected `.diastage` file
- Reloads the stage list and auto-selects the updated stage
- If no stage is selected, scene is created but not associated (user can manually link later)

### AC-5: DiaSceneEditor stage dropdown styling

- Stages with a `scenePath` render normally
- Stages with empty `scenePath` render greyed out with a "(no scene)" suffix
- Both are selectable — selecting a sceneless stage shows an empty canvas with a prominent "Create Scene" action

### AC-6: DiaEntityTemplateEditor "+New Template" button

- Toolbar gains a "+New Template" button
- Invokes `asset_catalogue.create_asset` with `assetType: "diaentitytemplate"`
- On success: navigates to the newly created template (calls existing `selectBlueprint`)
- Replaces the current `create_from_template` as the primary creation flow (nav-failure fallback still calls `create_asset` instead of `create_from_template`)

### Out of scope

- Scene templates (non-empty defaults)
- Drag-and-drop scene assignment to stages
- Multi-scene-per-stage support

## Binding Decisions

- **SD-ACE-001** — Asset Catalogue owns file creation and record registration. All creation goes through it; other editors only do follow-up.
- **SD-ACE-004** — `CreateRecordCommand` used for all record creation (undo/redo enabled).

## Open Design Questions

1. **Deprecate `create_scene` immediately or keep as alias?** — Recommend keeping it for one release as a thin redirect to `create_asset`, then remove.
2. **Stage association write-back** — Should the `.diastage` write also be undoable via a command, or is file-save sufficient? Current thinking: file-save is enough since `.diastage` is outside the catalogue's undo domain.
