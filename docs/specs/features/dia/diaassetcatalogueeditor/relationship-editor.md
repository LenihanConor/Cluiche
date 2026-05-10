# Feature Spec: Relationship Editor

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | **Relationship Editor** | (this document) |

## Summary

The Relationship Editor allows users to add and remove relationship edges between asset records. It displays forward references (assets this record depends on) and reverse references (assets that depend on this record) for the currently selected record. All mutations are `IEditorCommand` instances for full undo/redo support. The editor validates that target records exist in the registry before allowing edge creation.

## Problem

Assets in a catalogue are not isolated — they reference each other (a stage contains entities, an entity uses textures, a config references other configs). These relationships drive pipeline build ordering, scope computation, and dependency validation. Without a dedicated relationship editor, users would need to hand-edit JSON arrays of relationship entries, with no validation that referenced assets actually exist and no clean way to see the reverse dependency picture (who depends on me?).

## Acceptance Criteria

1. Add a relationship edge: specify `fromId`, relationship `type` (contains, uses), and `toId`
2. Remove a relationship edge by selecting it from the list
3. Display forward references for the selected record (edges where this record is `fromId`)
4. Display reverse references for the selected record (edges where this record is `toId`)
5. Validate that `toId` exists in the registry before adding an edge; show error if not found
6. Validate that the edge does not already exist (no duplicate edges)
7. Prevent self-referential edges (`fromId` must not equal `toId`)
8. All add/remove operations wrapped as `IEditorCommand` subclasses (SD-ACE-004)
9. DiaAPI commands: `add_relationship`, `remove_relationship`, `get_forward_refs`, `get_reverse_refs`
10. Relationship type selector: dropdown with available types (contains, uses)
11. Target record selector: searchable dropdown or autocomplete from existing registry IDs
12. Click a reference to navigate to that record in the Record Editor panel

## API Design

### DiaAPI Commands

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `asset_catalogue.add_relationship` | `{ fromId, type, toId }` | `{ success: bool, error?: string }` | Add a relationship edge |
| `asset_catalogue.remove_relationship` | `{ fromId, type, toId }` | `{ success: bool, error?: string }` | Remove a relationship edge |
| `asset_catalogue.get_forward_refs` | `{ id }` | `{ refs: [{ toId, type }] }` | Get all forward edges from a record |
| `asset_catalogue.get_reverse_refs` | `{ id }` | `{ refs: [{ fromId, type }] }` | Get all reverse edges to a record |

### C++ Types

```cpp
namespace Dia::AssetCatalogue::Editor
{
    // Add relationship command
    class AddRelationshipCommand : public Dia::Editor::IEditorCommand
    {
    public:
        AddRelationshipCommand(Dia::AssetCatalogue::RelationshipIndex& relationships,
                               const Dia::Core::StringCRC& fromId,
                               const Dia::Core::StringCRC& type,
                               const Dia::Core::StringCRC& toId);

        void Execute() override;   // Add edge to RelationshipIndex
        void Undo() override;      // Remove edge from RelationshipIndex
        const char* GetDescription() const override;

    private:
        Dia::AssetCatalogue::RelationshipIndex& mRelationships;
        Dia::Core::StringCRC mFromId;
        Dia::Core::StringCRC mType;
        Dia::Core::StringCRC mToId;
    };

    // Remove relationship command
    class RemoveRelationshipCommand : public Dia::Editor::IEditorCommand
    {
    public:
        RemoveRelationshipCommand(Dia::AssetCatalogue::RelationshipIndex& relationships,
                                  const Dia::Core::StringCRC& fromId,
                                  const Dia::Core::StringCRC& type,
                                  const Dia::Core::StringCRC& toId);

        void Execute() override;   // Remove edge from RelationshipIndex
        void Undo() override;      // Re-add edge to RelationshipIndex
        const char* GetDescription() const override;

    private:
        Dia::AssetCatalogue::RelationshipIndex& mRelationships;
        Dia::Core::StringCRC mFromId;
        Dia::Core::StringCRC mType;
        Dia::Core::StringCRC mToId;
    };
}
```

### CEF Panel

**Relationship section** within the Record Editor panel (not a standalone panel). Two collapsible lists:

1. **Forward References**: Table of edges where this record is the source. Columns: Target ID (clickable link), Relationship Type. Each row has a remove button.
2. **Reverse References**: Table of edges where this record is the target. Columns: Source ID (clickable link), Relationship Type. Read-only (removal must be done from the source record's forward refs).
3. **Add Relationship form**: Inline form below forward refs with type dropdown (contains/uses), target ID autocomplete, and Add button.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | AddRelationshipCommand | Implement command: validate target exists, validate no duplicate, validate no self-ref, add edge to RelationshipIndex; undo removes edge |
| 2 | RemoveRelationshipCommand | Implement command: remove edge from RelationshipIndex; undo re-adds edge |
| 3 | DiaAPI query handlers | Register `get_forward_refs` and `get_reverse_refs`; query RelationshipIndex, serialize results as JSON |
| 4 | DiaAPI mutation handlers | Register `add_relationship` and `remove_relationship`; parse params, validate, instantiate commands, execute via CommandHistory |
| 5 | Relationship UI section (CEF) | Implement forward/reverse reference lists in Record Editor panel with remove buttons and clickable navigation links |
| 6 | Add Relationship form (CEF) | Implement inline form with type dropdown, target ID autocomplete from registry, and Add button with validation feedback |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `RelationshipIndex` (add/remove/query edges), `AssetRegistry` (validate target exists) |
| DiaEditor | `IEditorCommand` + `CommandHistory` (undo/redo) |
| DiaAPI | Command registration for C++/CEF bridge |
| DiaUICEF | CEF rendering for relationship UI section |
| Asset Record CRUD (Feature 2) | Record Editor panel hosts the relationship section; record selection drives which relationships are displayed |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/Commands/AddRelationshipCommand.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/AddRelationshipCommand.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/RemoveRelationshipCommand.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/RemoveRelationshipCommand.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register relationship commands |
| `Dia/DiaAssetCatalogueEditor/UI/RecordEditorPanel.html` | Modify — add relationship section with forward/reverse lists and add form |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — fromId, toId, relationship type, command names are all StringCRC |
| PD-004 | No STL in public APIs | **Compliant** — public API uses StringCRC, `const char*`; DiaCore containers internally |
| AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| PD-005 | x64 Windows only | **Compliant** — no cross-platform concerns |
| PD-007 | C++20 required | **Compliant** |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — vcxproj inherits centralized settings |
| PD-009 | Generated output under Cluiche/out/ | **N/A** — relationship editing does not produce output files |
| AD-001 | Module system with YAML frontmatter | **Compliant** — part of DiaAssetCatalogueEditor module |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — fromId and toId are validated as existing registry entries (which already have valid IDs) |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **Compliant** — relationship edges are part of the catalogue manifest |
| SD-ACE-004 | All mutations are IEditorCommand | **Compliant** — AddRelationshipCommand and RemoveRelationshipCommand are both IEditorCommand subclasses |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **N/A** — no file output from relationship editing |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **Compliant** — both commands go through CommandHistory.ExecuteCommand() |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | **Compliant** — no DiaApplicationFlow types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **N/A** — no file output |
| SED-021 | Per-plugin session context via .context.json | **N/A** — no session state specific to relationships |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Validation | What happens if the target record does not exist? | The `add_relationship` command returns `{ success: false, error: "Target record not found" }`. The UI shows an inline error. The command is not executed. |
| 2 | Validation | Can you create duplicate edges? | No. AddRelationshipCommand checks for an existing edge with the same (fromId, type, toId) triple. If found, returns an error. |
| 3 | Validation | Can a record reference itself? | No. Self-referential edges (`fromId == toId`) are rejected with an error message. |
| 4 | Reverse refs | Can the user remove a reverse reference from the target record's view? | No. Reverse references are read-only in the target's view. To remove, the user must navigate to the source record (via the clickable link) and remove the forward reference there. This maintains a clear ownership model. |
| 5 | Relationship types | Are relationship types extensible? | Currently fixed to `contains` and `uses` as defined by DiaAssetCatalogue. If new types are added to the catalogue's schema, the dropdown in the UI would need to be updated. A future enhancement could query available types dynamically from the type registry. |
| 6 | Cascading | Does removing a relationship affect scope computation? | Relationship changes may affect automatic scope computation (e.g., if an asset is no longer referenced by any stage, it might become global). However, scope recomputation is DiaAssetCatalogue's responsibility and is triggered separately (e.g., via validate or rules engine). The editor just adds/removes edges. |
| 7 | Navigation | How does clicking a reference navigate to the target record? | Clicking a reference ID in the forward or reverse list selects that record in the Asset List panel and opens it in the Record Editor panel. This is a standard cross-panel navigation via EditorModel observer notification. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — **Record Editor** (center, "Asset Records" tab), **Relationships** section at bottom. Shows forward/reverse ref tabs, relationship items with type label and clickable target ID, remove button, and "+ Add Relationship" action. Use as the visual acceptance gate after implementation.

## Status

`Done`
