# Feature Spec: Validation Panel

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | Validation Panel | (this document) |

## Summary

A dockable CEF panel that runs catalogue validation on demand and displays per-record errors. The user triggers validation explicitly via a "Validate" button, which calls `asset_catalogue.validate`. Results are displayed as a grouped, sortable error list with click-to-navigate to the offending record in the Record Editor.

## Problem

Without a dedicated validation view, the user has no way to check catalogue integrity before handing the manifest to the asset pipeline. Errors such as dangling references, duplicate IDs, missing source files, and schema constraint violations would only surface at pipeline build time — far from the authoring context. A validation panel surfaces these issues immediately in the editor, at the point of authoring.

## Acceptance Criteria

1. "Validate" button triggers the `asset_catalogue.validate` DiaAPI command
2. Validation results displayed as a table: asset ID, error type, severity, message
3. Error types include: missing source file, dangling reference, duplicate ID, schema constraint violation
4. Results grouped by severity: errors first, then warnings
5. Clicking an error row navigates to the corresponding record in the Record Editor
6. Validation is explicit (user-triggered only) — not automatic on save or on record change
7. "Re-validate" clears previous results before displaying new ones
8. Error count summary shown at the top of the panel (e.g., "3 errors, 2 warnings")
9. Empty state: "No validation results. Click Validate to check the catalogue."
10. Clean state: "Validation passed — no errors or warnings found."

## API Design

### DiaAPI Commands Used

| Command | Usage |
|---------|-------|
| `asset_catalogue.validate { }` | Trigger full catalogue validation; returns per-record error list |

### C++ Types Involved

```cpp
namespace Dia::AssetCatalogue::Editor
{
    // Command handler registered in DiaAssetCatalogueEditor::OnLoad
    // Delegates to AssetRegistry::Validate() (DiaAssetCatalogue)

    // Response JSON shape for validate:
    // {
    //   "errors": [
    //     {
    //       "assetId": "texture.hero_diffuse",
    //       "errorType": "missing_source_file",
    //       "severity": "error",
    //       "message": "Source file Assets/Textures/hero_diffuse.png not found"
    //     },
    //     {
    //       "assetId": "entity.player",
    //       "errorType": "dangling_reference",
    //       "severity": "error",
    //       "message": "Forward ref to 'texture.deleted_asset' — target does not exist"
    //     }
    //   ],
    //   "summary": { "errorCount": 3, "warningCount": 2 }
    // }
}
```

All validation logic (file existence checks, reference resolution, schema constraint checks, duplicate ID detection) lives in DiaAssetCatalogue. The editor command handler calls the API and serializes results to JSON.

### CEF Panel Description

**Validation Panel** — a dockable panel containing:
- A "Validate" button in the toolbar
- An error/warning count summary bar
- A scrollable table with columns: Severity icon, Asset ID, Error Type, Message
- Rows sorted by severity (errors before warnings), then alphabetically by asset ID
- Click handler on rows that dispatches navigation to Record Editor

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement validate command handler | Register `asset_catalogue.validate` in DiaAPI; call `AssetRegistry::Validate()`, serialize per-record errors to JSON |
| 2 | Create Validation Panel HTML/JS | Build CEF panel with Validate button, summary bar, error table, empty/clean state messages |
| 3 | Wire click-to-navigate | Click handler on table row dispatches message to open the asset's record in Record Editor |
| 4 | Handle re-validate | Clear previous results, show loading indicator, then display new results |
| 5 | Unit tests for validate command handler | Test JSON serialization of validation results with known registry states (missing file, dangling ref, duplicate ID, clean) |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `AssetRegistry::Validate()` — all validation logic (file checks, ref resolution, schema constraints, duplicate detection) |
| DiaAssetCatalogueEditor (Feature 2) | Record Editor as navigation target for click-to-navigate |
| DiaEditor | `IEditorPlugin` panel hosting, CEF rendering |
| DiaUICEF | CEF browser instance for rendering the panel |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/Commands/ValidateHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/ValidateHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/validation-panel/index.html` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/validation-panel/validation.js` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/validation-panel/validation.css` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register validation panel and command handler |
| `Cluiche/UnitTests/DiaAssetCatalogueEditor/ValidationPanelTests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — asset IDs are StringCRC in C++; serialized as `type.name` strings in JSON response |
| PD-004 / AD-002 | No STL in public APIs | **Compliant** — command handler uses `const char*` and DiaCore containers; JSON bridge uses `Json::Value` |
| PD-005 | x64 Windows only | **Compliant** — no cross-platform considerations |
| PD-007 | C++20 required | **Compliant** — all new C++ files compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — no build setting overrides |
| PD-009 | Generated output under Cluiche/out/ | **Compliant** — validation results are ephemeral UI state, not written to disk |
| AD-001 | Module system with YAML frontmatter | **Compliant** — covered by parent module doc |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all C++ code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — error rows display `type.name` composite IDs |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **N/A** — validation is read-only inspection, no manifest mutation |
| SD-ACE-004 | All mutations are IEditorCommand | **N/A** — no mutations in this feature |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **Compliant** — no file output |
| SD-ACE-007 | Rules engine logic in DiaAssetCatalogue, editor UI only | **Compliant** — all validation logic is in DiaAssetCatalogue; editor only displays results |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **N/A** — read-only feature, no undoable actions |
| SED-015 | DiaEditor is pure C++ library, no DiaApplication dependency | **Compliant** — no DiaApplication types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **Compliant** — no file output |
| SED-021 | Per-plugin session context via .context.json | **N/A** — validation results are transient, not persisted in session |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Trigger | Should validation run automatically on save? | No. System spec AI Review Q4 explicitly states validation is explicit only. Auto-validate on save can be added as a user preference in a future iteration. |
| 2 | Performance | How long can validation take for a large catalogue (1000+ records)? | Validation runs synchronously on the main thread via DiaAPI. For very large catalogues, file existence checks dominate. If this becomes a bottleneck, validation can be moved to an async DiaAPI command in a future iteration. The UI shows a "Validating..." indicator during execution. |
| 3 | Staleness | Are results invalidated when the user edits a record after validating? | No. Results are a snapshot from the last explicit validate run. The user must re-validate to see updated results. A subtle "results may be stale" indicator could be added if any mutation has occurred since the last validate. |
| 4 | Error types | Is the error type list extensible? | Yes. DiaAssetCatalogue defines validation rules; the editor displays whatever error types come back in the JSON. The four listed types (missing source file, dangling reference, duplicate ID, schema constraint violation) are the initial set. New validation rules in DiaAssetCatalogue automatically surface in the panel. |
| 5 | Navigation | What if the user clicks a row for a record that no longer exists? | The Record Editor receives the asset ID and shows a "record not found" inline error. The validation results remain visible — the user can re-validate to get a fresh view. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — Click **"Validation"** tab. Shows error/warning summary bar, per-record error rows with severity icons, asset ID links, and error messages. Click rows to navigate to record. Use as the visual acceptance gate after implementation.

## Status

`Done`
