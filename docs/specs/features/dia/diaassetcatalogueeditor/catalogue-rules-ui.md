# Feature Spec: Catalogue Rules UI

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogueEditor | @docs/specs/systems/dia/diaassetcatalogueeditor.md |
| Feature | Catalogue Rules UI | (this document) |

## Summary

Presentation layer for the Catalogue Rules Engine (DiaAssetCatalogue Feature 4). Provides a CEF panel to load `assets.rules.json`, view loaded rules, run a "Dry Run" preview of proposed changes with conflict highlighting, and trigger "Apply Rules" to commit the changeset. All business logic — rule parsing, evaluation, dry-run computation, conflict detection — lives in DiaAssetCatalogue's `CatalogueRulesEngine`. This feature is purely UI.

## Problem

The Catalogue Rules Engine in DiaAssetCatalogue can auto-tag, auto-scope, and auto-assign fields based on pattern rules. However, without a UI, users cannot see which rules are loaded, preview what changes the engine would make, identify conflicts between rules, or review manual overrides. Blind application of rules risks unintended bulk mutations. The Dry Run preview makes rule application safe and transparent.

## Acceptance Criteria

1. "Load Rules" action loads `assets.rules.json` via `CatalogueRulesEngine::LoadRules` and displays the loaded rules list
2. Each rule row shows: rule name, match criteria (pattern/type filter), action type (set_tag, set_scope, set_field)
3. "Dry Run" button calls `CatalogueRulesEngine::EvaluateDryRun` and displays the resulting `RuleChangeset` as a preview table
4. Each changeset row shows: record ID, field name, old value, new value, rule name that triggered the change
5. Conflict rows (where `mIsConflict == true`) are visually highlighted (e.g., yellow background, warning icon)
6. Conflict count summary displayed above the preview table (e.g., "12 changes, 2 conflicts")
7. "Apply Rules" button calls `CatalogueRulesEngine::Apply`, then refreshes the Asset List to show updated records
8. Apply Rules wraps all mutations as IEditorCommand instances (one compound command) for undo/redo
9. Fields that were manually set by the user (not by a rule) show a "manual" badge in the preview table
10. Manual override fields are skipped by rule application unless the user explicitly opts to overwrite
11. No direct record mutation from this UI — all changes go through `CatalogueRulesEngine::Apply` which delegates to `AssetRegistry` mutations
12. Panel shows empty state when no rules file is loaded: "No rules loaded. Use Load Rules to open assets.rules.json."

## API Design

### DiaAPI Commands Used

| Command | Usage |
|---------|-------|
| `asset_catalogue.load_manifest { path }` | (Existing) Ensure catalogue is loaded before rules can be applied |
| `asset_catalogue.validate { }` | (Existing) Optionally validate after applying rules |

### C++ API Consumed (DiaAssetCatalogue)

```cpp
namespace Dia::AssetCatalogue
{
    class CatalogueRulesEngine
    {
    public:
        // Load rules from JSON file
        bool LoadRules(const char* rulesFilePath);

        // Preview what changes would be made (no mutations)
        RuleChangeset EvaluateDryRun(const AssetRegistry& registry) const;

        // Apply the changeset to the registry (performs mutations)
        void Apply(AssetRegistry& registry, const RuleChangeset& changeset);
    };

    struct RuleChangeEntry
    {
        Dia::Core::StringCRC mRecordId;     // Asset ID affected
        const char* mFieldName;              // Field being changed
        const char* mOldValue;               // Current value
        const char* mNewValue;               // Proposed value
        const char* mRuleName;               // Rule that triggered this
        bool mIsConflict;                    // True if multiple rules target this field
        bool mIsManualOverride;              // True if the field was manually set by user
    };

    struct RuleChangeset
    {
        Dia::Core::Containers::DynamicArrayC<RuleChangeEntry, 256> mEntries;
        int mConflictCount;
    };
}
```

The editor feature creates DiaAPI command handlers that wrap these calls and serialize results to JSON for the CEF panel.

### CEF Panel Description

**Catalogue Rules Panel** — a dockable panel with three sections:
1. **Rules List** — table of loaded rules: name, match criteria, action type
2. **Changeset Preview** — table of proposed changes from Dry Run; conflict rows highlighted; manual override badges
3. **Action Bar** — "Load Rules", "Dry Run", "Apply Rules" buttons; conflict count summary

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement LoadRules command handler | Register DiaAPI command to call `CatalogueRulesEngine::LoadRules`, serialize loaded rules list to JSON for the panel |
| 2 | Implement DryRun command handler | Register DiaAPI command to call `CatalogueRulesEngine::EvaluateDryRun`, serialize `RuleChangeset` to JSON (entries, conflict count, manual override flags) |
| 3 | Implement ApplyRules command handler | Register DiaAPI command to call `CatalogueRulesEngine::Apply`; wrap all mutations in a single `CompoundCommand` for undo/redo via `CommandHistory` |
| 4 | Create Rules Panel HTML/JS | Build CEF panel with rules list table, changeset preview table, conflict highlighting, manual override badges, action bar buttons |
| 5 | Wire Dry Run results to preview table | On Dry Run response, populate changeset preview table; highlight conflict rows; show conflict count summary |
| 6 | Wire Apply Rules to Asset List refresh | After Apply completes, dispatch a refresh event to the Asset List panel to reflect updated records |
| 7 | Handle manual override skip logic | In Apply handler, filter out entries where `mIsManualOverride == true` unless user explicitly opted to overwrite (checkbox per row or "Overwrite all manuals" toggle) |
| 8 | Unit tests for command handlers | Test LoadRules, DryRun, and Apply command handlers with known rules and registry states; verify conflict detection and manual override filtering |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue | `CatalogueRulesEngine::LoadRules`, `EvaluateDryRun`, `Apply`; `RuleChangeset`, `RuleChangeEntry` types |
| DiaAssetCatalogueEditor (Feature 1) | Manifest must be loaded before rules can be evaluated |
| DiaAssetCatalogueEditor (Feature 2) | Asset List refresh after Apply; Record Editor for manual override tracking |
| DiaEditor | `IEditorCommand`, `CompoundCommand`, `CommandHistory` for undo/redo of Apply action |
| DiaUICEF | CEF browser instance for rendering the panel |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogueEditor/Commands/LoadRulesHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/LoadRulesHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/DryRunHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/DryRunHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/ApplyRulesHandler.h` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/ApplyRulesHandler.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/Commands/ApplyRulesCommand.h` | Create — IEditorCommand wrapping CatalogueRulesEngine::Apply for undo/redo |
| `Dia/DiaAssetCatalogueEditor/Commands/ApplyRulesCommand.cpp` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/rules-panel/index.html` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/rules-panel/rules.js` | Create |
| `Dia/DiaAssetCatalogueEditor/UI/rules-panel/rules.css` | Create |
| `Dia/DiaAssetCatalogueEditor/DiaAssetCatalogueEditor.cpp` | Modify — register rules panel and command handlers |
| `Cluiche/UnitTests/DiaAssetCatalogueEditor/CatalogueRulesUITests.cpp` | Create |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | **Compliant** — asset IDs and rule references use StringCRC in C++; serialized as strings for JSON bridge |
| PD-004 / AD-002 | No STL in public APIs | **Compliant** — command handlers use `const char*`, DiaCore containers, `Json::Value`; no STL in public interface |
| PD-005 | x64 Windows only | **Compliant** — no cross-platform considerations |
| PD-007 | C++20 required | **Compliant** — all new C++ files compiled under `/std:c++20` |
| PD-008 | Directory.Build.props owns build settings | **Compliant** — no build setting overrides |
| PD-009 | Generated output under Cluiche/out/ | **Compliant** — no generated file output from this feature; rules file is user-authored input, not generated |
| AD-001 | Module system with YAML frontmatter | **Compliant** — covered by parent module doc |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant** — all C++ code in `Dia::AssetCatalogue::Editor::` |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant** — changeset entries reference assets by `type.name` composite ID |
| SD-ACE-001 | Editor owns catalogue manifest authoring | **Compliant** — Apply Rules writes back to the manifest via AssetRegistry; the rules file itself is user-authored input |
| SD-ACE-004 | All mutations are IEditorCommand | **Compliant** — Apply Rules wraps all mutations in a `CompoundCommand` pushed to `CommandHistory` for undo/redo |
| SD-ACE-005 | Output to Cluiche/out/CluicheEditor/DiaAssetCatalogueEditor/ | **Compliant** — no additional file output |
| SD-ACE-007 | Rules engine logic in DiaAssetCatalogue, editor UI only | **Compliant** — this feature IS the UI-only implementation of SD-ACE-007; all rule parsing, evaluation, dry-run, conflict detection logic is in `CatalogueRulesEngine` (DiaAssetCatalogue) |
| SED-009 | Undo/redo via IEditorCommand + CommandHistory | **Compliant** — Apply Rules uses `CompoundCommand` wrapping per-record update commands; full undo/redo support |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | **Compliant** — no DiaApplicationFlow types used |
| SED-020 | Plugin output to Cluiche/out/CluicheEditor/\<PluginName\>/ | **Compliant** — no file output |
| SED-021 | Per-plugin session context via .context.json | **Compliant** — last loaded rules file path can be persisted in session context for quick reload |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Dry Run | Is the Dry Run purely read-only? | Yes. `EvaluateDryRun` returns a `RuleChangeset` without modifying the `AssetRegistry`. The changeset is a preview only. Mutations happen only when the user clicks "Apply Rules". |
| 2 | Undo | Can the user undo Apply Rules? | Yes. Apply Rules creates a `CompoundCommand` containing one `UpdateRecordCommand` per changed record. Undoing the compound command reverses all changes atomically. |
| 3 | Conflicts | What is a conflict? | A conflict occurs when two or more rules target the same field on the same record with different values. `CatalogueRulesEngine` flags these entries with `mIsConflict = true`. The UI highlights them so the user can review before applying. |
| 4 | Manual overrides | How does the system know a field was manually set? | DiaAssetCatalogue tracks which fields were explicitly set by the user (via Feature 2 record CRUD) vs. set by a rule. Fields manually edited after a rule application are flagged as manual overrides. The `mIsManualOverride` flag in `RuleChangeEntry` surfaces this. |
| 5 | Rules file | Where does assets.rules.json live? | In `Assets/<AppName>/assets.rules.json`, alongside the catalogue manifest. The user provides the path via a file picker in the "Load Rules" action. Session context remembers the last loaded path. |
| 6 | Partial apply | Can the user apply only some changes from the changeset? | Not in the initial implementation. Apply is all-or-nothing (minus manual overrides). A future iteration could add per-row checkboxes to selectively apply changes. The Dry Run preview makes the all-or-nothing approach safe because the user sees exactly what will change. |
| 7 | Error handling | What if LoadRules fails (invalid JSON, missing file)? | `CatalogueRulesEngine::LoadRules` returns false. The command handler returns an error JSON response. The panel displays an error message: "Failed to load rules: <reason>." No rules are loaded; Dry Run and Apply buttons remain disabled. |

## Visual Reference

[mockups/diaassetcatalogueeditor.html](mockups/diaassetcatalogueeditor.html) — Click **"Catalogue Rules"** tab. Shows rules list with name/match/action columns, Dry Run/Apply buttons, changeset preview table with old→new values, conflict highlighting (red CONFLICT badge), and manual override badge (yellow). Use as the visual acceptance gate after implementation.

## Status

`Done`
