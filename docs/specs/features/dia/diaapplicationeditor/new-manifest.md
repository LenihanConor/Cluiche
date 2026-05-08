# Feature Spec: New Manifest

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **New Manifest** | (this document) |

## Problem Statement

The editor can Open existing manifests but cannot create new ones. Users must create .diaapp files externally (manually or via DiaCLI) and then open them. A "New" action provides a complete in-editor workflow for starting a fresh manifest.

## Acceptance Criteria

- [ ] "New" button in toolbar (next to Open)
- [ ] Ctrl+N keyboard shortcut
- [ ] Creates a blank manifest with: version 1, one ProcessingUnit ("NewProcessingUnit"), one Phase ("InitPhase"), no modules, no imports
- [ ] New manifest is immediately loaded into the editor (all views populated)
- [ ] New manifest is marked dirty (unsaved)
- [ ] New manifest has no file path — Save triggers "Save As" (file picker)
- [ ] If current manifest is dirty when New is clicked, prompt: "Unsaved changes will be lost. Continue?" (OK/Cancel)
- [ ] After Save As, the manifest gets a file path and subsequent Ctrl+S saves to that path

## Design

### Blank Manifest Template

```typescript
const BLANK_MANIFEST: ManifestData = {
    version: 1,
    imports: [],
    processing_units: [
        {
            type_id: 'NewProcessingUnit',
            instance_id: 'NewProcessingUnit',
            root: true,
            frequency_hz: 30.0,
            dedicated_thread: false,
            config: {},
            phases: [
                { type_id: 'InitPhase', instance_id: 'InitPhase', config: {} }
            ],
            transitions: [],
            initial_phase: 'InitPhase',
            modules: [],
        }
    ],
};
```

### Toolbar and Keyboard

```typescript
// Toolbar.tsx
<button onClick={handleNew} title="New Manifest (Ctrl+N)">New</button>

// App.tsx keyboard handler
if (e.ctrlKey && e.key === 'n') { e.preventDefault(); handleNew(); }
```

### Unsaved Changes Guard

```typescript
const handleNew = () => {
    if (isDirty) {
        if (!window.confirm('Unsaved changes will be lost. Continue?')) return;
    }
    
    // Clear undo history
    clearHistory();
    
    // Load blank template
    setManifest(BLANK_MANIFEST);
    setFilePath(null);  // No file path — triggers Save As on next save
    setDirty(true);     // New manifest needs saving
    setImports([]);
};
```

### Save As Integration

Existing save logic in C++ checks if file path is null:
```cpp
void DiaApplicationEditor::HandleSave() {
    if (mFilePath == nullptr) {
        // No path — ask UI to show Save As dialog
        PushEvent("show_save_as_dialog", Json::Value());
        return;
    }
    SaveToPath(mFilePath);
}
```

The UI-side Save As dialog is a native file picker via the CEF bridge:
```typescript
sendToPlugin('save_manifest_as', { path: selectedPath });
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/src/Toolbar.tsx` - "New" button
- `Dia/DiaApplicationEditor/UI/src/App.tsx` - Ctrl+N shortcut, handleNew logic
- `Dia/DiaApplicationEditor/UI/src/ManifestStore.ts` - `setFilePath` action (null = untitled)
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Handle null path in save → trigger Save As

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-008 | Manual save workflow | ✅ New manifest starts dirty; user must save explicitly; Save As for untitled |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Template | Why not empty (no PU at all)? | An empty manifest is invalid (no processing units). Starting with one PU and one Phase gives the user a valid starting point to build from. |
| 2 | Naming | "NewProcessingUnit" / "InitPhase" — user will rename? | Yes. These are placeholder names. The PU/Phase Inspector feature allows renaming instance_id. Type discovery dropdown helps pick the right type_id. |
| 3 | Guard | What if user clicks New with no manifest open and nothing dirty? | No prompt — just load the blank template immediately. Guard only fires when isDirty is true. |

## Status

`Approved`
