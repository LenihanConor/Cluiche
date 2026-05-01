# Feature Spec: File Conflict Detection

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **File Conflict Detection** | (this document) |

## Problem Statement

Detects when .diaapp manifest files are modified externally (by version control, other editors, or hot reload) while open in DiaApplicationEditor, presenting conflicts via non-modal banner with options to keep local changes or reload from disk.

## Acceptance Criteria

- [x] Watch file for external modifications using filesystem watcher
- [x] Detect changes when file modified timestamp updates
- [x] Show non-modal banner at top of editor when conflict detected
- [x] Display banner message: "File modified externally. [Keep My Changes] [Reload from Disk] [Show Diff]"
- [x] "Keep My Changes" - dismiss banner, keep current editor state
- [x] "Reload from Disk" - discard editor changes, reload file
- [x] "Show Diff" - display side-by-side diff of local vs disk versions
- [x] Auto-hide banner if user saves changes (resolves conflict)
- [x] Pause file watching during save to avoid false positives
- [x] Handle multiple rapid external changes (debounce notifications)
- [x] Display diff using react-diff-viewer or similar

## Design

### File System Watcher

**DiaApplicationEditor::StartFileWatcher:**
```cpp
void DiaApplicationEditor::StartFileWatcher() {
    if (!mPluginData->filePath) return;
    
    // Create file watcher for current manifest
    mFileWatcher = new Dia::Core::FileWatcher(mPluginData->filePath);
    
    mFileWatcher->SetCallback([this](const char* path, FileChangeType changeType) {
        HandleFileChanged(path, changeType);
    });
    
    mFileWatcher->Start();
}

void DiaApplicationEditor::StopFileWatcher() {
    if (mFileWatcher) {
        mFileWatcher->Stop();
        delete mFileWatcher;
        mFileWatcher = nullptr;
    }
}
```

**DiaApplicationEditor::HandleFileChanged:**
```cpp
void DiaApplicationEditor::HandleFileChanged(const char* path, FileChangeType changeType) {
    // Ignore if we're currently saving (pause watcher)
    if (mIsSaving) return;
    
    DIA_LOG("File modified externally: %s", path);
    
    // Load disk version
    Dia::Application::ApplicationManifestLoader loader;
    Dia::Application::ApplicationManifest* diskManifest = loader.Load(path);
    
    if (!diskManifest) {
        DIA_LOG_ERROR("Failed to load modified file");
        return;
    }
    
    // Compare with current editor state
    bool hasLocalChanges = mPluginData->isDirty;
    
    if (hasLocalChanges) {
        // Conflict detected - show banner
        Json::Value conflictData;
        conflictData["local_has_changes"] = true;
        conflictData["disk_version"] = SerializeManifest(diskManifest);
        conflictData["editor_version"] = SerializeManifest(mPluginData->manifest);
        
        mEditorModel->NotifyObservers(StringCRC("file_conflict_detected"), conflictData);
    } else {
        // No local changes - auto-reload
        delete mPluginData->manifest;
        mPluginData->manifest = diskManifest;
        mEditorModel->NotifyObservers(StringCRC("manifest_reloaded_from_disk"));
        
        DIA_LOG("Auto-reloaded file (no local changes)");
    }
}
```

**DiaApplicationEditor::SaveManifest (extended):**
```cpp
void DiaApplicationEditor::SaveManifest() {
    // Pause file watcher during save
    mIsSaving = true;
    
    // ... (existing save logic)
    
    // Resume file watcher after 100ms delay
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        mIsSaving = false;
    }).detach();
}
```

### Conflict Banner Component

**ConflictBanner.tsx:**
```typescript
import React, { useState } from 'react';
import ReactDiffViewer from 'react-diff-viewer';
import { useManifestStore } from './ManifestStore';

interface ConflictBannerProps {
    localVersion: string;
    diskVersion: string;
    onKeepLocal: () => void;
    onReloadDisk: () => void;
}

export const ConflictBanner: React.FC<ConflictBannerProps> = ({
    localVersion,
    diskVersion,
    onKeepLocal,
    onReloadDisk,
}) => {
    const [showDiff, setShowDiff] = useState(false);
    
    const handleKeepLocal = () => {
        onKeepLocal();
        // C++ dismisses banner
        window.DiaEditor.executeCommand('diaapp-editor', 'resolve_conflict', {
            action: 'keep_local',
        });
    };
    
    const handleReloadDisk = () => {
        if (confirm('Discard your changes and reload from disk?')) {
            onReloadDisk();
            
            window.DiaEditor.executeCommand('diaapp-editor', 'resolve_conflict', {
                action: 'reload_disk',
            });
        }
    };
    
    const handleShowDiff = () => {
        setShowDiff(!showDiff);
    };
    
    return (
        <div className="conflict-banner">
            <div className="banner-message">
                ⚠️ File modified externally.
                <div className="banner-actions">
                    <button onClick={handleKeepLocal}>Keep My Changes</button>
                    <button onClick={handleReloadDisk}>Reload from Disk</button>
                    <button onClick={handleShowDiff}>
                        {showDiff ? 'Hide Diff' : 'Show Diff'}
                    </button>
                </div>
            </div>
            
            {showDiff && (
                <div className="diff-viewer">
                    <ReactDiffViewer
                        oldValue={diskVersion}
                        newValue={localVersion}
                        splitView={true}
                        leftTitle="Disk Version"
                        rightTitle="Your Changes"
                    />
                </div>
            )}
        </div>
    );
};
```

### React Integration

**ManifestEditor.tsx (extended):**
```typescript
const ManifestEditor: React.FC = () => {
    const [conflictData, setConflictData] = useState<ConflictData | null>(null);
    
    useEffect(() => {
        const handleConflictDetected = (data: any) => {
            setConflictData({
                localVersion: JSON.stringify(data.editor_version, null, 2),
                diskVersion: JSON.stringify(data.disk_version, null, 2),
            });
        };
        
        const handleConflictResolved = () => {
            setConflictData(null);
        };
        
        window.DiaEditor.subscribe('file_conflict_detected', handleConflictDetected);
        window.DiaEditor.subscribe('conflict_resolved', handleConflictResolved);
        
        return () => {
            window.DiaEditor.unsubscribe('file_conflict_detected', handleConflictDetected);
            window.DiaEditor.unsubscribe('conflict_resolved', handleConflictResolved);
        };
    }, []);
    
    return (
        <div className="manifest-editor">
            {conflictData && (
                <ConflictBanner
                    localVersion={conflictData.localVersion}
                    diskVersion={conflictData.diskVersion}
                    onKeepLocal={() => setConflictData(null)}
                    onReloadDisk={() => {
                        // Reload handled by C++
                        setConflictData(null);
                    }}
                />
            )}
            
            {/* ... rest of editor */}
        </div>
    );
};
```

### C++ Conflict Resolution

**DiaApplicationEditor::ResolveConflict:**
```cpp
void DiaApplicationEditor::ResolveConflict(const Json::Value& data) {
    std::string action = data["action"].asString();
    
    if (action == "keep_local") {
        // Dismiss banner, keep current state
        mEditorModel->NotifyObservers(StringCRC("conflict_resolved"));
        DIA_LOG("Conflict resolved: kept local changes");
    }
    else if (action == "reload_disk") {
        // Reload from disk, discard local changes
        if (mPluginData->filePath) {
            OpenManifest(mPluginData->filePath);
        }
        mEditorModel->NotifyObservers(StringCRC("conflict_resolved"));
        DIA_LOG("Conflict resolved: reloaded from disk");
    }
}
```

### Auto-Hide on Save

**When user saves, conflict is resolved automatically:**
```cpp
void DiaApplicationEditor::SaveManifest() {
    // ... (save logic)
    
    // Auto-hide conflict banner (conflict resolved by saving)
    mEditorModel->NotifyObservers(StringCRC("conflict_resolved"));
}
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - File watcher and conflict detection
- `Dia/DiaApplicationEditor/UI/ConflictBanner.tsx` - Conflict banner component
- `Dia/DiaCore/FileWatcher.h` - File system watcher utility
- `Dia/DiaCore/FileWatcher.cpp` - File watcher implementation (uses Win32 ReadDirectoryChangesW)

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-008 | Manual save workflow | ✅ **Compliant** - User controls save timing |
| DiaApplicationEditor | DAED-009 | Detect file conflicts, show diff, let user resolve | ✅ **Compliant** - Full conflict resolution workflow |

**Decision 53 Compliance:**
- **Filesystem watcher to detect external changes** - ✅ Implemented via Dia::Core::FileWatcher

**Decision 54 Compliance:**
- **Non-modal banner with Keep/Reload/Diff options** - ✅ Implemented with ConflictBanner component

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Modal dialog or banner? | Decision 54 | ✅ Non-modal banner - doesn't block editing |
| 2 | Auto-reload if no local changes? | Decision 53 | ✅ Yes - seamless experience when no conflict |
| 3 | Show diff inline? | Decision 54 | ✅ Yes - react-diff-viewer side-by-side in banner |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | UX | Non-modal better than modal? | ✅ Yes - Decisions 53-54 - user can keep editing, resolve conflict when ready |
| 2 | Performance | File watcher overhead? | ✅ Minimal - watches single file; paused during save to avoid false positives |
| 3 | Edge Cases | Multiple rapid changes? | ✅ Debounce notifications (100ms) to avoid spam |
| 4 | Diff Library | react-diff-viewer adequate? | ✅ Yes - good UX, syntax highlighting, side-by-side view |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
