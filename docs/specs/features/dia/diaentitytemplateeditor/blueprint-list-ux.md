# Feature Spec: blueprint-list-ux

**System:** DiaEntityTemplateEditor
**Parent:** @docs/specs/systems/dia/DiaEntityTemplateEditor.md
**Status:** Done

## Summary

Fix the blueprint list population bug and improve the list UX: blueprints appear on load (no manual registration needed), refresh on catalogue changes, support text filtering and stage/global scope filtering, remove the dead Save button, add trace logging on auto-save, and remove the meaningless "Ready" status text.

## Goals

- Blueprint list populates automatically from the asset catalogue when a `.diagame` is loaded
- List refreshes automatically when catalogue changes (asset added/removed/renamed), plus a manual refresh button
- Text filter narrows the list by ID substring match
- Scope filter: "All" (default) vs per-stage (if stages reference specific blueprints)
- Remove UX dead weight: Save button, dirty indicator, "Ready" status
- Trace-level logging on every auto-save (update_field, add_component, remove_component)

## Acceptance Criteria

### AC-1: List populates from catalogue on project load

- When DiaEntityTemplateEditor initialises and a valid `.diagame` is loaded, `get_list` queries the DiaAssetCatalogue's shared registry (via `asset_catalogue.query_by_type`) for `diaentitytemplate`, `diacamera`, `dialight`
- The private `mRegistry` + `register_catalogue_asset` handler are removed — single source of truth is the catalogue
- `blargh.diaentitytemplatetemplate` (and any other registered assets) appear in the left panel without manual intervention

### AC-2: Auto-refresh on catalogue change

- DiaAssetCatalogueEditor broadcasts a `asset_catalogue.registry_changed` notification via `NotifyUIDataChanged` whenever assets are created, deleted, or renamed
- DiaEntityTemplateEditor subscribes to this notification and re-fetches the list
- A manual "Refresh" button (↻) in the list panel title bar also triggers a re-fetch

### AC-3: Text filter

- A text input above the list filters blueprint items by case-insensitive substring match on the asset ID
- Filter applies across all groups (Entity/Camera/Light)
- Empty groups are hidden when all items are filtered out
- Clearing the filter restores the full list

### AC-4: Scope filter (global / per-stage) — DEFERRED

Deferred until scene→blueprint relationships are modelled in the catalogue (likely via DiaSceneEditor work). Text filter is sufficient at current blueprint counts. Revisit when 50+ blueprints across multiple stages makes stage scoping valuable.

### AC-5: Remove Save button and dirty indicator

- Save button removed from toolbar
- Dirty indicator (●) removed from toolbar
- `onSave()` function and `setDirty()` logic removed from UI
- `entity_template_editor.save` handler remains (used internally by update_field/add/remove)

### AC-6: Trace logging on save

- Every successful `BlueprintFileHandler::Save()` call emits `DIA_LOG_INFO("Editor", "Blueprint saved: '%s'", path)`
- This already partially exists — verify it's on all mutation paths (update_field, add_component, remove_component)

### AC-7: Remove "Ready" status

- `setStatus('Ready')` call in `init()` removed
- Status area only shows transient messages (loaded, saved, errors) that auto-clear after 3 seconds

## Binding Decisions

| Decision | Compliance |
|---|---|
| SED-BP-003 | Discovery via DiaAssetCatalogue. List now queries catalogue directly — full compliance. |

## Open Design Questions

None — stage scope filter deferred (see AC-4).

## Tasks

See [blueprint-list-ux.plan.md](./blueprint-list-ux.plan.md)
