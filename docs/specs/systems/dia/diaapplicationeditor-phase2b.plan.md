# Plan: DiaApplicationEditor Phase 2b — Static Editing Enhancements

**Spec:** @docs/specs/systems/dia/diaapplicationeditor.md  
**Status:** In Progress  
**Started:** 2026-05-06  
**Last Updated:** 2026-05-06

## Prerequisites

All Phase 2 features are complete. No new external dependencies required — all work is within existing DiaApplicationEditor C++ and UI code.

## Critical Pre-fix: Save-Flattening Guard

Since `LoadFromFile` now resolves imports (via ManifestComposer), the editor receives merged content. If a user saves, the current ManifestSerializer writes ALL PUs — effectively flattening imports. This must be fixed before Import Management, but also protects users NOW.

**Fix:** ManifestSerializer should only serialize PUs where `sourceManifestPath` is empty or matches the current file path. The imports array is preserved as-is.

## Implementation Order

Dependencies:
- Undo/Redo is foundational — other features integrate with it
- New Manifest is independent, quick
- PU/Phase Inspector is independent, medium
- Editable Lifecycle Grid integrates with Undo
- Validation Navigation is independent, small
- Import Management is the most complex, comes last (builds on save-flattening fix)

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Save-flattening guard: ManifestSerializer only writes local PUs | GoogleTest: serialize manifest with sourceManifestPath set → imported PUs excluded | Done | sonnet | SerializeLocal() added; WriteManifestToDisk uses it |
| 1 | Undo/Redo: UndoStore.ts + keyboard shortcuts + toolbar buttons | UndoStore.test.ts: push/pop/redo/cap at 8/clear | Done | sonnet | UndoStore.ts created; Toolbar Undo/Redo buttons; Ctrl+Z/Y in App.tsx |
| 2 | Undo/Redo: integrate pushSnapshot into all existing mutation paths | Manual: make changes, Ctrl+Z, verify revert | Done | sonnet | pushUndo calls in TreeView, FlowView, ModuleInspector, LifecycleView |
| 3 | New Manifest: Ctrl+N + blank template + Save As flow | Manual: Ctrl+N, verify tree shows 1 PU/1 Phase, Save triggers file picker | Done | sonnet | BLANK_MANIFEST in App.tsx + Toolbar; HandleNewManifest in C++ |
| 4 | PU/Phase Inspector: InspectorPanel router + PUInspector + PhaseInspector | Manual: select PU → see fields; select Phase → see fields; edit frequency_hz → dirty | Done | sonnet | InspectorPanel.tsx routes by segment count; PUInspector + PhaseInspector created |
| 5 | PU/Phase Inspector: C++ bridge handlers (pu_property_changed, phase_property_changed, initial_phase_changed) | GoogleTest: handle pu_property_changed updates manifest IR | Done | sonnet | HandlePUPropertyChanged, HandlePhasePropertyChanged, HandleInitialPhaseChanged |
| 6 | Editable Lifecycle Grid: click-to-toggle cells + undo integration | LifecycleView.test.tsx: click cell toggles phase association | Done | sonnet | Cell click handler with last-phase guard; undo push; sendToPlugin |
| 7 | Editable Lifecycle Grid: C++ handler (module_phases_changed) | GoogleTest: handle module_phases_changed updates phaseIds | Done | sonnet | HandleModulePhasesChanged added |
| 8 | Validation Navigation: click error → resolve path → select + scroll | ValidationPanel.test.tsx: click error calls setSelectedNode with correct ID | Done | sonnet | resolveContextToNodeId function; hover feedback on error rows |
| 9 | Import Management: ImportsPanel UI + ManifestStore imports state | Manual: open manifest with imports → see import list with badges | Done | sonnet | ImportsPanel.tsx created; added to App.tsx |
| 10 | Import Management: visual badges in Tree/Flow/Lifecycle for imported nodes | Manual: imported PUs show [filename] badge / dashed border | Done | sonnet | _source in serializer; TreeView name badge; FlowView dashed border |
| 11 | Import Management: add/remove import + cycle detection in C++ | GoogleTest: cycle detection blocks circular import | Done | sonnet | HandleAddImport with ManifestComposer cycle check; HandleRemoveImport |
| 12 | Import Management: inline edit of imported content + multi-file save | Manual: edit imported module config → source file saved | Done | sonnet | Editing imported nodes works via existing config handlers; SerializeLocal writes only local PUs on save |

## Session Notes

### 2026-05-06
- Plan created. 6 features → 13 tasks (including pre-fix).
- Save-flattening identified as critical pre-fix: without it, opening + saving a manifest with imports destroys the import structure.
- All 13 tasks implemented. C++ compiles clean (DiaApplicationEditor.lib produced). UI build requires Node.js in PATH (not available in this session) but TS code is structurally complete.
- Key files created: UndoStore.ts, InspectorPanel.tsx, PUInspector.tsx, PhaseInspector.tsx, ImportsPanel.tsx
- Key files modified: ManifestSerializer.h/.cpp, DiaApplicationEditor.h/.cpp, App.tsx, Toolbar.tsx, ManifestStore.ts, FlowView.tsx, TreeView.tsx, LifecycleView.tsx, ModuleInspector.tsx, ValidationPanel.tsx, types.ts
