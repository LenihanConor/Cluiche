# Feature Spec: Risky Change Warnings

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Risky Change Warnings** | (this document) |

## Problem Statement

Warns users when triggering hot reload with potentially risky structural changes (add/remove module, change phase transitions, reorder execution), displaying a summary of changes and allowing users to proceed with awareness of potential crashes.

## Acceptance Criteria

- [x] Detect risky changes when user clicks "Hot Reload" button
- [x] Show warning dialog with change summary before hot reload executes
- [x] Display categories: Added Modules, Removed Modules, Changed Transitions, Reordered Phases
- [x] User can Proceed or Cancel hot reload
- [x] No warning for safe config-only changes (handled by runtime-config-push)
- [x] Warning appears only on hot reload (not during regular editing)
- [x] Store last-known game state to enable accurate diff
- [x] Highlight specific risky changes (e.g., "Removed PhysicsModule from UpdatePhase")
- [x] "Don't warn me again" checkbox (session-only, not persistent)

## Design

### Risky Change Detection

**DiaApplicationEditor::DetectRiskyChanges:**
```cpp
struct RiskyChangeReport {
    bool hasRiskyChanges;
    
    DynamicArrayC<const char*, 16> addedModules;
    DynamicArrayC<const char*, 16> removedModules;
    DynamicArrayC<const char*, 16> changedTransitions;
    DynamicArrayC<const char*, 16> reorderedPhases;
};

RiskyChangeReport DiaApplicationEditor::DetectRiskyChanges() {
    RiskyChangeReport report;
    report.hasRiskyChanges = false;
    
    if (!mPluginData->manifest || !mPluginData->lastKnownGameState) {
        return report;  // Can't compare - no baseline
    }
    
    const auto* currentManifest = mPluginData->manifest;
    const auto* gameManifest = mPluginData->lastKnownGameState;
    
    // Compare manifests
    for (const auto& pu : currentManifest->processingUnits) {
        const auto* gamePU = FindProcessingUnit(gameManifest, pu.id);
        if (!gamePU) continue;
        
        // Check for added/removed modules
        for (const auto& phase : pu.phases) {
            const auto* gamePhase = FindPhase(gamePU, phase.instanceId);
            if (!gamePhase) continue;
            
            // Added modules
            for (const auto& module : phase.modules) {
                if (!FindModule(gamePhase, module.instanceId)) {
                    report.addedModules.Add(
                        Dia::Core::String::Format("%s.%s.%s",
                            pu.id, phase.instanceId, module.instanceId).GetData()
                    );
                    report.hasRiskyChanges = true;
                }
            }
            
            // Removed modules
            for (const auto& gameModule : gamePhase->modules) {
                if (!FindModule(&phase, gameModule.instanceId)) {
                    report.removedModules.Add(
                        Dia::Core::String::Format("%s.%s.%s",
                            pu.id, phase.instanceId, gameModule.instanceId).GetData()
                    );
                    report.hasRiskyChanges = true;
                }
            }
            
            // Changed transitions
            for (const auto& transition : phase.transitions) {
                if (!FindTransition(gamePhase, transition.toPhase)) {
                    report.changedTransitions.Add(
                        Dia::Core::String::Format("%s: %s → %s",
                            pu.id, phase.instanceId, transition.toPhase.GetString()).GetData()
                    );
                    report.hasRiskyChanges = true;
                }
            }
        }
        
        // Check for phase reordering
        if (!PhasesInSameOrder(pu, *gamePU)) {
            report.reorderedPhases.Add(Dia::Core::String::Format("%s phases reordered", pu.id).GetData());
            report.hasRiskyChanges = true;
        }
    }
    
    return report;
}
```

### Hot Reload with Warning

**DiaApplicationEditor::TriggerHotReload (extended):**
```cpp
void DiaApplicationEditor::TriggerHotReload() {
    // Detect risky changes
    RiskyChangeReport report = DetectRiskyChanges();
    
    if (report.hasRiskyChanges && !mSuppressRiskyChangeWarnings) {
        // Serialize report for UI
        Json::Value reportJson;
        reportJson["has_risky_changes"] = true;
        
        Json::Value addedModules(Json::arrayValue);
        for (const auto& change : report.addedModules) {
            addedModules.append(change);
        }
        reportJson["added_modules"] = addedModules;
        
        Json::Value removedModules(Json::arrayValue);
        for (const auto& change : report.removedModules) {
            removedModules.append(change);
        }
        reportJson["removed_modules"] = removedModules;
        
        Json::Value changedTransitions(Json::arrayValue);
        for (const auto& change : report.changedTransitions) {
            changedTransitions.append(change);
        }
        reportJson["changed_transitions"] = changedTransitions;
        
        Json::Value reorderedPhases(Json::arrayValue);
        for (const auto& change : report.reorderedPhases) {
            reorderedPhases.append(change);
        }
        reportJson["reordered_phases"] = reorderedPhases;
        
        // Show warning dialog (async)
        mEditorModel->NotifyObservers(StringCRC("show_risky_change_warning"), reportJson);
        return;  // User will confirm via callback
    }
    
    // No risky changes or warnings suppressed - proceed immediately
    ExecuteHotReload();
}

void DiaApplicationEditor::ConfirmHotReload(bool suppressFutureWarnings) {
    if (suppressFutureWarnings) {
        mSuppressRiskyChangeWarnings = true;  // Session-only
    }
    
    ExecuteHotReload();
}

void DiaApplicationEditor::ExecuteHotReload() {
    // ... (existing hot reload logic)
}
```

### React Warning Dialog

**RiskyChangeWarningDialog.tsx:**
```typescript
import React, { useState } from 'react';

interface RiskyChangeReport {
    has_risky_changes: boolean;
    added_modules: string[];
    removed_modules: string[];
    changed_transitions: string[];
    reordered_phases: string[];
}

interface RiskyChangeWarningDialogProps {
    report: RiskyChangeReport;
    onProceed: (suppressWarnings: boolean) => void;
    onCancel: () => void;
}

export const RiskyChangeWarningDialog: React.FC<RiskyChangeWarningDialogProps> = ({
    report,
    onProceed,
    onCancel
}) => {
    const [suppressWarnings, setSuppressWarnings] = useState(false);
    
    const handleProceed = () => {
        onProceed(suppressWarnings);
    };
    
    return (
        <div className="risky-change-warning-dialog">
            <div className="dialog-header">
                <h2>⚠️ Risky Hot Reload</h2>
            </div>
            
            <div className="dialog-body">
                <p>
                    The following structural changes may cause the game to crash or behave unexpectedly:
                </p>
                
                {report.added_modules.length > 0 && (
                    <div className="change-category">
                        <h4>Added Modules:</h4>
                        <ul>
                            {report.added_modules.map((change, i) => (
                                <li key={i} className="added">{change}</li>
                            ))}
                        </ul>
                    </div>
                )}
                
                {report.removed_modules.length > 0 && (
                    <div className="change-category">
                        <h4>Removed Modules:</h4>
                        <ul>
                            {report.removed_modules.map((change, i) => (
                                <li key={i} className="removed">{change}</li>
                            ))}
                        </ul>
                    </div>
                )}
                
                {report.changed_transitions.length > 0 && (
                    <div className="change-category">
                        <h4>Changed Transitions:</h4>
                        <ul>
                            {report.changed_transitions.map((change, i) => (
                                <li key={i} className="changed">{change}</li>
                            ))}
                        </ul>
                    </div>
                )}
                
                {report.reordered_phases.length > 0 && (
                    <div className="change-category">
                        <h4>Reordered Phases:</h4>
                        <ul>
                            {report.reordered_phases.map((change, i) => (
                                <li key={i} className="reordered">{change}</li>
                            ))}
                        </ul>
                    </div>
                )}
                
                <div className="warning-note">
                    <strong>Recommendation:</strong> Save your game state before proceeding.
                </div>
            </div>
            
            <div className="dialog-footer">
                <label className="suppress-checkbox">
                    <input
                        type="checkbox"
                        checked={suppressWarnings}
                        onChange={(e) => setSuppressWarnings(e.target.checked)}
                    />
                    Don't warn me again (this session)
                </label>
                
                <div className="dialog-actions">
                    <button onClick={onCancel} className="cancel-button">
                        Cancel
                    </button>
                    <button onClick={handleProceed} className="proceed-button">
                        Proceed with Hot Reload
                    </button>
                </div>
            </div>
        </div>
    );
};
```

### Integration in ManifestEditor

**ManifestEditor.tsx (extended):**
```typescript
const ManifestEditor: React.FC = () => {
    const [riskyChangeReport, setRiskyChangeReport] = useState<RiskyChangeReport | null>(null);
    
    useEffect(() => {
        const handleWarning = (data: RiskyChangeReport) => {
            setRiskyChangeReport(data);
        };
        
        window.DiaEditor.subscribe('show_risky_change_warning', handleWarning);
        return () => window.DiaEditor.unsubscribe('show_risky_change_warning', handleWarning);
    }, []);
    
    const handleProceedWithHotReload = (suppressWarnings: boolean) => {
        window.DiaEditor.executeCommand('diaapp-editor', 'confirm_hot_reload', {
            suppress_warnings: suppressWarnings,
        });
        setRiskyChangeReport(null);
    };
    
    const handleCancelHotReload = () => {
        setRiskyChangeReport(null);
    };
    
    return (
        <div className="manifest-editor">
            {/* ... existing editor UI ... */}
            
            {riskyChangeReport && (
                <RiskyChangeWarningDialog
                    report={riskyChangeReport}
                    onProceed={handleProceedWithHotReload}
                    onCancel={handleCancelHotReload}
                />
            )}
        </div>
    );
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Risky change detection and warning logic
- `Dia/DiaApplicationEditor/UI/RiskyChangeWarningDialog.tsx` - Warning dialog component
- `Dia/DiaApplicationEditor/UI/ManifestEditor.tsx` - Dialog integration

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-010 | Warn on risky runtime changes | ✅ **Compliant** - Warns before hot reload |

**Decision 60 Compliance:**
- **Warning on hot reload only, show change summary** - ✅ Implemented: warning appears when user clicks Hot Reload button; displays categorized change list

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | When to warn? | Decision 60 | ✅ Only when clicking Hot Reload button (not during editing) |
| 2 | What to show? | Decision 60 | ✅ Categorized change summary: added/removed modules, transitions, reordered phases |
| 3 | Persistent suppress? | Decision 60 | ✅ No - session-only suppress (checkbox resets on editor restart) |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | UX | Warning too intrusive? | ✅ No - Decision 60 - only appears on hot reload; users can suppress for session |
| 2 | Detection | Accurate diff? | ✅ Yes - compares current manifest with last-known game state from WebSocket |
| 3 | Categories | Right level of detail? | ✅ Yes - grouped by risk type; specific changes listed (e.g., "Removed PhysicsModule from UpdatePhase") |
| 4 | Safe Changes | Config-only changes trigger warning? | ✅ No - Decision 59 - config values pushed via runtime-config-push; only structural changes warn |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
