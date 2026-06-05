# Feature Spec: scene-hierarchy-panel

**System:** DiaSceneEditor
**App:** Dia
**Status:** Draft
**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Summary

Implement the `DiaSceneEditorPlugin` scaffold, the scene hierarchy panel (left panel with collapsible sections for Layers/Cameras/Lights/Entities), search/filter, selection state, and the property inspector panel (right panel) with context-sensitive rendering per item type. Blueprint defaults are shown read-only with a link to DiaEntityTemplateEditor. This is the foundational feature — all subsequent features (entity CRUD, change-blueprint, layer authoring) build on this scaffold.

## Traceability

| Level | Spec |
|---|---|
| Platform | [Cluiche.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diasceneeditor.md](../../systems/dia/diasceneeditor.md) |
| Depends on system | [diascene2d.md](../../systems/dia/diascene2d.md) |
| Depends on system | [diaeditor.md](../../systems/dia/diaeditor.md) |
| Depends on system | [diagame.md](../../systems/dia/diagame.md) |

## Goals

- CluicheEditor can load a `.diagame`, select a stage/scene from the toolbar, and display its full contents (layers, cameras, lights, entities, referenced blueprints)
- Selecting any scene item shows its properties in the right panel with type-appropriate fields
- The plugin scaffold is in place so subsequent features only need to add their specific logic (CRUD operations, blueprint editing, validation)
- File I/O for `.diascene` works: load on scene selection, save on explicit Save action

## Acceptance Criteria

### Plugin scaffold
- `DiaSceneEditorPlugin` class exists in `Dia/DiaSceneEditor/`, inherits `IEditorPlugin`
- Plugin registers via `REGISTER_EDITOR_PLUGIN(DiaSceneEditorPlugin, "DiaSceneEditor")`
- `GetLayoutMode()` returns `LayoutMode::kDockable`
- `OnProjectChanged` callback fires when `.diagame` is loaded; populates stage/scene dropdowns
- `OnLoad` / `OnUnload` / `OnUpdate` lifecycle hooks work correctly

### Stage/Scene selector (toolbar)
- Toolbar shows "Stage:" dropdown populated from `.diagame` stage imports
- Toolbar shows "Scene:" dropdown populated from the selected stage's `.diastage` scene reference
- Selecting a different stage loads that stage's `.diascene` file
- If no `.diagame` is loaded, both dropdowns are empty and disabled

### Scene file I/O
- `SceneFileHandler` reads `.diascene` JSON into a `Scene2D` struct via JsonArchive
- `SceneFileHandler` writes the current `Scene2D` state back to the same file path on Save
- Title bar shows the loaded `.diascene` filename
- Dirty flag tracked: any property edit marks the scene dirty; Save clears it
- Title bar shows "* Unsaved" indicator when dirty

### Scene hierarchy panel (left panel)
- Left panel has column header "SCENE HIERARCHY" with total item count
- Search input filters all sections simultaneously by ID or blueprint name (case-insensitive)
- Four collapsible sections, each with chevron, type icon, section name, and item count badge:
  - **Layers** `[L]` teal — each row shows layer ID + sort_order value
  - **Cameras** `[C]` purple — each row shows camera ID + [ACTIVE] badge if active
  - **Lights** `[*]` amber — each row shows light ID + [DIR]/[PNT] type badge
  - **Entities** `[E]` green — each row shows entity ID + blueprint name (dimmed)
- All sections default to expanded
- Clicking a section header toggles collapse/expand
- Single-click a row to select it (purple left-border highlight)
- Items with `enabled: false` render at 50% opacity

### Property inspector (right panel)
- When nothing is selected: centered placeholder with "Select an item from the scene hierarchy"
- Context strip shows: type icon + selected item ID (bold) + blueprint reference (if applicable)
- Sub-tabs shown for items that have a blueprint (entities, cameras, lights):
  - **Instance Overrides** (default active) — editable fields
  - **Blueprint Defaults** — read-only view of all blueprint fields for reference
- No sub-tabs for layers or blueprints (layers have no blueprint; blueprints are edited directly)

### Property rendering by type

**Entity (Instance Overrides tab):**
- Identity section: ID (read-only), Blueprint (link to DiaEntityTemplateEditor + "Change..." button), Enabled (checkbox)
- Instance Data section: ALL blueprint fields shown, grouped by component prefix (e.g. "Transform2D", "Health")
  - **Overridden fields:** purple left-border, full-brightness text, editable input
  - **Non-overridden fields (inherited defaults):** no left-border, muted/dimmed styling, non-editable (greyed input with dashed border)
  - Clicking a non-overridden field promotes it to an override (copies blueprint default value into `instance_data`, becomes editable)
  - Per SED-SCN-014: shows full context without requiring Blueprint Defaults tab switch
- "+ Add Override..." button opens same dropdown as clicking a dimmed field (lists non-overridden fields)

**Camera (Instance Overrides tab):**
- Identity section: ID (read-only), Blueprint (link), Active (checkbox)
- Instance Data Overrides section: same grouped field layout as entities

**Light (Instance Overrides tab):**
- Identity section: ID (read-only), Blueprint (link), Enabled (checkbox)
- Affects Layers section: checkbox per layer in the scene
- Instance Data Overrides section: same grouped field layout

**Layer (no tabs):**
- Identity section: ID (read-only), Sort Order (editable int), Enabled (checkbox)
- Parallax section: X (float), Y (float)
- Sort Policy section: Policy dropdown (v1: only "insertion")
- Assigned Lights section: informational list of lights that reference this layer

### Blueprint link and "Open in Blueprint Editor"
- Clicking the blueprint `→` link on any entity/camera/light opens DiaEntityTemplateEditor with that blueprint focused
- This is the only path to editing blueprint defaults from the scene editor — no inline blueprint editing

### Blueprint Defaults tab (read-only reference)
- Banner: "Blueprint defaults from {name}.diaentitytemplate (read-only)" with "Open in Blueprint Editor →" link
- All blueprint fields shown grouped by component, inputs disabled (greyed/dashed styling)
- Clicking "Open in Blueprint Editor →" opens DiaEntityTemplateEditor with that blueprint focused

### Status bar
- Shows: connected `.diagame` name, entity count, camera count, light count, layer count, blueprint count, save state

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaSceneEditor/DiaSceneEditor.vcxproj` | New — static library project |
| `Dia/DiaSceneEditor/DiaSceneEditor.vcxproj.filters` | New |
| `Dia/DiaSceneEditor/DiaSceneEditorPlugin.h` | New — `IEditorPlugin` subclass |
| `Dia/DiaSceneEditor/DiaSceneEditorPlugin.cpp` | New |
| `Dia/DiaSceneEditor/SceneHierarchyController.h` | New |
| `Dia/DiaSceneEditor/SceneHierarchyController.cpp` | New |
| `Dia/DiaSceneEditor/PropertyInspectorController.h` | New |
| `Dia/DiaSceneEditor/PropertyInspectorController.cpp` | New |
| `Dia/DiaSceneEditor/SceneFileHandler.h` | New — `.diascene` load/save |
| `Dia/DiaSceneEditor/SceneFileHandler.cpp` | New |
| `Cluiche/Cluiche.sln` | Add `DiaSceneEditor.vcxproj` |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all entity/component IDs | Layer IDs, entity IDs, blueprint names, component types all use StringCRC internally. Compliant. |
| PD-002 | PU/Phase/Module architecture | DiaSceneEditorPlugin is a plain `IEditorPlugin` subclass. No PU/Phase/Module. Compliant. |
| PD-004 | No STL containers in public APIs | File I/O uses `Json::Value`. Controller internals may use STL privately. Compliant. |
| PD-006 | VS project files are source of truth | New `DiaSceneEditor.vcxproj`. Compliant. |
| SED-SCN-001 | Stage/scene via toolbar dropdown | Implemented as described. Compliant. |
| SED-SCN-003 | Blueprint defaults read-only on instance tab | Instance tab shows overrides; Blueprint Defaults tab is read-only. Compliant. |
| SED-SCN-009 | Blueprint discovery via asset catalogue | Blueprint pickers query DiaAssetCatalogue for available blueprints. Compliant. |

## Open Design Questions

None — all resolved.

## Resolved Questions

| # | Question | Resolution |
|---|----------|------------|
| 1 | Should the property panel show non-overridden blueprint fields as dimmed reference rows? | Yes (SED-SCN-014). All blueprint fields shown: overridden = purple left-border + editable; non-overridden = dimmed + greyed/dashed + non-editable. Clicking a dimmed field promotes it to an override. |
| 2 | If the `.diascene` references a blueprint that doesn't exist as a file, how should the editor behave? | Show the entity row normally but mark the blueprint badge as red/error. Properties panel shows "Blueprint file not found" warning. User can still edit instance_data but cannot view inherited defaults. |

## Status

`Approved`
