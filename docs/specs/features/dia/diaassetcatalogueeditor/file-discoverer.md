# Feature Spec: File Discoverer

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | **File Discoverer** | (this document) |

## Summary

The File Discoverer scans a configurable root directory (defaulting to `Assets/<AppName>/`), matches files against `AssetTypeRegistry` file patterns to suggest asset types, and presents only unregistered files to the user. The user selects discovered files and confirms a bulk-add, which creates asset records through `IEditorCommand` instances for full undo/redo. The discoverer never auto-adds records; the user must confirm every addition (SD-ACE-002).

## Problem

Asset catalogues grow alongside the project's content. When artists or designers add new files to the asset directories, those files need to be registered in the catalogue manifest before the pipeline can process them. Without a discoverer, users must manually create records for each new file, which is tedious and error-prone (wrong type, typos in path, missed files). The discoverer automates the "what's new?" question and streamlines bulk registration while keeping the user in control.

## Acceptance Criteria

1. Scan a configurable root directory recursively for files matching `AssetTypeRegistry` patterns
2. Default root is `Assets/<AppName>/` where `<AppName>` comes from the loaded manifest context
3. Match files against registered type patterns (e.g., `*.texture.png` maps to `texture` type)
4. Show only unregistered files — files already in the `AssetRegistry` are filtered out
5. Display discovered files in a list: file path, suggested type, file size, last modified date
6. User can select individual files or select-all for bulk operations
7. Bulk-add: selected files are confirmed by the user, then each creates an asset record
8. Each bulk-add record creation is an individual `IEditorCommand` grouped in a `CompoundCommand` (single undo step)
9. Auto-generate asset ID from file: `<type>.<filename_without_ext>` following SD-CAT-001 format
10. User can override the suggested type or ID before confirming
11. Folder asset discovery: `*.folder/` directories are treated as single assets (SD-CAT-013)
12. DiaAPI command `asset_catalogue.discover_files` is the C++/CEF bridge
13. Discovery results are transient (not persisted); re-scan to refresh

## API Design

### DiaAPI Commands

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `asset_catalogue.discover_files` | `{ root_path: string }` | `{ files: [{ path, suggested_type, size, modified }], count: int }` | Scan directory and return unregistered files with suggested types |

### C++ Types

```cpp
namespace Dia::AssetCatalogue::Editor
{
    struct DiscoveredFile
    {
        const char* relativePath;                   // Relative to scan root
        Dia::Core::StringCRC suggestedTypeId;       // From AssetTypeRegistry pattern match
        unsigned long long fileSize;                 // Bytes
        unsigned long long lastModified;             // Timestamp
    };

    class FileDiscoverer
    {
    public:
        // Scan root directory, match against type registry, filter out registered assets
        void Scan(const char* rootPath,
                  const Dia::AssetCatalogue::AssetTypeRegistry& typeRegistry,
                  const Dia::AssetCatalogue::AssetRegistry& assetRegistry);

        // Access results
        int GetDiscoveredCount() const;
        const DiscoveredFile& GetDiscoveredFile(int index) const;

        // Generate a default asset ID for a discovered file (type.name format)
        static void GenerateDefaultId(const DiscoveredFile& file,
                                      char* outBuffer, int bufferSize);

    private:
        DynamicArrayC<DiscoveredFile, 256> mDiscoveredFiles;
    };

    // Compound command for bulk-add — groups multiple CreateRecordCommands
    // Uses Dia::Editor::CompoundCommand (from undo-redo feature)
}
```

### CEF Panel

**File Discoverer Panel**: A two-section layout:
1. **Top bar**: Root path input with browse button, Scan button, result count
2. **Results list**: Table of discovered files with columns: checkbox (select), File Path, Suggested Type (editable dropdown), Suggested ID (editable text), Size, Modified. Multi-select with shift/ctrl-click. "Add Selected" button at bottom triggers bulk-add with confirmation dialog.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | FileDiscoverer — Scan | Implement recursive directory scan using Win32 `FindFirstFileW`/`FindNextFileW`; match files against `AssetTypeRegistry` patterns; filter out files already in `AssetRegistry` |
| 2 | FileDiscoverer — Folder assets | Detect `*.folder/` directories and treat them as single discovered assets (SD-CAT-013) |
| 3 | FileDiscoverer — ID generation | Implement `GenerateDefaultId()`: extract filename, strip extension, prepend type prefix, validate `type.name` format |
| 4 | DiaAPI command handler | Register `asset_catalogue.discover_files`; run scan, serialize results as JSON array |
| 5 | Bulk-add with CompoundCommand | On user confirm, create a `CompoundCommand` containing one `CreateRecordCommand` per selected file; execute via `CommandHistory` |
| 6 | File Discoverer panel (CEF) | Implement results table with checkboxes, editable type/ID fields, root path input, scan button, add-selected button |
| 7 | Confirmation dialog | Show summary before bulk-add: count of records to create, any ID conflicts or override warnings |
| 8 | Type override UI | Allow user to change suggested type via dropdown in results table before confirming add |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `AssetTypeRegistry` (file pattern matching), `AssetRegistry` (filter out registered files), `ContentHasher` (hash on record create) |
| DiaEditor | `IEditorCommand`, `CompoundCommand`, `CommandHistory` (undo/redo for bulk-add) |
| DiaAPI | Command registration for C++/CEF bridge |
| DiaUICEF | CEF rendering for File Discoverer panel |
| Asset Record CRUD (Feature 2) | Uses `CreateRecordCommand` for each discovered file added |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/FileDiscoverer.h` | Create |
| `Dia/DiaAssetCatalogueEditor/FileDiscoverer.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register discover_files command |
| `Dia/DiaAssetCatalogueEditor/UI/FileDiscovererPanel.html` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/BulkAddConfirmDialog.html` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — suggested type IDs and generated asset IDs are StringCRC; command name is StringCRC |
| PD-004 | No STL in public APIs | **Compliant** — public API uses `const char*` for paths, DiaCore containers (DynamicArrayC) for results |
| AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| PD-005 | x64 Windows only | **Compliant** — directory scan uses Win32 API (`FindFirstFileW`/`FindNextFileW`) |
| PD-007 | C++20 required | **Compliant** |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — vcxproj inherits centralized settings |
| PD-009 | Generated output under Cluiche/out/ | **N/A** — discovery results are transient in-memory; no files written |
| AD-001 | Module system with YAML frontmatter | **Compliant** — part of DiaAssetCatalogueEditor module |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — `GenerateDefaultId()` produces `type.name` format; validated before creating record |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **Compliant** — discoverer creates records through the editor's CRUD commands |
| SD-ACE-002 | Discoverer suggests, user confirms | **Compliant** — discoverer presents candidates; user selects and explicitly confirms before any records are created |
| SD-ACE-004 | All mutations are IEditorCommand | **Compliant** — bulk-add uses CompoundCommand containing CreateRecordCommands, all executed via CommandHistory |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **N/A** — no file output from discovery |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **Compliant** — CompoundCommand allows undoing entire bulk-add in one step |
| SED-015 | DiaEditor is pure C++ library, no DiaApplication dependency | **Compliant** — no DiaApplication types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **N/A** — no file output |
| SED-021 | Per-plugin session context via .context.json | **N/A** — discovery results are transient; root path could be persisted in context by Feature 1 in the future |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Discovery | Does the discoverer show files that are already registered? | No. Already-registered files (matched by source path) are filtered out. The list is a "what's missing?" tool, not a full file browser. |
| 2 | Bulk-add | Can the user undo a bulk-add? | Yes. All records from a single bulk-add are grouped in a `CompoundCommand`, so a single Ctrl+Z undoes the entire batch. |
| 3 | Performance | What if the asset directory has thousands of files? | The scan runs synchronously on the first call but results are cached in `mDiscoveredFiles`. For very large directories, a future enhancement could run the scan on a background thread with a progress indicator. The current design handles typical project sizes (hundreds to low thousands of files). |
| 4 | Type matching | What happens if a file does not match any registered type pattern? | The file is still shown in the results with `suggestedTypeId` set to an "unknown" sentinel. The user can manually assign a type before adding. Files with unknown type cannot be added without a type override. |
| 5 | Folder assets | How are `*.folder/` directories handled? | Per SD-CAT-013, directories ending in `.folder/` are treated as single assets. The scanner detects these directories and adds them as a single entry with the `folder` type, rather than recursing into them. |
| 6 | ID conflicts | What if the auto-generated ID conflicts with an existing record? | The UI highlights the conflicting ID in red. The user must change the ID before the file can be added. The confirmation dialog also shows a warning count for any unresolved conflicts. |
| 7 | Re-scan | Are discovery results persisted? | No. Results are transient and held in memory only. The user re-scans to get fresh results. This avoids stale data if files are added/removed outside the editor. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — Click **"File Discoverer"** tab. Shows root path input, scan button, unregistered file list with checkboxes, suggested type badges, and "Add Selected" bulk action. Use as the visual acceptance gate after implementation.

## Status

`Done`
