# Feature Spec: Editable Lifecycle Grid

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Editable Lifecycle Grid** | (this document) |

## Problem Statement

The Lifecycle View shows a matrix of modules (rows) × phases (columns), displaying which modules are active in which phases. Currently this is read-only. Managing `phase_ids` for a module requires going through the Module Config Editor or editing raw JSON. The lifecycle grid is the most natural and visual place to toggle module-to-phase associations — a single click should add or remove a module from a phase.

## Acceptance Criteria

- [ ] Clicking an inactive cell (module not in that phase) adds the module to that phase's `phase_ids`
- [ ] Clicking an active cell (module in that phase) removes the module from that phase's `phase_ids`
- [ ] Toggle is instant (no confirmation dialog)
- [ ] Cell immediately updates its visual state (color/symbol) on toggle
- [ ] Change marks the manifest dirty
- [ ] Change triggers real-time validation (via existing 500ms debounce)
- [ ] Cannot remove a module from its last remaining phase (must be active in at least one phase) — show tooltip explaining why
- [ ] Cursor changes to pointer on hover over toggleable cells
- [ ] Visual hover feedback (subtle highlight) on cells
- [ ] Change is pushed to C++ via bridge event
- [ ] Change is undoable (integrates with undo/redo history)

## Design

### Cell Click Handler

```typescript
const handleCellClick = (moduleId: string, phaseId: string, currentlyActive: boolean) => {
    const module = findModule(manifest, moduleId);
    if (!module) return;
    
    // Guard: don't remove from last phase
    if (currentlyActive && module.phases.length <= 1) {
        // Show tooltip: "Module must be active in at least one phase"
        return;
    }
    
    // Push undo snapshot before mutation
    pushSnapshot(manifest, currentlyActive 
        ? `Remove ${moduleId} from ${phaseId}` 
        : `Add ${moduleId} to ${phaseId}`);
    
    // Toggle
    const newPhases = currentlyActive
        ? module.phases.filter(p => p !== phaseId)
        : [...module.phases, phaseId];
    
    // Update manifest in store
    updateModulePhases(moduleId, newPhases);
    
    // Notify C++
    sendToPlugin('module_phases_changed', {
        pu_id: puId,
        module_id: moduleId,
        phase_ids: newPhases,
    });
    
    setDirty(true);
};
```

### Visual Feedback

Extend existing cell styles in LifecycleView.tsx:

```typescript
// Add to cell <td>:
style={{
    ...existingStyle,
    cursor: 'pointer',
    transition: 'background-color 0.1s',
}}
onMouseEnter={e => e.currentTarget.style.opacity = '0.7'}
onMouseLeave={e => e.currentTarget.style.opacity = '1'}
onClick={() => handleCellClick(mod.instance_id, phase.instance_id, isActive)}
```

### Tooltip for Last-Phase Guard

Use a title attribute or a small transient tooltip when the user clicks a cell that can't be toggled:
```typescript
title={isLastPhase ? 'Module must be active in at least one phase' : 'Click to toggle'}
```

### C++ Bridge Handler

```cpp
void DiaApplicationEditor::HandleModulePhasesChanged(const Json::Value& data) {
    const char* puId = data["pu_id"].asCString();
    const char* moduleId = data["module_id"].asCString();
    const Json::Value& phaseIds = data["phase_ids"];
    
    // Find module in manifest IR, update phase_ids
    // Mark dirty, trigger validation
}
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/src/LifecycleView.tsx` - Add click handlers to cells, hover styles, last-phase guard
- `Dia/DiaApplicationEditor/UI/src/ManifestStore.ts` - Add `updateModulePhases` action
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Handle `module_phases_changed` event

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-008 | Manual save workflow | ✅ Toggle marks dirty; user saves explicitly |
| DiaApplicationEditor | DAED-001 | Reuse ApplicationManifestValidator | ✅ Toggle triggers validation which may catch invalid state |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | UX | Why no confirmation? | User specified instant apply. Toggle is easily reversible (click again, or Ctrl+Z). Low risk. |
| 2 | Guard | Why prevent removing from last phase? | A module with empty phase_ids has no execution context — it's invalid. Validation would catch it, but better to prevent it at the UI level. |
| 3 | Dependencies | What about module dependencies? | If module A depends on module B, and you remove B from a phase where A still runs, validation will flag it. The grid doesn't block the action — validation provides the feedback. |
| 4 | Ordering | Does phase_ids order matter? | No — it's a set of phase associations. Order in the array is irrelevant to runtime behavior. |

## Status

`Approved`
