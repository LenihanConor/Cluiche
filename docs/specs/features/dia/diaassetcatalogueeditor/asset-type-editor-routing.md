# Feature Spec: Asset Type Editor Routing

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | Asset Type Editor Routing | (this document) |

## Summary

Provides an `AssetTypeEditorRegistry` that maps asset type IDs to editor plugin IDs, and an `open_asset` DiaAPI command that routes an asset to its registered editor or falls back to the OS default application. This enables extensible asset editing — game-specific editors register themselves at startup, while all built-in types use OS defaults.

## Problem

When a user wants to edit an asset's content (not its catalogue metadata), they need to open it in an appropriate editor. Without a routing mechanism, the catalogue editor would either have to embed content editors (violating separation of concerns) or force users to manually find and open files. The routing system lets each asset type declare its preferred editor, with OS default as a universal fallback, and makes the system extensible for future custom editors without modifying DiaAssetCatalogueEditor.

## Acceptance Criteria

1. `AssetTypeEditorRegistry` class maps `StringCRC` type IDs to `StringCRC` editor plugin IDs
2. `RegisterTypeEditor(typeId, editorPluginId)` registers a type-to-editor mapping
3. `FindEditorForType(typeId)` returns the registered editor plugin ID, or `nullptr` if none
4. `asset_catalogue.open_asset { id }` DiaAPI command implemented
5. If a registered editor exists for the asset's type: activate that editor plugin with the asset's source path
6. If no registered editor: open the asset's source path via OS default (`ShellExecuteEx` on Windows)
7. `folder` type assets open the folder in Windows File Explorer via `ShellExecuteEx`
8. All current built-in types (entity, config, stage, folder) use OS default — no custom editors registered at startup
9. Game-specific editors can register themselves via `RegisterTypeEditor` during plugin `OnLoad`
10. If `open_asset` is called for an asset with no source path, log a warning and do nothing
11. If `ShellExecuteEx` fails (no OS association), log a warning to the output console — no crash

## API Design

### DiaAPI Commands Used

| Command | Usage |
|---------|-------|
| `asset_catalogue.open_asset { id }` | Look up asset record, resolve editor, open in registered editor or OS default |

### C++ Types Involved

```cpp
namespace Dia::AssetCatalogue::Editor
{
    class AssetTypeEditorRegistry
    {
    public:
        // Register a type-specific editor plugin
        void RegisterTypeEditor(const Dia::Core::StringCRC& typeId,
                                const Dia::Core::StringCRC& editorPluginId);

        // Find the editor plugin for a type; returns nullptr if OS default should be used
        const Dia::Core::StringCRC* FindEditorForType(
            const Dia::Core::StringCRC& typeId) const;

    private:
        // Maps typeId -> editorPluginId
        Dia::Core::Containers::HashTable<Dia::Core::StringCRC,
                                          Dia::Core::StringCRC, 32> mEditorMap;
    };
}
```

### Open Asset Flow

```
open_asset { id }
  -> AssetRegistry::FindRecord(id)
  -> AssetTypeEditorRegistry::FindEditorForType(record.typeId)
  -> if editor found:
       EditorPluginRegistry::ActivatePlugin(editorPluginId, record.sourcePath)
  -> if no editor:
       if record.typeId == "folder":
         ShellExecuteEx("explore", record.sourcePath)
       else:
         ShellExecuteEx("open", record.sourcePath)
  -> if no source path:
       Log warning, return
```

### CEF Panel Description

No dedicated panel. The `open_asset` action is triggered from the Asset List (double-click or "Open" button) and from the Record Editor ("Open in Editor" button). These panels invoke the DiaAPI command; this feature provides the command handler and routing logic.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement AssetTypeEditorRegistry | Create the class with `RegisterTypeEditor`, `FindEditorForType`, backed by a HashTable |
| 2 | Implement open_asset command handler | Register `asset_catalogue.open_asset` in DiaAPI; look up record, resolve editor, dispatch to plugin or OS default |
| 3 | Implement ShellExecuteEx fallback | Call `ShellExecuteExW` with "open" verb for files, "explore" verb for folders; handle failure with warning log |
| 4 | Wire open action to Asset List UI | Double-click on Asset List row and "Open" button both invoke `asset_catalogue.open_asset` |
| 5 | Wire open action to Record Editor UI | "Open in Editor" button in Record Editor invokes `asset_catalogue.open_asset` for the current record |
| 6 | Unit tests for AssetTypeEditorRegistry | Test RegisterTypeEditor, FindEditorForType (found/not found), and override behavior |
| 7 | Unit tests for open_asset handler | Test routing to registered editor, OS default fallback, folder type, no source path warning |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `AssetRegistry::FindRecord()` to look up asset type and source path |
| DiaEditor | `EditorPluginRegistry::ActivatePlugin()` to open a registered editor plugin with a file path |
| DiaAssetCatalogueEditor (Feature 2) | Asset List and Record Editor UI trigger the open_asset action |
| Windows API | `ShellExecuteExW` for OS default file/folder opening |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/AssetTypeEditorRegistry.h` | Create |
| `Dia/DiaAssetCatalogueEditor/AssetTypeEditorRegistry.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/OpenAssetHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/OpenAssetHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.h` | Modify — add `AssetTypeEditorRegistry` member |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register command handler, initialize registry |
| `Dia/DiaAssetCatalogueEditor/UI/asset-list/asset-list.js` | Modify — add double-click and Open button handler |
| `Dia/DiaAssetCatalogueEditor/UI/record-editor/record-editor.js` | Modify — add "Open in Editor" button |
| `Cluiche/UnitTests/DiaAssetCatalogueEditor/AssetTypeEditorRegistryTests.cpp` | Create |
| `Cluiche/UnitTests/DiaAssetCatalogueEditor/OpenAssetHandlerTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — type IDs and editor plugin IDs are `StringCRC`; `AssetTypeEditorRegistry` keys on `StringCRC` |
| PD-004 / AD-002 | No STL in public APIs | **Compliant** — uses DiaCore `HashTable`, `StringCRC`, `const char*`; no STL types in public interface |
| PD-005 | x64 Windows only | **Compliant** — `ShellExecuteExW` is Windows-only; no cross-platform abstraction needed |
| PD-007 | C++20 required | **Compliant** — all new C++ files compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — no build setting overrides |
| PD-009 | Generated output under Cluiche/out/ | **N/A** — no generated output from this feature |
| AD-001 | Module system with YAML frontmatter | **Compliant** — covered by parent module doc |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all C++ code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — open_asset receives composite asset ID, extracts type for registry lookup |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **N/A** — open_asset does not modify the manifest |
| SD-ACE-003 | OS default unless type-specific editor registered | **Compliant** — this feature IS the implementation of SD-ACE-003 |
| SD-ACE-004 | All mutations are IEditorCommand | **N/A** — open_asset is a read-only action (opens external program) |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **Compliant** — no file output |
| SD-ACE-007 | Rules engine logic in DiaAssetCatalogue, editor UI only | **N/A** — not related to rules engine |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **N/A** — open_asset is not undoable (external side effect) |
| SED-015 | DiaEditor is pure C++ library, no DiaApplication dependency | **Compliant** — no DiaApplication types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **Compliant** — no file output |
| SED-021 | Per-plugin session context via .context.json | **N/A** — no session state for routing |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Fallback | What if ShellExecuteEx fails because the file type has no OS association? | Log a warning to the output console: "No application associated with file type '.xyz'. Cannot open asset." No crash, no dialog box. The user must install or associate an application manually. Per system spec AI Review Q3. |
| 2 | Registration | Can a game override a built-in registration? | Yes. `RegisterTypeEditor` overwrites any previous mapping for the same type ID. Game-specific editors register during their `OnLoad`, which runs after built-in registrations (though currently all built-ins use OS default, so there is nothing to override). |
| 3 | Source path | What if the asset's source path is relative? | Source paths in the catalogue are relative to the `Assets/<AppName>/` root. The open_asset handler resolves the full path by prepending the configured asset root before passing to `ShellExecuteExW` or to a plugin. |
| 4 | Plugin activation | How does ActivatePlugin pass the file path to the target editor? | `EditorPluginRegistry::ActivatePlugin(pluginId, filePath)` calls the target plugin's `OnActivate(filePath)` method (part of `IEditorPlugin`). The plugin then loads/displays the file in its own panel. |
| 5 | Folder type | Why does folder use "explore" verb instead of "open"? | `ShellExecuteExW` with "explore" opens Windows File Explorer at that folder. Using "open" on a folder path also works but may behave differently depending on OS settings. "explore" is the explicit, predictable choice. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — **Quick Actions** (right sidebar). Shows "Open in External Editor" button which triggers AssetTypeEditorRegistry lookup then ShellExecuteEx fallback. Use as the visual acceptance gate after implementation.

## Status

`Approved`
