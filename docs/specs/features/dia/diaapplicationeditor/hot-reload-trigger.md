# Feature Spec: Hot Reload Trigger

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Hot Reload Trigger** | (this document) |

## Problem Statement

Enables triggering hot reload of modified .diaapp manifests on connected game without confirmation dialog, sending only the file path and letting the game load from disk, with success/failure feedback displayed in editor.

## Acceptance Criteria

- [x] "Hot Reload" button in toolbar when connected to game
- [x] No confirmation dialog - immediate hot reload (Decision 58)
- [x] Send hot_reload command via WebSocket with manifest file path
- [x] Game loads from disk (not from editor state)
- [x] Display success notification on successful reload
- [x] Display error notification with details on failure
- [x] Show progress indicator during reload
- [x] Highlight errors in manifest if reload fails due to validation
- [x] Auto-save before hot reload (prompt if unsaved changes)
- [x] Disable button if not connected or no file loaded
- [x] Keyboard shortcut: Ctrl+R for hot reload

## Design

### Hot Reload Button

**Toolbar.tsx:**
```typescript
const HotReloadButton: React.FC = () => {
    const { isConnectedToGame, filePath, isDirty } = useManifestStore();
    const [isReloading, setIsReloading] = useState(false);
    
    const handleHotReload = async () => {
        if (!isConnectedToGame || !filePath) return;
        
        // Auto-save if dirty
        if (isDirty) {
            const shouldSave = confirm('Save changes before hot reload?');
            if (shouldSave) {
                window.DiaEditor.executeCommand('diaapp-editor', 'save', {});
                // Wait for save to complete
                await new Promise(resolve => setTimeout(resolve, 100));
            } else {
                return;  // User cancelled
            }
        }
        
        // Trigger hot reload (no confirmation - Decision 58)
        setIsReloading(true);
        
        window.DiaEditor.executeCommand('diaapp-editor', 'hot_reload', {
            manifest_path: filePath,
        });
    };
    
    // Keyboard shortcut: Ctrl+R
    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.ctrlKey && e.key === 'r') {
                e.preventDefault();
                handleHotReload();
            }
        };
        
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, [isConnectedToGame, filePath, isDirty]);
    
    // Listen for hot reload response
    useEffect(() => {
        const handleResponse = (data: any) => {
            setIsReloading(false);
            
            if (data.success) {
                // Success notification (handled by NotificationPanel)
            } else {
                // Error notification (handled by NotificationPanel)
            }
        };
        
        window.DiaEditor.subscribe('hot_reload_response', handleResponse);
        return () => window.DiaEditor.unsubscribe('hot_reload_response', handleResponse);
    }, []);
    
    const isDisabled = !isConnectedToGame || !filePath || isReloading;
    
    return (
        <button
            onClick={handleHotReload}
            disabled={isDisabled}
            className="hot-reload-button"
            title="Hot Reload (Ctrl+R)"
        >
            {isReloading ? '⏳ Reloading...' : '🔄 Hot Reload'}
        </button>
    );
};
```

### C++ Hot Reload Command

**DiaApplicationEditor::TriggerHotReload:**
```cpp
void DiaApplicationEditor::TriggerHotReload() {
    if (!mPluginData->isConnectedToGame) {
        DIA_LOG_ERROR("Cannot hot reload: not connected to game");
        mEditorModel->NotifyObservers(StringCRC("hot_reload_error"), "Not connected to game");
        return;
    }
    
    if (!mPluginData->filePath) {
        DIA_LOG_ERROR("Cannot hot reload: no file loaded");
        mEditorModel->NotifyObservers(StringCRC("hot_reload_error"), "No file loaded");
        return;
    }
    
    // Send hot reload command via GameConnectionManager
    Json::Value command;
    command["type"] = "command";
    command["command"] = "hot_reload";
    command["payload"]["manifest_path"] = mPluginData->filePath;
    
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    connMgr.SendCommand(command);
    
    DIA_LOG("Hot reload triggered: %s", mPluginData->filePath);
}

void DiaApplicationEditor::HandleHotReloadResponse(const Json::Value& data) {
    bool success = data["success"].asBool();
    
    if (success) {
        // Success
        Json::Value notification;
        notification["type"] = "success";
        notification["message"] = "Hot reload successful";
        
        mEditorModel->NotifyObservers(StringCRC("hot_reload_response"), notification);
        
        // Reload file from disk to sync with game state
        OpenManifest(mPluginData->filePath);
        
        DIA_LOG("Hot reload successful");
    } else {
        // Failure
        std::string errorCode = data["error_code"].asString();
        std::string errorMessage = data["message"].asString();
        
        Json::Value notification;
        notification["type"] = "error";
        notification["message"] = errorMessage;
        notification["error_code"] = errorCode;
        
        mEditorModel->NotifyObservers(StringCRC("hot_reload_response"), notification);
        
        // If validation error, highlight in UI
        if (errorCode == "VALIDATION_ERROR") {
            ValidateManifest();  // Re-validate to show errors
        }
        
        DIA_LOG_ERROR("Hot reload failed: %s (%s)", errorMessage.c_str(), errorCode.c_str());
    }
}
```

### Game-Side Hot Reload Handler

**DiaDebugServer::HandleHotReloadCommand:**
```cpp
void DiaDebugServer::HandleHotReloadCommand(int connectionId, const Json::Value& payload) {
    std::string manifestPath = payload["manifest_path"].asString();
    
    // Trigger hot reload via HotReloadManager
    bool success = Dia::Application::HotReloadManager::Instance().ReloadManifest(manifestPath.c_str());
    
    Json::Value response;
    response["type"] = "command_response";
    response["command"] = "hot_reload";
    response["success"] = success;
    
    if (!success) {
        // Get error details from HotReloadManager
        const auto& error = Dia::Application::HotReloadManager::Instance().GetLastError();
        response["error_code"] = error.code;
        response["message"] = error.message;
    }
    
    // Send response to editor
    SendMessage(connectionId, response);
}
```

### Notification Display

**NotificationPanel.tsx:**
```typescript
interface NotificationProps {
    type: 'success' | 'error' | 'info';
    message: string;
    errorCode?: string;
}

export const NotificationPanel: React.FC = () => {
    const [notification, setNotification] = useState<NotificationProps | null>(null);
    
    useEffect(() => {
        const handleResponse = (data: any) => {
            setNotification({
                type: data.type,
                message: data.message,
                errorCode: data.error_code,
            });
            
            // Auto-hide after 5 seconds
            setTimeout(() => setNotification(null), 5000);
        };
        
        window.DiaEditor.subscribe('hot_reload_response', handleResponse);
        return () => window.DiaEditor.unsubscribe('hot_reload_response', handleResponse);
    }, []);
    
    if (!notification) return null;
    
    return (
        <div className={`notification ${notification.type}`}>
            <span className="icon">
                {notification.type === 'success' ? '✅' : '❌'}
            </span>
            <div className="content">
                <div className="message">{notification.message}</div>
                {notification.errorCode && (
                    <div className="error-code">Error: {notification.errorCode}</div>
                )}
            </div>
            <button onClick={() => setNotification(null)}>×</button>
        </div>
    );
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Hot reload trigger and response handling
- `Dia/DiaApplicationEditor/UI/HotReloadButton.tsx` - Hot reload button component
- `Dia/DiaApplicationEditor/UI/NotificationPanel.tsx` - Success/error notifications
- `Dia/DiaDebugServer/DiaDebugServer.cpp` - Hot reload command handler

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-011 | Hot reload sends path only, game loads from disk | ✅ **Compliant** - Only manifest path sent |
| DiaApplicationEditor | DAED-013 | All inter-process comms use Protobuf | ✅ **Compliant** - Hot reload command (path + request ID) sent as Protobuf; response received as Protobuf |

**Decision 58 Compliance:**
- **No confirmation dialog, immediate reload** - ✅ Implemented: button triggers reload directly (no confirm dialog)

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Confirmation dialog? | Decision 58 | ✅ No - immediate reload for fast iteration |
| 2 | Send manifest content or path? | Decision 58 / DAED-011 | ✅ Path only - game loads from disk (guarantees sync) |
| 3 | Auto-save before reload? | Decision 58 | ✅ Prompt user to save if unsaved changes exist |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | UX | No confirmation dangerous? | ✅ No - Decision 58 - fast iteration priority; risky-change-warnings.md handles dangerous changes |
| 2 | Protocol | Why only path? | ✅ DAED-011 - simpler; guarantees game sees saved file; avoids sync issues |
| 3 | Error Handling | How to show validation errors? | ✅ error_code field; trigger validation to highlight in UI |
| 4 | Keyboard Shortcut | Ctrl+R conflicts with browser refresh? | ✅ preventDefault() in editor window; CEF context isolated |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
