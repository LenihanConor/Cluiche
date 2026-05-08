# Feature Spec: Undo/Redo

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Undo/Redo** | (this document) |

## Problem Statement

Users need the ability to undo and redo editing actions in the manifest editor. Without this, any accidental change requires manually re-editing or closing without saving — unacceptable for a professional editing tool.

## Acceptance Criteria

- [ ] Ctrl+Z undoes the most recent manifest-modifying action
- [ ] Ctrl+Y (or Ctrl+Shift+Z) redoes the most recently undone action
- [ ] History stack depth is capped at 8 entries
- [ ] Redo stack is cleared when a new action is performed after undoing
- [ ] Undo/redo updates all views (Tree, Flow, Lifecycle) immediately
- [ ] Undo/redo correctly sets/clears the dirty flag
- [ ] History is reset when switching views (Tree → Flow → back clears history)
- [ ] History is reset when opening a new manifest or closing the current one
- [ ] Actions that are undoable: add/remove node, add/remove transition, reorder node, module config change, PU/phase property change, module-to-phase toggle
- [ ] Toolbar shows Undo/Redo buttons (disabled when stack is empty)

## Design

### History Store

**UndoStore.ts (Zustand middleware or separate slice):**
```typescript
interface HistoryEntry {
    snapshot: ManifestData;
    description: string;  // e.g., "Add phase", "Remove module"
}

interface UndoState {
    undoStack: HistoryEntry[];  // max 8
    redoStack: HistoryEntry[];
    
    pushSnapshot: (manifest: ManifestData, description: string) => void;
    undo: () => ManifestData | null;
    redo: () => ManifestData | null;
    clearHistory: () => void;
    canUndo: () => boolean;
    canRedo: () => boolean;
}
```

### Snapshot Strategy

Full manifest snapshot on each undoable action. Given manifests are small JSON objects (< 10 KB typically), storing 8 full copies is negligible. This avoids the complexity of command-pattern inverses.

### Integration Points

- **ManifestStore** calls `pushSnapshot()` before applying any mutation
- **View toggle** calls `clearHistory()`
- **Open/close manifest** calls `clearHistory()`
- **Undo/redo** calls `setManifest()` with the restored snapshot and `setDirty()` accordingly

### Keyboard Shortcuts

Registered in App.tsx alongside existing Ctrl+S and Ctrl+T:
```typescript
useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
        if (e.ctrlKey && e.key === 'z') { e.preventDefault(); handleUndo(); }
        if (e.ctrlKey && e.key === 'y') { e.preventDefault(); handleRedo(); }
        if (e.ctrlKey && e.shiftKey && e.key === 'Z') { e.preventDefault(); handleRedo(); }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
}, []);
```

### Dirty Flag Interaction

- If user makes changes A, B, C then undoes to B: dirty = true (still differs from saved)
- If user undoes all the way back to the last-saved state: dirty = false
- Track `savedSnapshot` reference to compare against current state

## Implementation Files

- `Dia/DiaApplicationEditor/UI/src/UndoStore.ts` - Undo/redo state management
- `Dia/DiaApplicationEditor/UI/src/ManifestStore.ts` - Integration: push snapshots before mutations
- `Dia/DiaApplicationEditor/UI/src/App.tsx` - Keyboard shortcuts
- `Dia/DiaApplicationEditor/UI/src/Toolbar.tsx` - Undo/Redo buttons

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-008 | Manual save workflow: mark dirty, user saves | ✅ Undo/redo correctly maintains dirty flag relative to last save |
| DiaApplicationEditor | DAED-002 | Two view modes: Flow and Tree | ✅ History resets on view switch per user requirement |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Strategy | Why full snapshots instead of command pattern? | Manifests are small (< 10 KB); 8 copies = negligible memory; avoids complex inverse logic for every action type |
| 2 | Depth | Why 8? | User-specified cap; keeps memory bounded; typical undo session rarely needs more |
| 3 | View switch | Why clear on view switch? | User preference; simplifies reasoning about what "undo" means when context changes |
| 4 | Scope | Does undo restore selection state? | No — only manifest data. Selection is UI state, not document state |
| 5 | Conflict | What if file conflict occurs during undo? | File conflict detection is based on disk state vs in-memory; undo changes in-memory state, so conflict banner may appear/disappear naturally |

## Status

`Approved`
