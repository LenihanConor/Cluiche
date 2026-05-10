# Feature Spec: Asset Record CRUD

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | **Asset Record CRUD** | (this document) |

## Summary

Asset Record CRUD provides the core create, edit, and delete operations for asset records in the catalogue manifest. Every mutation is wrapped as an `IEditorCommand` for full undo/redo support. The feature includes an Asset List panel (filterable table of all records) and a Record Editor panel (form for viewing/editing a single record's fields). Content hashing is computed via DiaAssetCatalogue's `ContentHasher` on create and update.

## Problem

The catalogue manifest is a structured JSON file containing asset records with IDs, types, source paths, tags, scopes, and stage names. Editing this file by hand is error-prone: asset IDs must follow the `type.name` format (SD-CAT-001), content hashes must be recomputed when source files change, and relationship edges must be cleaned up on delete. Without a structured CRUD interface, users would need to manually maintain JSON consistency, leading to malformed manifests and dangling references.

## Acceptance Criteria

1. **Create record**: enforce `type.name` ID format (SD-CAT-001); compute content hash via `ContentHasher`; add to `AssetRegistry`
2. **Edit record**: update individual fields (tags, scope, stage name, source path); recompute content hash if source path changes
3. **Delete record**: remove from `AssetRegistry` and remove all relationship edges referencing the deleted record
4. **All three operations** wrapped as `IEditorCommand` subclasses for undo/redo (SD-ACE-004)
5. **Asset List panel**: filterable/sortable table showing ID, type, status, tags, scope for all records
6. **Record Editor panel**: form to view and edit a single selected record's fields
7. **Tag editing**: add/remove tags on a record; tags are StringCRC values
8. **Scope selection**: dropdown for `global` or `stage`; if `stage`, a text field for stage name
9. **Inline validation**: show error if ID format is invalid, if source path does not exist, or if ID already exists on create
10. **DiaAPI commands**: `create_record`, `update_record`, `delete_record` registered and functional
11. **Dirty flag**: all mutations update dirty tracking via `CommandHistory`

## API Design

### DiaAPI Commands

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `asset_catalogue.create_record` | `{ id, type, source_path, tags[], scope, stage? }` | `{ success: bool, content_hash?: string, error?: string }` | Create a new asset record with content hash |
| `asset_catalogue.update_record` | `{ id, ...fields }` | `{ success: bool, content_hash?: string, error?: string }` | Update fields on an existing record |
| `asset_catalogue.delete_record` | `{ id }` | `{ success: bool, removed_relationships: int, error?: string }` | Remove a record and all its relationship edges |

### C++ Types

```cpp
namespace Dia::AssetCatalogue::Editor
{
    // Create record command — captures all fields for undo
    class CreateRecordCommand : public Dia::Editor::IEditorCommand
    {
    public:
        CreateRecordCommand(Dia::AssetCatalogue::AssetRegistry& registry,
                            const Dia::AssetCatalogue::AssetRecord& record);

        void Execute() override;   // Add record to registry
        void Undo() override;      // Remove record from registry
        const char* GetDescription() const override;

    private:
        Dia::AssetCatalogue::AssetRegistry& mRegistry;
        Dia::AssetCatalogue::AssetRecord mRecord;  // Full copy for undo
    };

    // Update record command — captures old and new field values
    class UpdateRecordCommand : public Dia::Editor::IEditorCommand
    {
    public:
        UpdateRecordCommand(Dia::AssetCatalogue::AssetRegistry& registry,
                            const Dia::Core::StringCRC& recordId,
                            const Dia::AssetCatalogue::AssetRecord& oldValues,
                            const Dia::AssetCatalogue::AssetRecord& newValues);

        void Execute() override;   // Apply new values
        void Undo() override;      // Restore old values
        const char* GetDescription() const override;

    private:
        Dia::AssetCatalogue::AssetRegistry& mRegistry;
        Dia::Core::StringCRC mRecordId;
        Dia::AssetCatalogue::AssetRecord mOldValues;
        Dia::AssetCatalogue::AssetRecord mNewValues;
    };

    // Delete record command — captures record + relationships for undo
    class DeleteRecordCommand : public Dia::Editor::IEditorCommand
    {
    public:
        DeleteRecordCommand(Dia::AssetCatalogue::AssetRegistry& registry,
                            Dia::AssetCatalogue::RelationshipIndex& relationships,
                            const Dia::Core::StringCRC& recordId);

        void Execute() override;   // Remove record + all edges
        void Undo() override;      // Restore record + all edges
        const char* GetDescription() const override;

    private:
        Dia::AssetCatalogue::AssetRegistry& mRegistry;
        Dia::AssetCatalogue::RelationshipIndex& mRelationships;
        Dia::Core::StringCRC mRecordId;
        Dia::AssetCatalogue::AssetRecord mDeletedRecord;             // Snapshot for undo
        DynamicArrayC<RelationshipEdge, 16> mDeletedEdges;           // Snapshot for undo
    };
}
```

### CEF Panels

**Asset List Panel**: Filterable table rendered in CEF. Columns: ID, Type, Status, Tags, Scope. Filter by type dropdown, tag search, free-text search on ID. Click a row to select and open in Record Editor.

**Record Editor Panel**: Form with fields for the selected record. ID (read-only after create), Type (read-only), Source Path (text + browse button), Tags (chip list with add/remove), Scope (dropdown: global/stage), Stage Name (text, visible only when scope=stage). Save button commits the edit as an `UpdateRecordCommand`.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | CreateRecordCommand | Implement command: validate `type.name` format, compute content hash, add to registry; undo removes |
| 2 | UpdateRecordCommand | Implement command: snapshot old values, apply new values, recompute content hash if source path changed; undo restores old values |
| 3 | DeleteRecordCommand | Implement command: snapshot record + all relationship edges, remove from registry and RelationshipIndex; undo restores both |
| 4 | DiaAPI command handlers | Register `create_record`, `update_record`, `delete_record`; parse JSON params, instantiate commands, execute via CommandHistory |
| 5 | Asset List panel (CEF) | Implement filterable table: columns ID/Type/Status/Tags/Scope; filter by type, tag search, free-text; row click selects record |
| 6 | Record Editor panel (CEF) | Implement form: ID, Type, Source Path (with browse), Tags (chip list), Scope (dropdown), Stage Name; Save button creates UpdateRecordCommand |
| 7 | Create record dialog (CEF) | Dialog for new record: ID input with `type.name` validation, type selector, source path browse, initial tags/scope |
| 8 | Delete confirmation | Confirm dialog showing record ID and count of relationship edges that will be removed |
| 9 | Inline validation | Real-time validation in create/edit forms: ID format check, duplicate ID check, source path existence check |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `AssetRegistry` (add/remove/update records), `AssetRecord` (data type), `ContentHasher` (compute hashes), `RelationshipIndex` (edge cleanup on delete), `AssetTypeRegistry` (type validation) |
| DiaEditor | `IEditorCommand` + `CommandHistory` (undo/redo), `EditorModel` (observer notifications) |
| DiaAPI | Command registration for C++/CEF bridge |
| DiaUICEF | CEF rendering for Asset List and Record Editor panels |
| Manifest Load/Save (Feature 1) | Registry must be loaded before CRUD operations can be performed |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/Commands/CreateRecordCommand.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/CreateRecordCommand.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/UpdateRecordCommand.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/UpdateRecordCommand.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/DeleteRecordCommand.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/DeleteRecordCommand.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register CRUD commands |
| `Dia/DiaAssetCatalogueEditor/UI/AssetListPanel.html` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/RecordEditorPanel.html` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/CreateRecordDialog.html` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — asset IDs, type IDs, tag IDs, command names are all StringCRC |
| PD-004 | No STL in public APIs | **Compliant** — public API uses `const char*`, DiaCore containers (DynamicArrayC); CEF bridge uses JSON strings |
| AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| PD-005 | x64 Windows only | **Compliant** — no cross-platform concerns |
| PD-007 | C++20 required | **Compliant** |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — vcxproj inherits centralized settings |
| PD-009 | Generated output under Cluiche/out/ | **Compliant** — no generated output from CRUD itself; output path inherited from SD-ACE-005 |
| AD-001 | Module system with YAML frontmatter | **Compliant** — part of DiaAssetCatalogueEditor module |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — CreateRecordCommand validates `type.name` format before adding to registry |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **Compliant** — CRUD is the core authoring mechanism |
| SD-ACE-004 | All mutations are IEditorCommand | **Compliant** — Create, Update, Delete are each IEditorCommand subclasses with Execute/Undo |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **N/A** — CRUD does not produce output files; manifest save is Feature 1 |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **Compliant** — all three commands go through CommandHistory.ExecuteCommand() |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | **Compliant** — no DiaApplicationFlow types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **N/A** — no output files from CRUD |
| SED-021 | Per-plugin session context via .context.json | **N/A** — session context is Feature 1's responsibility |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Create | What happens if the user enters an ID that already exists? | Inline validation shows an error. The CreateRecordCommand is never executed. The DiaAPI command returns `{ success: false, error: "ID already exists" }`. |
| 2 | Delete | Does deleting a record also remove all relationship edges? | Yes. DeleteRecordCommand snapshots all forward and reverse edges for the record, removes them from RelationshipIndex, then removes the record. Undo restores both. |
| 3 | Undo | How does UpdateRecordCommand handle partial field updates? | The command snapshots the entire old record state and the entire new record state. Undo restores the full old state. This is simpler and safer than tracking individual field diffs. |
| 4 | Content Hash | When is the content hash recomputed? | On create (always) and on update if the source path field changed. ContentHasher reads the file at the source path and computes the hash. If the file is missing, the hash is set to empty and a warning is logged. |
| 5 | Validation | Is ID format validation done in C++ or JavaScript? | Both. JavaScript does real-time inline validation for immediate user feedback. C++ does authoritative validation in CreateRecordCommand before modifying the registry. The DiaAPI command returns an error if validation fails. |
| 6 | Tags | How are tags represented? | Tags are StringCRC values. The UI shows human-readable tag strings; the C++ layer converts to/from StringCRC. Tag editing uses a chip-list UI with autocomplete from existing tags in the registry. |
| 7 | Scope | Can the user set scope to `stage` without specifying a stage name? | No. If scope is `stage`, the stage name field is required. Inline validation prevents saving without it. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — **Asset List** (left sidebar) and **Record Editor** (center, "Asset Records" tab). Shows filterable asset list with type badges, scope indicators; record form with ID, type, source path, content hash, scope/stage selector, tag editor, and relationship list with add/remove. Use as the visual acceptance gate after implementation.

## Status

`Done`
