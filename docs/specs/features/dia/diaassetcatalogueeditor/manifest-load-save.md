# Feature Spec: Manifest Load/Save

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | **Manifest Load/Save** | (this document) |

## Summary

Manifest Load/Save enables opening an existing `assets.catalogue.json` from disk into the editor's AssetRegistry, saving the registry state back to disk with dirty tracking, and creating new empty manifests from scratch. Session context persists the last-opened manifest path via `.context.json` so the editor can restore state across sessions (SED-021).

## Problem

Without a load/save feature the editor has no way to read or write the catalogue manifest file that is the entire purpose of DiaAssetCatalogueEditor. Every other feature (CRUD, discoverer, relationships) depends on having a loaded registry to operate on. The user also needs dirty tracking to avoid accidental data loss, and session persistence to avoid re-navigating to the same manifest every time the editor opens.

## Acceptance Criteria

1. Load an existing `assets.catalogue.json` via `CatalogueManifestSerializer` into `AssetRegistry`
2. Create a new empty manifest from scratch (empty registry, no file on disk yet)
3. Save the current registry state back to disk via `CatalogueManifestSerializer`
4. Dirty flag tracks whether the registry has unsaved changes
5. Prompt the user on close/load if there are unsaved changes ("Save / Discard / Cancel")
6. Loading a manifest resets the `CommandHistory` (undo/redo stack cleared)
7. Saving does not affect the `CommandHistory` (save is not an undoable operation)
8. Save marks the `CommandHistory` save point so dirty flag tracks correctly with undo/redo
9. Session context persists the last-opened manifest path in `.context.json` (SED-021)
10. On editor startup, if `.context.json` contains a valid last path, offer to re-open it
11. Output written to `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/` per SD-ACE-005
12. DiaAPI commands `asset_catalogue.load_manifest` and `asset_catalogue.save_manifest` are the C++/CEF bridge

## API Design

### DiaAPI Commands

| Command | Parameters | Returns | Description |
|---------|-----------|---------|-------------|
| `asset_catalogue.load_manifest` | `{ path: string }` | `{ success: bool, record_count: int, error?: string }` | Deserialize manifest from disk into AssetRegistry |
| `asset_catalogue.save_manifest` | `{ path: string }` | `{ success: bool, error?: string }` | Serialize current AssetRegistry state to disk |

### C++ Types

```cpp
namespace Dia::AssetCatalogue::Editor
{
    // IEditorCommand for load (clears history, not undoable itself)
    // Save is not a command — it is a direct operation that marks save point

    class ManifestLoadHandler
    {
    public:
        bool Load(const char* path,
                  Dia::AssetCatalogue::AssetRegistry& registry,
                  Dia::AssetCatalogue::CatalogueManifestSerializer& serializer,
                  Dia::Editor::CommandHistory& history);

        bool Save(const char* path,
                  const Dia::AssetCatalogue::AssetRegistry& registry,
                  Dia::AssetCatalogue::CatalogueManifestSerializer& serializer,
                  Dia::Editor::CommandHistory& history);

        void NewManifest(Dia::AssetCatalogue::AssetRegistry& registry,
                         Dia::Editor::CommandHistory& history);

        bool IsDirty() const;
    };

    // Session context — persisted to .context.json
    struct SessionContext
    {
        const char* lastManifestPath;   // Last-opened manifest file path
        // Future: active panel, selected record, scroll position
    };
}
```

### CEF Panel

No dedicated panel. Load/Save is accessed via:
- **File menu**: New Manifest, Open Manifest, Save, Save As
- **Toolbar**: Save button with dirty indicator
- **Startup dialog**: Re-open last manifest from session context

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | ManifestLoadHandler — Load | Implement `Load()`: call `CatalogueManifestSerializer::Deserialize()`, populate `AssetRegistry`, clear `CommandHistory`, update session context |
| 2 | ManifestLoadHandler — Save | Implement `Save()`: call `CatalogueManifestSerializer::Serialize()`, mark `CommandHistory` save point, update session context |
| 3 | ManifestLoadHandler — New | Implement `NewManifest()`: clear `AssetRegistry`, clear `CommandHistory`, clear current path |
| 4 | SessionContext persistence | Read/write `.context.json` in `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/` with last manifest path |
| 5 | DiaAPI command registration | Register `asset_catalogue.load_manifest` and `asset_catalogue.save_manifest` commands in `OnLoad()` |
| 6 | Dirty tracking integration | Wire dirty flag to `CommandHistory::IsAtSavePoint()`, update UI on change |
| 7 | Unsaved changes prompt | On close or load-new, check dirty flag and prompt Save/Discard/Cancel via CEF dialog |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `AssetRegistry`, `CatalogueManifestSerializer` — all serialization and registry logic |
| DiaEditor | `IEditorPlugin` lifecycle, `CommandHistory` (clear on load, mark save point on save), `EditorModel` |
| DiaAPI | Command registration and execution for C++/CEF bridge |
| DiaUICEF | CEF rendering for file dialogs and dirty indicator |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/ManifestLoadHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/ManifestLoadHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/SessionContext.h` | Create |
| `Dia/DiaAssetCatalogueEditor/SessionContext.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register load/save commands |
| `Dia/DiaAssetCatalogueEditor/UI/index.html` | Modify — add file menu, toolbar save button |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — command names registered as StringCRC; asset IDs in registry are StringCRC |
| PD-004 | No STL in public APIs | **Compliant** — public API uses `const char*` for paths; DiaCore containers internally |
| AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| PD-005 | x64 Windows only | **Compliant** — no cross-platform code; file dialogs use Win32 API |
| PD-007 | C++20 required | **Compliant** — uses C++20 features as available |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — vcxproj inherits centralized settings |
| PD-009 | Generated output under Cluiche/out/ | **Compliant** — session context written to `Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/` |
| AD-001 | Module system with YAML frontmatter | **Compliant** — DiaAssetCatalogueEditor has its own architecture module doc |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **N/A** — load/save does not create IDs; format enforced by CRUD feature |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **Compliant** — this feature IS the manifest authoring entry point |
| SD-ACE-004 | All mutations are IEditorCommand | **Compliant** — load resets command history; save marks save point; neither is a mutation command itself |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **Compliant** — session context written to this path |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **Compliant** — load clears history; save marks save point; dirty flag tracks via IsAtSavePoint() |
| SED-015 | DiaEditor is pure C++ library, no DiaApplication dependency | **Compliant** — no DiaApplication types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **Compliant** — same as SD-ACE-005 |
| SED-021 | Per-plugin session context via .context.json | **Compliant** — SessionContext reads/writes `.context.json` with last manifest path |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Load | Can the editor create a new manifest from scratch, or only load existing files? | Both. `NewManifest()` creates an empty registry with no file on disk. Save writes a new file. Load opens an existing one. |
| 2 | Undo/Redo | Does loading a manifest affect the undo/redo history? | Yes. Loading resets (clears) the command history entirely. You cannot undo a load. |
| 3 | Undo/Redo | Does saving affect the undo/redo history? | No. Save marks the current position as the save point but does not modify the stack. Undo/redo continues to work normally after save. |
| 4 | Session | What happens if the `.context.json` last path points to a file that no longer exists? | The editor shows a warning ("Last manifest not found") and falls back to the empty/new state. The stale path is cleared from context. |
| 5 | Concurrency | Can two editor instances edit the same manifest simultaneously? | No file locking is implemented. This is a single-user build-time tool. If two instances edit the same file, last-save-wins. A future enhancement could add advisory locking. |
| 6 | Error handling | What happens if deserialization fails (corrupt JSON, schema mismatch)? | `Load()` returns false with an error string. The registry is left in its previous state (not partially loaded). The error is displayed in the Output Console. |
| 7 | Dirty prompt | What triggers the unsaved-changes prompt? | Three actions: (1) loading a different manifest, (2) creating a new manifest, (3) closing/unloading the plugin. Each checks the dirty flag first. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — **Toolbar** (top bar). Shows Open/Save/New buttons, manifest path display, dirty indicator (*), and Ctrl+S target. Use as the visual acceptance gate after implementation.

## Status

`Approved`
