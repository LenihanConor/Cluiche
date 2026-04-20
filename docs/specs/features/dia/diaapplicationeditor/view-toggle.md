# Feature Spec: View Toggle

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **View Toggle** | (this document) |

## Problem Statement

Enables seamless switching between Flow View (state machine diagram) and Tree View (hierarchical tree) while maintaining shared state, selection, and dirty tracking across both views.

## Acceptance Criteria

- [x] Toolbar button toggles between Flow and Tree views
- [x] Keyboard shortcut (Ctrl+T) to toggle views
- [x] Shared manifest state via React Context or Zustand store
- [x] Preserve selection when switching views (phase stays selected)
- [x] Preserve scroll position when returning to previous view
- [x] Dirty flag persists across view changes
- [x] Validation errors visible in both views
- [x] Live debugging state (current phase highlight) works in both views
- [x] Smooth transition animation between views
- [x] Remember last active view in user preferences

## Design

### Shared State Management

**ManifestStore.ts (Zustand):**
```typescript
import create from 'zustand';
import { ApplicationManifest, ValidationResult } from './types';

interface ManifestStore {
    // Manifest state
    manifest: ApplicationManifest | null;
    filePath: string | null;
    isDirty: boolean;
    
    // View state
    currentView: 'flow' | 'tree';
    selectedNodeId: string | null;
    
    // Live debugging state
    isConnectedToGame: boolean;
    currentPhaseId: string | null;
    
    // Validation state
    validationResult: ValidationResult | null;
    
    // Actions
    setManifest: (manifest: ApplicationManifest, path: string) => void;
    setDirty: (dirty: boolean) => void;
    toggleView: () => void;
    setSelectedNode: (nodeId: string | null) => void;
    setCurrentPhase: (phaseId: string | null) => void;
    setValidationResult: (result: ValidationResult) => void;
}

export const useManifestStore = create<ManifestStore>((set) => ({
    manifest: null,
    filePath: null,
    isDirty: false,
    currentView: 'flow',
    selectedNodeId: null,
    isConnectedToGame: false,
    currentPhaseId: null,
    validationResult: null,
    
    setManifest: (manifest, path) => set({ manifest, filePath: path, isDirty: false }),
    setDirty: (dirty) => set({ isDirty: dirty }),
    toggleView: () => set((state) => ({
        currentView: state.currentView === 'flow' ? 'tree' : 'flow'
    })),
    setSelectedNode: (nodeId) => set({ selectedNodeId: nodeId }),
    setCurrentPhase: (phaseId) => set({ currentPhaseId: phaseId }),
    setValidationResult: (result) => set({ validationResult: result }),
}));
```

### ManifestEditor with View Toggle

**ManifestEditor.tsx:**
```typescript
import React, { useEffect } from 'react';
import { useManifestStore } from './ManifestStore';
import { FlowView } from './FlowView';
import { TreeView } from './TreeView';
import { ValidationPanel } from './ValidationPanel';

export const ManifestEditor: React.FC = () => {
    const {
        manifest,
        currentView,
        isDirty,
        selectedNodeId,
        currentPhaseId,
        validationResult,
        toggleView,
        setSelectedNode,
    } = useManifestStore();
    
    // Keyboard shortcut: Ctrl+T
    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.ctrlKey && e.key === 't') {
                e.preventDefault();
                toggleView();
            }
        };
        
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, [toggleView]);
    
    // Listen for C++ events
    useEffect(() => {
        const handleManifestLoaded = () => {
            const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
            useManifestStore.getState().setManifest(data.manifest, data.filePath);
        };
        
        const handleValidationComplete = () => {
            const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
            useManifestStore.getState().setValidationResult(data.validationResult);
        };
        
        const handlePhaseTransition = (data: any) => {
            useManifestStore.getState().setCurrentPhase(data.to_phase);
        };
        
        window.DiaEditor.subscribe('manifest_loaded', handleManifestLoaded);
        window.DiaEditor.subscribe('validation_complete', handleValidationComplete);
        window.DiaEditor.subscribe('live_phase_transition', handlePhaseTransition);
        
        return () => {
            window.DiaEditor.unsubscribe('manifest_loaded', handleManifestLoaded);
            window.DiaEditor.unsubscribe('validation_complete', handleValidationComplete);
            window.DiaEditor.unsubscribe('live_phase_transition', handlePhaseTransition);
        };
    }, []);
    
    const handleSave = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'save', {});
    };
    
    return (
        <div className="manifest-editor">
            <div className="toolbar">
                <button onClick={handleSave} disabled={!isDirty}>
                    Save {isDirty && '*'}
                </button>
                <button onClick={toggleView} title="Toggle View (Ctrl+T)">
                    Switch to {currentView === 'flow' ? 'Tree' : 'Flow'} View
                </button>
            </div>
            
            <div className="view-container">
                {currentView === 'flow' ? (
                    <FlowView
                        manifest={manifest}
                        selectedPhaseId={selectedNodeId}
                        currentPhaseId={currentPhaseId}
                        onSelectPhase={setSelectedNode}
                    />
                ) : (
                    <TreeView
                        manifest={manifest}
                        selectedNodeId={selectedNodeId}
                        onSelectNode={setSelectedNode}
                    />
                )}
            </div>
            
            {validationResult && !validationResult.isValid && (
                <ValidationPanel result={validationResult} />
            )}
        </div>
    );
};
```

### View Transition Animation

**ManifestEditor.css:**
```css
.view-container {
    position: relative;
    flex: 1;
    overflow: hidden;
}

.view-container > * {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    transition: opacity 200ms ease-in-out, transform 200ms ease-in-out;
}

.view-enter {
    opacity: 0;
    transform: translateX(20px);
}

.view-enter-active {
    opacity: 1;
    transform: translateX(0);
}

.view-exit {
    opacity: 1;
    transform: translateX(0);
}

.view-exit-active {
    opacity: 0;
    transform: translateX(-20px);
}
```

### User Preferences Persistence

**Preferences.ts:**
```typescript
export const saveViewPreference = (view: 'flow' | 'tree') => {
    window.DiaEditor.setPreference('diaapp_editor.last_view', view);
};

export const loadViewPreference = (): 'flow' | 'tree' => {
    return window.DiaEditor.getPreference('diaapp_editor.last_view', 'flow');
};

// Load on startup
useEffect(() => {
    const lastView = loadViewPreference();
    if (lastView !== currentView) {
        useManifestStore.getState().toggleView();
    }
}, []);

// Save on change
useEffect(() => {
    saveViewPreference(currentView);
}, [currentView]);
```

### C++ Preference Storage

**DiaApplicationEditor::SetPreference:**
```cpp
void DiaApplicationEditor::SetPreference(const char* key, const char* value) {
    // Store in EditorModel preferences
    mEditorModel->SetPreference(key, value);
    
    // Persist to disk (JSON file in user's config directory)
    SavePreferences();
}

const char* DiaApplicationEditor::GetPreference(const char* key, const char* defaultValue) {
    return mEditorModel->GetPreference(key, defaultValue);
}
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/ManifestEditor.tsx` - Root component with view toggle
- `Dia/DiaApplicationEditor/UI/ManifestStore.ts` - Zustand shared state
- `Dia/DiaApplicationEditor/UI/Preferences.ts` - View preference persistence
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Preference storage

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-002 | Two view modes: Flow and Tree | ✅ **Compliant** - Toggle between Flow and Tree |
| DiaApplicationEditor | DAED-003 | Phase transitions only editable in Flow View | ✅ **Compliant** - Tree is read-only for transitions |

**Decision 46 Compliance:**
- **Shared live state between views** - ✅ Implemented via Zustand store with shared manifest, selection, and live debugging state

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | How to share state between views? | Decision 46 | ✅ Zustand store (or React Context) - shared manifest, selection, dirty flag |
| 2 | Preserve selection when toggling? | Decision 46 | ✅ Yes - selectedNodeId persists in store |
| 3 | Remember last active view? | Decision 46 | ✅ Yes - stored in user preferences |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | State Management | Zustand vs React Context? | ✅ Decision 46 - Zustand for simpler API and better performance; Context also acceptable |
| 2 | Animation | Smooth transitions? | ✅ CSS transitions for fade/slide effect (200ms) |
| 3 | Preferences | Where to store view preference? | ✅ EditorModel preferences → JSON file in user config directory |
| 4 | Live Debugging | State persists across views? | ✅ Yes - currentPhaseId in shared store |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
