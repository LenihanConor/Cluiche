# Feature Spec: Runtime Config Push

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Runtime Config Push** | (this document) |

## Problem Statement

Pushes module configuration value changes to live game with debounced timing (500ms), allowing immediate testing of config tweaks without full hot reload. Only handles config value changes; structural changes (add/remove module) require hot reload.

## Acceptance Criteria

- [x] Push config changes when user edits module config in ModuleInspector
- [x] Debounce config push (500ms) to avoid excessive network traffic
- [x] Only push config value changes (not structural changes)
- [x] Display "pending" indicator during debounce period
- [x] Show success/failure feedback after push
- [x] Module must implement CanHotUpdateConfig() to support config push
- [x] Fall back to hot reload if module doesn't support config push
- [x] Mark manifest dirty after successful config push
- [x] Show error if game rejects config (validation failure)
- [x] "Apply to Game" button in ModuleInspector (optional immediate push)

## Design

### Debounced Config Push

**ModuleInspector.tsx (extended):**
```typescript
export const ModuleInspector: React.FC<ModuleInspectorProps> = ({
    module,
    isConnectedToGame
}) => {
    const [configJson, setConfigJson] = useState<string>('');
    const [isPending, setIsPending] = useState(false);
    const [debouncedConfig] = useDebounce(configJson, 500);  // 500ms debounce
    
    // Apply debounced config changes
    useEffect(() => {
        if (!module || !debouncedConfig || !isConnectedToGame) return;
        
        try {
            const parsedConfig = JSON.parse(debouncedConfig);
            
            // Send to C++ for runtime push
            window.DiaEditor.executeCommand('diaapp-editor', 'push_config_to_game', {
                pu_id: module.processingUnitId,
                module_id: module.instance_id,
                config: parsedConfig,
            });
            
            setIsPending(false);
        } catch (e) {
            console.error('Invalid JSON:', e);
        }
    }, [debouncedConfig, module, isConnectedToGame]);
    
    // Manual "Apply to Game" button (bypass debounce)
    const handleApplyToGame = () => {
        if (!module || !isConnectedToGame) return;
        
        try {
            const parsedConfig = JSON.parse(configJson);
            
            window.DiaEditor.executeCommand('diaapp-editor', 'push_config_to_game', {
                pu_id: module.processingUnitId,
                module_id: module.instance_id,
                config: parsedConfig,
            });
        } catch (e) {
            alert('Invalid JSON - cannot apply to game');
        }
    };
    
    return (
        <div className="module-inspector">
            {/* ... existing inspector UI ... */}
            
            {isPending && <span className="pending-indicator">⏱ Saving...</span>}
            
            {isConnectedToGame && (
                <button
                    onClick={handleApplyToGame}
                    className="apply-to-game-button"
                    title="Push config to running game immediately"
                >
                    Apply to Game
                </button>
            )}
        </div>
    );
};
```

### C++ Config Push Handler

**DiaApplicationEditor::PushConfigToGame:**
```cpp
void DiaApplicationEditor::PushConfigToGame(const Json::Value& data) {
    if (!mPluginData->isConnectedToGame) {
        DIA_LOG_ERROR("Cannot push config: not connected to game");
        return;
    }
    
    std::string puId = data["pu_id"].asString();
    std::string moduleId = data["module_id"].asString();
    const Json::Value& newConfig = data["config"];
    
    // Send update_config command via GameConnectionManager
    Json::Value command;
    command["type"] = "command";
    command["command"] = "update_config";
    command["payload"]["pu_id"] = puId;
    command["payload"]["module_id"] = moduleId;
    command["payload"]["config"] = newConfig;
    
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    connMgr.SendCommand(command);
    
    // Update local manifest
    Module* module = FindModule(puId.c_str(), moduleId.c_str());
    if (module) {
        module->config = newConfig;
        MarkDirty();
    }
    
    DIA_LOG("Pushed config to game: %s.%s", puId.c_str(), moduleId.c_str());
}

void DiaApplicationEditor::HandleConfigPushResponse(const Json::Value& data) {
    bool success = data["success"].asBool();
    
    if (success) {
        // Success notification
        Json::Value notification;
        notification["type"] = "success";
        notification["message"] = "Config updated in game";
        
        mEditorModel->NotifyObservers(StringCRC("config_push_response"), notification);
    } else {
        // Failure - module doesn't support hot config update
        std::string errorCode = data["error_code"].asString();
        std::string errorMessage = data["message"].asString();
        
        Json::Value notification;
        notification["type"] = "error";
        notification["message"] = errorMessage;
        notification["error_code"] = errorCode;
        
        if (errorCode == "CONFIG_PUSH_NOT_SUPPORTED") {
            notification["message"] = "Module doesn't support hot config update. Use Hot Reload instead.";
        }
        
        mEditorModel->NotifyObservers(StringCRC("config_push_response"), notification);
        
        DIA_LOG_ERROR("Config push failed: %s (%s)", errorMessage.c_str(), errorCode.c_str());
    }
}
```

### Game-Side Config Push Handler

**DiaDebugServer::HandleUpdateConfigCommand:**
```cpp
void DiaDebugServer::HandleUpdateConfigCommand(int connectionId, const Json::Value& payload) {
    std::string puId = payload["pu_id"].asString();
    std::string moduleId = payload["module_id"].asString();
    const Json::Value& newConfig = payload["config"];
    
    // Find module in game
    auto& app = Dia::Application::ApplicationRuntime::Instance();
    ProcessingUnit* pu = app.GetProcessingUnit(StringCRC(puId.c_str()));
    
    if (!pu) {
        SendError(connectionId, "update_config", "PROCESSING_UNIT_NOT_FOUND", "Processing unit not found");
        return;
    }
    
    Module* module = pu->GetModule(StringCRC(moduleId.c_str()));
    
    if (!module) {
        SendError(connectionId, "update_config", "MODULE_NOT_FOUND", "Module not found");
        return;
    }
    
    // Check if module supports hot config update
    if (!module->CanHotUpdateConfig()) {
        SendError(connectionId, "update_config", "CONFIG_PUSH_NOT_SUPPORTED",
                  "Module doesn't support hot config update");
        return;
    }
    
    // Apply config
    bool success = module->UpdateConfig(newConfig);
    
    Json::Value response;
    response["type"] = "command_response";
    response["command"] = "update_config";
    response["success"] = success;
    
    if (!success) {
        response["error_code"] = "CONFIG_UPDATE_FAILED";
        response["message"] = "Module rejected config update";
    }
    
    // Broadcast updated state to all connected editors
    if (success) {
        BroadcastProcessingUnitState(pu);
    }
    
    SendMessage(connectionId, response);
}
```

### Module Interface Extension

**Module.h (extended):**
```cpp
namespace Dia::Application {
    class Module {
    public:
        // Hot config update support
        virtual bool CanHotUpdateConfig() const { return false; }  // Opt-in
        virtual bool UpdateConfig(const Json::Value& newConfig) { return false; }
        
    protected:
        Json::Value mConfig;
    };
}
```

**Example Module Implementation:**
```cpp
class DebugServerModule : public Module {
public:
    bool CanHotUpdateConfig() const override { return true; }
    
    bool UpdateConfig(const Json::Value& newConfig) override {
        // Update port (requires restart)
        if (newConfig.isMember("port")) {
            int newPort = newConfig["port"].asInt();
            if (newPort != mPort) {
                return false;  // Port change requires hot reload, not config push
            }
        }
        
        // Update auto_start (safe to change at runtime)
        if (newConfig.isMember("auto_start")) {
            mAutoStart = newConfig["auto_start"].asBool();
        }
        
        mConfig = newConfig;
        return true;
    }
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Config push command and response handling
- `Dia/DiaApplicationEditor/UI/ModuleInspector.tsx` - Debounced config editing
- `Dia/DiaDebugServer/DiaDebugServer.cpp` - update_config command handler
- `Dia/DiaApplication/Module.h` - CanHotUpdateConfig() and UpdateConfig() interface

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-006 | Runtime config changes debounced (500ms) | ✅ **Compliant** - Debounced with react hook |
| DiaApplicationEditor | DAED-007 | Hybrid hot-update: modules opt-in | ✅ **Compliant** - CanHotUpdateConfig() opt-in |

**Decision 59 Compliance:**
- **Config values only; structural changes require hot reload** - ✅ Implemented: only pushes JSON config; modules validate and reject unsafe changes

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Debounce timing? | Decision 59 | ✅ 500ms - matches real-time-validation and module-config-editor |
| 2 | Opt-in or opt-out? | Decision 59 / DAED-007 | ✅ Opt-in via CanHotUpdateConfig() - safer default |
| 3 | Structural changes? | Decision 59 | ✅ Require hot reload - config push only for values |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Safety | Can modules reject config? | ✅ Yes - UpdateConfig() returns bool; module validates safety |
| 2 | UX | Debounce vs immediate? | ✅ Decision 59 - debounced 500ms for responsiveness; "Apply to Game" button for immediate |
| 3 | Fallback | What if module doesn't support? | ✅ Error message suggests hot reload instead |
| 4 | Performance | Network overhead? | ✅ Minimal - debounced; only sends when user stops typing |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
