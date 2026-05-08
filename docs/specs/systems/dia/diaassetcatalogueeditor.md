# System Spec: DiaAssetCatalogueEditor

## Parent Application
@docs/specs/applications/dia.md

## Asset System Context
@docs/specs/systems/dia/asset-system-overview.md

## Summary

DiaAssetCatalogueEditor is the build-time editor tool for authoring and maintaining `assets.catalogue.json` (the source-side catalogue manifest, stored in `Assets/<AppName>/`). It is the primary way asset records are created, edited, tagged, and connected. It implements `IEditorPlugin` and runs inside CluicheEditor. The catalogue manifest is read by DiaAssetPipeline, which generates a separate `assets.runtime.json` for deployment.

DiaAssetCatalogueEditor has no business logic of its own — it calls DiaAssetCatalogue's query and mutation API and renders the results. It does not run the asset pipeline, load asset content, or connect to a running game.

DiaAssetCatalogueEditor answers: "how do I build and maintain the catalogue of what assets exist?"

## Responsibilities

**Owns:**
- Manifest load/save — open `Assets/<AppName>/assets.catalogue.json`, edit it, save it back; the catalogue manifest is the canonical authored output of this tool
- Asset record authoring — create, edit, delete asset records (ID, type, source path, tags, scope, relationships)
- File discovery — browse the filesystem from a configured root (`Assets/` by default), auto-identify asset types via AssetTypeRegistry file pattern matching, suggest records for unregistered files
- Asset type routing — "open" action per asset record: routes to registered type-specific editor plugin (e.g. `DiaApplicationEditor` for `.diaapp`) or falls back to OS default application
- Relationship graph view — visual display of forward/reverse asset relationships via RelationshipIndex
- Validation status display — show which assets fail type schema constraints or have dangling references
- Catalogue rules UI — presents the rules engine (DiaAssetCatalogue's `CatalogueRulesEngine`) results, allows "Dry Run" preview (shows proposed changeset with conflict warnings before committing), "Apply Rules" trigger, displays manual overrides. All rule evaluation, validation, dry-run, and conflict detection logic is in DiaAssetCatalogue.
- Session context — persists last-opened manifest path and view state per SED-021

**Does NOT own:**
- Any business logic — all data operations go through DiaAssetCatalogue's API
- Asset pipeline execution — that is DiaAssetPipeline / DiaPipeline
- Asset content loading or rendering preview — OS default handles open; no in-editor preview
- Live game connection — pure build-time tool (future: DiaAssetRuntime live state view, noted as deferred)
- Manifest format definition — owned by DiaAssetCatalogue

## Public Interfaces

### Plugin Implementation

```cpp
namespace Dia::AssetCatalogue::Editor
{
    class DiaAssetCatalogueEditor : public Dia::Editor::IEditorPlugin
    {
    public:
        const char* GetName() const override { return "DiaAssetCatalogueEditor"; }
        const char* GetVersion() const override { return "1.0.0"; }
        const char* GetDescription() const override {
            return "Author and maintain the asset catalogue manifest";
        }
        const char* GetUIPath() const override;   // Path to CEF UI assets
        Dia::Editor::LayoutMode GetLayoutMode() const override {
            return Dia::Editor::LayoutMode::kDockable;
        }

        void OnLoad(Dia::Editor::EditorModel* model) override;
        void OnUnload() override;
        void OnUpdate(float deltaTime) override;
        void* GetPluginData() override;

    private:
        Dia::AssetCatalogue::AssetRegistry    mRegistry;
        Dia::AssetCatalogue::CatalogueManifestSerializer mSerializer;
        Dia::AssetCatalogue::AssetTypeRegistry mTypeRegistry;
        Dia::AssetCatalogue::RelationshipIndex* mRelationshipIndex; // owned by mRegistry
    };
}
```

### Commands (DiaAPI — bridge to CEF UI)

| Command | Description |
|---------|-------------|
| `asset_catalogue.load_manifest { path }` | Load manifest from path into registry |
| `asset_catalogue.save_manifest { path }` | Save current registry state to path |
| `asset_catalogue.create_record { id, type, source_path, tags, scope, stage }` | Add a new asset record |
| `asset_catalogue.update_record { id, ...fields }` | Update fields on an existing record |
| `asset_catalogue.delete_record { id }` | Remove a record from the registry |
| `asset_catalogue.add_relationship { fromId, type, toId }` | Add a relationship edge |
| `asset_catalogue.remove_relationship { fromId, type, toId }` | Remove a relationship edge |
| `asset_catalogue.discover_files { root_path }` | Scan directory, return unregistered files with suggested type |
| `asset_catalogue.open_asset { id }` | Route to registered type editor or OS default |
| `asset_catalogue.query_by_type { typeId }` | Return all records of a type |
| `asset_catalogue.query_by_tag { tag }` | Return all records with a tag |
| `asset_catalogue.get_forward_refs { id }` | Return forward relationship edges |
| `asset_catalogue.get_reverse_refs { id }` | Return reverse relationship edges |
| `asset_catalogue.validate { }` | Run validation; return per-record error list |

### Asset Type Editor Registry

Each asset type can register a custom editor plugin ID. If none is registered, the OS default is used.

```cpp
namespace Dia::AssetCatalogue::Editor
{
    class AssetTypeEditorRegistry
    {
    public:
        void RegisterTypeEditor(const Dia::Core::StringCRC& typeId,
                                const Dia::Core::StringCRC& editorPluginId);
        const Dia::Core::StringCRC* FindEditorForType(
            const Dia::Core::StringCRC& typeId) const;
    };
}
```

**Built-in registrations:**

| Asset Type | Editor Plugin |
|------------|--------------|
| `entity` | (OS default — `.entity.json` opens in OS JSON editor) |
| `config` | (OS default) |
| `stage` | (OS default) |
| `folder` | (OS default — opens folder in file explorer) |
| All others | OS default |

Game-specific editors (e.g. a future `DiaEntityEditor`) register themselves at startup.

### UI Panels (CEF)

| Panel | Description |
|-------|-------------|
| Asset List | Filterable table of all records — ID, type, status, tags, scope |
| Record Editor | Form to view/edit a single record's fields; relationship list with add/remove |
| Relationship Graph | Visual graph of selected asset's forward/reverse deps via RelationshipIndex |
| File Discoverer | Filesystem browser from configured root; shows unregistered files with suggested type; bulk-add selected files |
| Validation Panel | Per-record error list from last validate run; click to jump to record |

## Dependencies

| Dependency | What DiaAssetCatalogueEditor uses from it |
|------------|------------------------------------------|
| DiaAssetCatalogue | `AssetRegistry`, `AssetTypeRegistry`, `RelationshipIndex`, `CatalogueManifestSerializer`, `JsonDefinitionLoader` — all data operations |
| DiaEditor | `IEditorPlugin`, `EditorPluginRegistry`, `EditorModel`, `IEditorCommand` + `CommandHistory` (undo/redo), `SED-020` output path convention |
| DiaUICEF | CEF rendering of all editor UI panels |

No dependency on DiaAssetPipeline, DiaAssetRuntime, DiaWebSocket, or any rendering/windowing module.

## Features

| # | Feature | Size | Description | Spec | Status |
|---|---------|------|-------------|------|--------|
| 1 | Manifest Load/Save | S | Open manifest, load into registry, save back. Session context persists last path. | [manifest-load-save.md](../../features/dia/diaassetcatalogueeditor/manifest-load-save.md) | Done |
| 2 | Asset Record CRUD | M | Create, edit, delete records. Tag editing, scope selection, stage name. Full undo/redo. Calls `ContentHasher` (DiaAssetCatalogue) on create/update. | [asset-record-crud.md](../../features/dia/diaassetcatalogueeditor/asset-record-crud.md) | Done |
| 3 | File Discoverer | M | Filesystem browse from root, pattern matching via AssetTypeRegistry, bulk-add suggestions. | [file-discoverer.md](../../features/dia/diaassetcatalogueeditor/file-discoverer.md) | Done |
| 4 | Relationship Editor | S | Add/remove relationship edges per record. Forward/reverse display. | [relationship-editor.md](../../features/dia/diaassetcatalogueeditor/relationship-editor.md) | Done |
| 5 | Relationship Graph View | M | Visual graph of asset dependencies. Click to navigate to record. | [relationship-graph-view.md](../../features/dia/diaassetcatalogueeditor/relationship-graph-view.md) | Done |
| 6 | Validation Panel | S | Run validate, display per-record errors, click-to-navigate. | [validation-panel.md](../../features/dia/diaassetcatalogueeditor/validation-panel.md) | Done |
| 7 | Asset Type Editor Routing | S | `AssetTypeEditorRegistry`, open-asset action, OS default fallback. | [asset-type-editor-routing.md](../../features/dia/diaassetcatalogueeditor/asset-type-editor-routing.md) | Done |
| 8 | Catalogue Rules UI | S-M | UI for the Catalogue Rules Engine (DiaAssetCatalogue Feature 4). Load/save `assets.rules.json`, "Dry Run" preview (shows proposed changeset with conflict warnings), "Apply Rules" action, display results and manual overrides. All business logic (validation, dry-run, conflict detection, evaluation) is in DiaAssetCatalogue — this feature is presentation only. | [catalogue-rules-ui.md](../../features/dia/diaassetcatalogueeditor/catalogue-rules-ui.md) | Done |

**Build order:** 1 → 2 → 3 → 4 → 6 → 7 → 8 → 5 (rules engine after relationship editor exists; graph view last)

## Design Constraints

- **UI only — no business logic.** All data operations go through DiaAssetCatalogue's API. DiaAssetCatalogueEditor owns only presentation and user interaction.
- **Manifest is the output.** The editor's job is to produce a correct `assets.catalogue.json`. The pipeline reads it; the editor writes it.
- **OS default for asset open.** No in-editor preview or content rendering. Type-specific editors are separate plugins that register themselves.
- **Undo/redo for all mutations.** Every create/edit/delete/relationship change is an `IEditorCommand`. Follows DiaEditor's command pattern (SED-009).
- **Discovery suggests, user decides.** File discoverer never auto-adds records. It proposes; the user confirms.
- **No live game connection.** Pure build-time tool. Future live state view (showing DiaAssetRuntime load state) is deferred.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-ACE-001 | DiaAssetCatalogueEditor owns `assets.catalogue.json` authoring in `Assets/<AppName>/`; DiaAssetPipeline reads it and generates `assets.runtime.json` for deployment | Single source of truth for catalogue mutations; pipeline is a consumer of catalogue data and a producer of the runtime manifest (SD-APIPE-001) | All features | Accepted | Yes |
| SD-ACE-002 | File discoverer suggests records; user confirms before adding | Prevents phantom records from accidental discovery runs; user intent required for all registry mutations | File Discoverer | Accepted | Yes |
| SD-ACE-003 | Asset type routing uses OS default unless a type-specific editor is registered | Avoids embedding content editors in DiaAssetCatalogueEditor; extensible without modifying this system | Asset Type Editor Routing | Accepted | Yes |
| SD-ACE-004 | All manifest mutations are IEditorCommand instances (undo/redo enabled) | Consistent with DiaEditor framework (SED-009); manifest editing errors are recoverable | All features | Accepted | Yes |
| SD-ACE-005 | Output written to `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/` | Per SED-020 — all generated editor output is per-plugin under `Cluiche/out/CluicheEditor/` | All features | Accepted | Yes |
| SD-ACE-006 | No live game connection in this system | Pure build-time tool; DiaAssetRuntime live state view is a future feature for a separate plugin or DiaAssetRuntime extension | All features | Accepted | Yes |
| SD-ACE-007 | Rules engine logic lives in DiaAssetCatalogue (SD-CAT-008); editor provides UI only | Editor is UI only — rule evaluation, scope computation, hash computation, and relationship inference are DiaAssetCatalogue's `CatalogueRulesEngine`, `ScopeComputer`, `ContentHasher`, and `RelationshipInferrer` APIs. Editor calls these APIs and presents results. | Catalogue Rules UI | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | Source | Implication for DiaAssetCatalogueEditor |
|----|----------|--------|-----------------------------------------|
| PD-001 | StringCRC for all entity/component IDs | Platform | Asset IDs, type IDs, relationship types are all StringCRC. All registry lookups CRC-keyed. |
| PD-004 | No STL containers in public APIs | Platform | Plugin's C++ public interface uses DiaCore containers. CEF bridge uses JSON strings (not C++ STL). |
| PD-005 | x64 Windows only | Platform | No cross-platform considerations. OS default open uses `ShellExecuteEx`. |
| PD-006 | Visual Studio project files are source of truth | Platform | DiaAssetCatalogueEditor.vcxproj maintained manually. |
| PD-007 | C++20 required | Platform | Compliant. |
| PD-008 | Directory.Build.props owns OutDir | Platform | DiaAssetCatalogueEditor.vcxproj inherits centralized build settings. |
| PD-009 | Generated output under Cluiche/out/ | Platform | Session context and logs written to `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/` per SD-ACE-005. |
| AD-001 | Module system with YAML frontmatter | Application | `dia.assetcatalogueeditor.architecture.module.md` created with this system. |
| AD-002 | No STL in public APIs | Application | Same as PD-004. |
| AD-003 | Namespace: Dia::\<Module\>:: | Application | All code under `Dia::AssetCatalogue::Editor::` namespace. |
| SD-CAT-001 | Asset IDs strictly `type.name` composites | DiaAssetCatalogue | Editor enforces `type.name` format on create; shows error inline if format invalid. |
| SD-CAT-007 | AssetRecord carries scope and tags | DiaAssetCatalogue | Editor exposes scope selector (global/stage + stage name) and tag editor per record. |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | DiaEditor | All manifest mutations wrapped as IEditorCommand. SD-ACE-004. |
| SED-015 | DiaEditor is a pure C++ library, no DiaApplication dependency | DiaEditor | DiaAssetCatalogueEditor implements IEditorPlugin; no DiaApplication types used. |
| SED-020 | Plugin output to `Cluiche/out/CluicheEditor/<PluginName>/` | DiaEditor | Output path is `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/`. SD-ACE-005. |
| SED-021 | Per-plugin session context via `.context.json` | DiaEditor | Session context (last manifest path, active panel, selected record) persisted in `.context.json`. |
| SD-APIPE-001 | DiaAssetPipeline reads `assets.catalogue.json`; generates `assets.runtime.json` | DiaAssetPipeline | DiaAssetCatalogueEditor is the sole writer of `assets.catalogue.json`. Pipeline never writes to it. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Manifest | Can DiaAssetCatalogueEditor create a new manifest from scratch, or does it require an existing file? | Both. New manifest creates an empty registry; Save writes a new file. Open loads an existing one. |
| 2 | Discovery | Should the file discoverer show files that are already registered, or only unregistered files? | Only unregistered files — the list is a "what's missing?" tool, not a full file browser. |
| 3 | Routing | What happens if `open_asset` is called for a type with no registered editor and no OS association? | Log a warning to the output console; no crash. Some file types (e.g. `.texture.png`) have OS associations by default. |
| 4 | Validation | Is validation run automatically on save, or only on explicit user action? | Explicit only — `asset_catalogue.validate` command. Auto-validate on save can be added as a user preference later. |
| 5 | Undo/redo | Should undo/redo cross manifest load/save boundaries (i.e. can you undo a load)? | No — loading a manifest resets the command history. Save does not affect history (it's not undoable). |
| 6 | Relationships | Should the relationship graph show the full transitive closure, or only direct edges? | Direct edges only by default. Depth can be a UI control (expand node to show its deps). Full transitive closure is expensive and visually noisy for large catalogues. |
| 7 | Scope | Who computes scope (`global` vs `stage`) — does the editor enforce the rule (asset referenced by >1 Stage = global), or can the user override it manually? | Editor enforces the rule automatically based on relationship data, but allows manual override. Override is flagged visually as "manually set" so it's auditable. |

## Status

`Done` — All 8 features implemented and tested.  
**Plan:** @docs/specs/systems/dia/diaassetcatalogueeditor.plan.md
