# Feature Spec: Runtime State Inspector

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Runtime State Inspector** | (this document) |

## Problem Statement

Displays live runtime state of ProcessingUnits, Phases, and Modules from connected game in a dual-pane layout (Flow View + Inspector Panel), allowing developers to monitor module configuration and execution status in real-time.

## Acceptance Criteria

- [x] Subscribe to processing_unit_state events from DiaDebugServer
- [x] Display current ProcessingUnit state (active, paused, stopped)
- [x] Show current phase for each ProcessingUnit
- [x] Display list of active modules with runtime status
- [x] Click module in Flow View to open inspector panel with live config
- [x] Show module execution metrics (calls per second, avg time)
- [x] Display module config as read-only (live state, not editable directly)
- [x] Highlight differences between manifest config and runtime config
- [x] Update state in real-time as game broadcasts changes
- [x] Show "Live" badge when viewing runtime state vs static manifest

## Design

### State Subscription

**DiaApplicationEditor::OnGameConnected:**
```cpp
void DiaApplicationEditor::OnGameConnected() {
    // Subscribe to processing unit state updates
    Json::Value subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["event"] = "processing_unit_state";
    
    auto& connMgr = Dia::Editor::GameConnectionManager::Instance();
    connMgr.SendCommand(subscribeMsg);
    
    DIA_LOG("Subscribed to processing_unit_state events");
}

void DiaApplicationEditor::HandleProcessingUnitState(const Json::Value& data) {
    // Parse runtime state
    std::string puId = data["processing_unit_id"].asString();
    std::string currentPhase = data["current_phase"].asString();
    std::string state = data["state"].asString();  // "active", "paused", "stopped"
    
    // Update live state
    mPluginData->liveState.currentProcessingUnit = StringCRC(puId.c_str());
    mPluginData->liveState.currentPhase = StringCRC(currentPhase.c_str());
    
    // Parse module states
    const Json::Value& modules = data["modules"];
    mPluginData->liveState.activeModules.Clear();
    
    for (const auto& moduleData : modules) {
        ModuleRuntimeState moduleState;
        moduleState.instanceId = StringCRC(moduleData["instance_id"].asCString());
        moduleState.isActive = moduleData["is_active"].asBool();
        moduleState.callsPerSecond = moduleData["calls_per_second"].asFloat();
        moduleState.avgTimeMs = moduleData["avg_time_ms"].asFloat();
        moduleState.config = moduleData["config"];  // Live config
        
        mPluginData->liveState.moduleStates.Add(moduleState);
    }
    
    // Notify UI
    mEditorModel->NotifyObservers(StringCRC("runtime_state_updated"), data);
}
```

### RuntimeStateInspector Component

**RuntimeStateInspector.tsx:**
```typescript
import React, { useState, useEffect } from 'react';
import { ModuleRuntimeState } from './types';

interface RuntimeStateInspectorProps {
    selectedModuleId: string | null;
    isLive: boolean;
}

export const RuntimeStateInspector: React.FC<RuntimeStateInspectorProps> = ({
    selectedModuleId,
    isLive
}) => {
    const [runtimeState, setRuntimeState] = useState<ModuleRuntimeState | null>(null);
    const [manifestConfig, setManifestConfig] = useState<any>(null);
    
    useEffect(() => {
        if (!selectedModuleId || !isLive) return;
        
        const handleStateUpdate = () => {
            const data = window.DiaEditor.getPluginData('DiaApplicationEditor');
            
            // Find module runtime state
            const moduleState = data.liveState.moduleStates.find(
                (m: any) => m.instanceId === selectedModuleId
            );
            
            setRuntimeState(moduleState);
            
            // Load manifest config for comparison
            const manifestModule = findModuleInManifest(data.manifest, selectedModuleId);
            setManifestConfig(manifestModule?.config || {});
        };
        
        window.DiaEditor.subscribe('runtime_state_updated', handleStateUpdate);
        handleStateUpdate();  // Initial load
        
        return () => {
            window.DiaEditor.unsubscribe('runtime_state_updated', handleStateUpdate);
        };
    }, [selectedModuleId, isLive]);
    
    if (!runtimeState) {
        return (
            <div className="runtime-state-inspector empty">
                {isLive ? 'Select a module to view runtime state' : 'Connect to game for live state'}
            </div>
        );
    }
    
    return (
        <div className="runtime-state-inspector">
            <div className="inspector-header">
                <h3>{runtimeState.instanceId}</h3>
                <span className="live-badge">🔴 LIVE</span>
            </div>
            
            <div className="state-section">
                <h4>Execution Metrics</h4>
                <div className="metric">
                    <span className="label">Status:</span>
                    <span className={`value ${runtimeState.isActive ? 'active' : 'inactive'}`}>
                        {runtimeState.isActive ? 'Active' : 'Inactive'}
                    </span>
                </div>
                <div className="metric">
                    <span className="label">Calls/sec:</span>
                    <span className="value">{runtimeState.callsPerSecond.toFixed(1)}</span>
                </div>
                <div className="metric">
                    <span className="label">Avg Time:</span>
                    <span className="value">{runtimeState.avgTimeMs.toFixed(2)}ms</span>
                </div>
            </div>
            
            <div className="config-section">
                <h4>Live Configuration</h4>
                <ConfigComparison
                    liveConfig={runtimeState.config}
                    manifestConfig={manifestConfig}
                />
            </div>
        </div>
    );
};

function findModuleInManifest(manifest: any, moduleId: string): any {
    // Search manifest for module by instance_id
    // ...
    return null;
}
```

### Config Comparison Component

**ConfigComparison.tsx:**
```typescript
import React from 'react';
import { diffJson } from 'diff';

interface ConfigComparisonProps {
    liveConfig: any;
    manifestConfig: any;
}

export const ConfigComparison: React.FC<ConfigComparisonProps> = ({
    liveConfig,
    manifestConfig
}) => {
    const diff = diffJson(manifestConfig, liveConfig);
    
    return (
        <div className="config-comparison">
            <div className="config-diff">
                {diff.map((part, index) => {
                    const className = part.added ? 'added' : part.removed ? 'removed' : 'unchanged';
                    return (
                        <span key={index} className={className}>
                            {part.value}
                        </span>
                    );
                })}
            </div>
            
            {diff.some(part => part.added || part.removed) && (
                <div className="diff-legend">
                    <span className="added">● Added in runtime</span>
                    <span className="removed">● Removed from manifest</span>
                </div>
            )}
        </div>
    );
};
```

### Dual-Pane Layout

**LiveDebuggerLayout.tsx:**
```typescript
import React from 'react';
import { FlowView } from './FlowView';
import { RuntimeStateInspector } from './RuntimeStateInspector';
import { useManifestStore } from './ManifestStore';

export const LiveDebuggerLayout: React.FC = () => {
    const { manifest, selectedNodeId, currentPhaseId, isConnectedToGame } = useManifestStore();
    
    return (
        <div className="live-debugger-layout">
            <div className="flow-pane">
                <FlowView
                    manifest={manifest}
                    selectedPhaseId={selectedNodeId}
                    currentPhaseId={currentPhaseId}
                    onSelectPhase={(id) => useManifestStore.getState().setSelectedNode(id)}
                />
            </div>
            
            <div className="inspector-pane">
                <RuntimeStateInspector
                    selectedModuleId={selectedNodeId}
                    isLive={isConnectedToGame}
                />
            </div>
        </div>
    );
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - State subscription and parsing
- `Dia/DiaApplicationEditor/UI/RuntimeStateInspector.tsx` - Live state display
- `Dia/DiaApplicationEditor/UI/ConfigComparison.tsx` - Diff view for live vs manifest config
- `Dia/DiaApplicationEditor/UI/LiveDebuggerLayout.tsx` - Dual-pane layout

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Module IDs use StringCRC |

**Decision 57 Compliance:**
- **Dual pane: Flow View + Inspector** - ✅ Implemented with LiveDebuggerLayout split layout

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Layout: side-by-side or stacked? | Decision 57 | ✅ Side-by-side: Flow on left, Inspector on right |
| 2 | Show live vs manifest diff? | Decision 57 | ✅ Yes - highlight differences in ConfigComparison |
| 3 | Execution metrics? | Decision 57 | ✅ Calls/sec, avg time, active status |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | UX | Dual pane better than tabs? | ✅ Decision 57 - allows viewing flow and state simultaneously |
| 2 | Performance | Real-time updates lag? | ✅ No - event-driven; game broadcasts only on change |
| 3 | Diff View | Too complex? | ✅ No - uses standard diff library; clear visual indicators |
| 4 | Metrics | Which metrics matter? | ✅ Calls/sec (activity), avg time (performance), active status (state) |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
