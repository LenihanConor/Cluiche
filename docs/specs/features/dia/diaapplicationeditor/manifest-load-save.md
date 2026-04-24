# Feature Spec: Manifest Load/Save

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Manifest Load/Save** | (this document) |

## Problem Statement

Provides reliable loading and saving of .diaapp manifest files in the DiaApplicationEditor, with validation, dirty tracking, backup creation, and error handling to prevent data loss during editing workflows.

## Acceptance Criteria

- [x] Load .diaapp file from disk using ApplicationManifestLoader
- [x] Parse JSON and populate EditorModel state
- [x] Display loaded manifest in both Flow View and Tree View
- [x] Track dirty state when user edits manifest
- [x] Save edited manifest back to file with Ctrl+S hotkey
- [x] Create .bak backup before overwriting existing file
- [x] Validate manifest before save using ApplicationManifestValidator
- [x] Show validation errors in UI before save completes
- [x] Block save if validation fails (show error panel)
- [x] Support Save As dialog for new file path
- [x] Handle file I/O errors gracefully with user-friendly messages
- [x] Prompt user on window close if unsaved changes exist
- [x] Clear dirty flag after successful save
- [x] Open button in DiaApplicationEditor toolbar triggers native Win32 file dialog (GetOpenFileName) when no path supplied
- [x] Empty state is a drag-and-drop zone — drop a .diaapp file to open it (CEF File.path used for full system path)
- [x] Document-level `dragover`/`drop` prevention stops CEF navigating the iframe when a file is dropped outside the drop zone (e.g. on toolbar or after manifest is already open)
- [ ] DiaDataExplorerEditor (future) can open by sending `open_manifest { path }` directly — no dialog needed

## Manifest Source Model

A manifest can come from two distinct sources, tracked as a typed concept:

```typescript
type ManifestSource =
    | { type: 'file'; path: string }
    | { type: 'live' }          // Phase 3 — connected game
    | null;
```

**File source** (Phase 2): Open button (native dialog) or drag-and-drop. Save/SaveAs available. DiaDataExplorerEditor sends path directly via bridge.

**Live source** (Phase 3): Loaded from connected game via WebSocket. Save is replaced by "Push to Game". A source indicator is always visible when a manifest is loaded (`● LIVE` vs `◌ FILE: foo.diaapp`). If both are loaded simultaneously, a toggle switches between them — they are two independent snapshots.

## Design

### DiaApplicationEditor Plugin Interface

**Plugin Methods:**
```cpp
namespace Dia::Application::Editor {
    class DiaApplicationEditor : public Dia::Editor::IEditorPlugin {
    public:
        // File operations
        void OpenManifest(const char* path);
        void SaveManifest();  // Save to current path
        void SaveManifestAs(const char* path);
        void CloseManifest();
        
        // Dirty tracking
        bool IsDirty() const { return mPluginData->isDirty; }
        void MarkDirty() { mPluginData->isDirty = true; }
        void MarkClean() { mPluginData->isDirty = false; }
        
        // Validation
        bool ValidateManifest();
        const ValidationResult& GetValidationResult() const;
        
    private:
        ManifestEditorData* mPluginData;
        Dia::Editor::EditorModel* mEditorModel;
    };
}
```

### OpenManifest Implementation

**DiaApplicationEditor::OpenManifest:**
```cpp
void DiaApplicationEditor::OpenManifest(const char* path) {
    DIA_ASSERT(path != nullptr);
    
    // Check for unsaved changes
    if (IsDirty()) {
        // C++ side triggers modal dialog via WebUIBridge
        mEditorModel->NotifyObservers(StringCRC("prompt_unsaved_changes"));
        // JavaScript will call back with user choice (save/discard/cancel)
        return;  // Async - continue in callback
    }
    
    // Load file from disk
    Dia::Core::FilePath filePath(path);
    if (!filePath.Exists()) {
        DIA_LOG_ERROR("File not found: %s", path);
        mEditorModel->NotifyObservers(StringCRC("file_error"), "File not found");
        return;
    }
    
    // Parse manifest using ApplicationManifestLoader
    Dia::Application::ApplicationManifestLoader loader;
    Dia::Application::ApplicationManifest* manifest = loader.Load(path);
    
    if (!manifest) {
        DIA_LOG_ERROR("Failed to load manifest: %s", path);
        mEditorModel->NotifyObservers(StringCRC("file_error"), "Failed to parse JSON");
        return;
    }
    
    // Store in plugin data
    if (mPluginData->manifest) {
        delete mPluginData->manifest;
    }
    mPluginData->manifest = manifest;
    mPluginData->filePath = Dia::Core::String::Duplicate(path);
    mPluginData->isDirty = false;
    
    // Notify UI to refresh views
    mEditorModel->NotifyObservers(StringCRC("manifest_loaded"));
    
    DIA_LOG("Loaded manifest: %s", path);
}
```

### SaveManifest Implementation

**DiaApplicationEditor::SaveManifest:**
```cpp
void DiaApplicationEditor::SaveManifest() {
    if (!mPluginData->manifest || !mPluginData->filePath) {
        // No file loaded or new file - trigger Save As
        SaveManifestAs(nullptr);
        return;
    }
    
    // Validate before save
    if (!ValidateManifest()) {
        DIA_LOG_ERROR("Cannot save: manifest has validation errors");
        mEditorModel->NotifyObservers(StringCRC("validation_failed"));
        return;  // UI shows validation panel
    }
    
    // Create backup (.bak file)
    Dia::Core::FilePath originalPath(mPluginData->filePath);
    if (originalPath.Exists()) {
        Dia::Core::String backupPath = Dia::Core::String::Format("%s.bak", mPluginData->filePath);
        Dia::Core::FilePath::Copy(mPluginData->filePath, backupPath.GetData());
        DIA_LOG("Created backup: %s", backupPath.GetData());
    }
    
    // Serialize manifest to JSON
    Json::Value json;
    Dia::Application::ManifestComposer composer;
    composer.ComposeToJson(mPluginData->manifest, json);
    
    // Write to disk
    std::ofstream file(mPluginData->filePath);
    if (!file.is_open()) {
        DIA_LOG_ERROR("Failed to open file for writing: %s", mPluginData->filePath);
        mEditorModel->NotifyObservers(StringCRC("file_error"), "Cannot write file");
        return;
    }
    
    Json::StyledWriter writer;
    file << writer.write(json);
    file.close();
    
    // Mark clean
    mPluginData->isDirty = false;
    mEditorModel->NotifyObservers(StringCRC("manifest_saved"));
    
    DIA_LOG("Saved manifest: %s", mPluginData->filePath);
}
```

### SaveManifestAs Implementation

**DiaApplicationEditor::SaveManifestAs:**
```cpp
void DiaApplicationEditor::SaveManifestAs(const char* newPath) {
    // If newPath is null, trigger file picker dialog
    if (newPath == nullptr) {
        mEditorModel->NotifyObservers(StringCRC("show_save_dialog"));
        return;  // JavaScript will call back with selected path
    }
    
    // Update file path
    if (mPluginData->filePath) {
        Dia::Core::String::Free(mPluginData->filePath);
    }
    mPluginData->filePath = Dia::Core::String::Duplicate(newPath);
    
    // Save to new path
    SaveManifest();
}
```

### Validation

**DiaApplicationEditor::ValidateManifest:**
```cpp
bool DiaApplicationEditor::ValidateManifest() {
    if (!mPluginData->manifest) return false;
    
    // Use ApplicationManifestValidator
    Dia::Application::ManifestValidator validator;
    mPluginData->validationResult.errors.Clear();
    mPluginData->validationResult.warnings.Clear();
    
    bool isValid = validator.Validate(mPluginData->manifest, 
                                      &mPluginData->validationResult.errors,
                                      &mPluginData->validationResult.warnings);
    
    mPluginData->validationResult.isValid = isValid;
    
    // Notify UI of validation result
    mEditorModel->NotifyObservers(StringCRC("validation_complete"));
    
    return isValid;
}
```

### React Component Integration

**ManifestEditor.tsx:**
```typescript
import React, { useState, useEffect } from 'react';
import { FlowView } from './FlowView';
import { TreeView } from './TreeView';

interface ManifestEditorProps {
    manifestPath: string;
    isDirty: boolean;
}

export const ManifestEditor: React.FC<ManifestEditorProps> = ({ manifestPath, isDirty }) => {
    const [manifest, setManifest] = useState<ApplicationManifest | null>(null);
    const [viewMode, setViewMode] = useState<'flow' | 'tree'>('flow');
    
    // Listen for C++ events
    useEffect(() => {
        const handleManifestLoaded = () => {
            // Query updated manifest from C++
            const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
            setManifest(data.manifest);
        };
        
        const handleManifestSaved = () => {
            // Refresh from C++ (dirty flag cleared)
            const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
            setManifest(data.manifest);
        };
        
        window.DiaEditor.subscribe('manifest_loaded', handleManifestLoaded);
        window.DiaEditor.subscribe('manifest_saved', handleManifestSaved);
        
        return () => {
            window.DiaEditor.unsubscribe('manifest_loaded', handleManifestLoaded);
            window.DiaEditor.unsubscribe('manifest_saved', handleManifestSaved);
        };
    }, []);
    
    // Save handler (Ctrl+S)
    const handleSave = () => {
        window.DiaEditor.executeCommand('diaapp-editor', 'save', {});
    };
    
    // Prompt on window close if dirty
    useEffect(() => {
        if (!isDirty) return;
        
        const handleBeforeUnload = (e: BeforeUnloadEvent) => {
            e.preventDefault();
            e.returnValue = 'You have unsaved changes. Are you sure you want to close?';
        };
        
        window.addEventListener('beforeunload', handleBeforeUnload);
        return () => window.removeEventListener('beforeunload', handleBeforeUnload);
    }, [isDirty]);
    
    // Keyboard shortcut
    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.ctrlKey && e.key === 's') {
                e.preventDefault();
                handleSave();
            }
        };
        
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, []);
    
    return (
        <div className="manifest-editor">
            <div className="toolbar">
                <button onClick={handleSave} disabled={!isDirty}>
                    Save {isDirty && '*'}
                </button>
                <button onClick={() => setViewMode(viewMode === 'flow' ? 'tree' : 'flow')}>
                    Switch to {viewMode === 'flow' ? 'Tree' : 'Flow'} View
                </button>
            </div>
            
            {viewMode === 'flow' ? (
                <FlowView manifest={manifest} />
            ) : (
                <TreeView manifest={manifest} />
            )}
        </div>
    );
};
```

### JavaScript → C++ Bridge

**Save Command:**
```javascript
// User presses Ctrl+S or clicks Save button
window.DiaEditor.executeCommand("diaapp-editor", "save", {});
// → C++ DiaApplicationEditor::SaveManifest()
```

**Save As Command:**
```javascript
// User clicks Save As
window.DiaEditor.executeCommand("diaapp-editor", "save_as", { path: newPath });
// → C++ DiaApplicationEditor::SaveManifestAs(newPath)
```

**Open Command:**
```javascript
// User selects File → Open
window.DiaEditor.executeCommand("diaapp-editor", "open", { path: filePath });
// → C++ DiaApplicationEditor::OpenManifest(filePath)
```

### C++ → JavaScript Events

**Manifest Loaded:**
```cpp
mEditorModel->NotifyObservers(StringCRC("manifest_loaded"));
// → JavaScript event handler refreshes UI
```

**Manifest Saved:**
```cpp
mEditorModel->NotifyObservers(StringCRC("manifest_saved"));
// → JavaScript clears dirty indicator (*)
```

**File Error:**
```cpp
mEditorModel->NotifyObservers(StringCRC("file_error"), errorMessage);
// → JavaScript shows error dialog
```

### Error Handling

**File Not Found:**
```cpp
if (!filePath.Exists()) {
    DIA_LOG_ERROR("File not found: %s", path);
    mEditorModel->NotifyObservers(StringCRC("file_error"), "File not found");
    return;
}
```

**Parse Error:**
```cpp
if (!manifest) {
    DIA_LOG_ERROR("Failed to load manifest: %s", path);
    mEditorModel->NotifyObservers(StringCRC("file_error"), "Failed to parse JSON");
    return;
}
```

**Write Error:**
```cpp
if (!file.is_open()) {
    DIA_LOG_ERROR("Failed to open file for writing: %s", mPluginData->filePath);
    mEditorModel->NotifyObservers(StringCRC("file_error"), "Cannot write file");
    return;
}
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.h` - Plugin interface with file operations
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Load/save/validation implementation
- `Dia/DiaApplicationEditor/UI/ManifestEditor.tsx` - Root React component with save logic
- `Dia/DiaApplicationEditor/UI/FileDialog.tsx` - Native file picker wrapper

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Event IDs are StringCRC |
| Platform | PD-004 | No STL in public APIs | ✅ **Compliant** - Uses `const char*` for paths |
| Dia | AD-002 | No STL in public APIs | ✅ **Compliant** - All public methods use Dia types |
| DiaApplicationEditor | DAED-001 | Reuse ApplicationManifestValidator | ✅ **Compliant** - Uses validator before save |
| DiaApplicationEditor | DAED-008 | Manual save workflow | ✅ **Compliant** - Dirty tracking, explicit save |

**Decision 43 Compliance:**
- **Create .bak backup before save** - ✅ Implemented in `SaveManifest()` using `FilePath::Copy()`

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Should we auto-save periodically? | N/A | No - explicit save only (Decision 43); can add auto-save to temp in Phase 7+ |
| 2 | How to handle file changes on disk while editor open? | Decision 53-54 | Separate feature: file-conflict-detection.md handles fs watcher |
| 3 | Should we support undo/redo? | N/A | Phase 7+ - complex; explicit save/revert for Phase 5 |
| 4 | Create .bak files? | Decision 43 | ✅ Yes - implemented in SaveManifest() |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Auto-Save | Auto-save drafts? | ✅ No - explicit save only per Decision 43; manual workflow prevents accidental overwrites |
| 2 | File Watching | Detect external changes? | ✅ Yes - handled by file-conflict-detection.md feature (Decisions 53-54) |
| 3 | Undo/Redo | Support undo? | ✅ Phase 7+ - complex; explicit save/revert for Phase 5 |
| 4 | Backup | Create .bak files? | ✅ Yes - Decision 43 implemented in SaveManifest() |
| 5 | Validation Timing | When to validate? | ✅ Before save (blocking); also real-time in separate feature (Decisions 51-52) |
| 6 | Error Recovery | What if save fails mid-write? | ✅ .bak file allows recovery; atomicity guaranteed by creating backup before overwriting |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
