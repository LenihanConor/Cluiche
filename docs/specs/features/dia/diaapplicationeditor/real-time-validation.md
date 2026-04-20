# Feature Spec: Real-Time Validation

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Real-Time Validation** | (this document) |

## Problem Statement

Provides continuous validation of .diaapp manifests with debounced timing (500ms) to prevent performance issues, displaying errors both inline (annotations on nodes/edges) and in a dedicated error panel.

## Acceptance Criteria

- [x] Validate manifest using ApplicationManifestValidator
- [x] Debounce validation (500ms) after user stops editing
- [x] Display validation errors inline in Flow View (red border on invalid nodes)
- [x] Display validation errors inline in Tree View (red icon + tooltip)
- [x] Show all errors in dedicated ValidationPanel (bottom panel)
- [x] Click error in panel to jump to source location (select node in view)
- [x] Show error count badge in toolbar
- [x] Distinguish errors vs warnings (red vs yellow)
- [x] Validate JSON syntax in CodeMirror (module-config-editor)
- [x] Block save if critical errors exist
- [x] Auto-validate on manifest load
- [x] Manual "Validate Now" button to bypass debounce

## Design

### Debounced Validation Hook

**useValidation.ts:**
```typescript
import { useEffect } from 'react';
import { useDebounce } from 'use-debounce';
import { useManifestStore } from './ManifestStore';

export const useValidation = (manifest: ApplicationManifest | null) => {
    const { setValidationResult } = useManifestStore();
    
    // Debounce manifest changes (500ms)
    const [debouncedManifest] = useDebounce(manifest, 500);
    
    useEffect(() => {
        if (!debouncedManifest) return;
        
        // Trigger validation in C++
        window.DiaEditor.executeCommand('diaapp-editor', 'validate', {});
        
        // C++ will call back with validation result
    }, [debouncedManifest]);
};
```

### C++ Validation Handler

**DiaApplicationEditor::ValidateManifest:**
```cpp
bool DiaApplicationEditor::ValidateManifest() {
    if (!mPluginData->manifest) {
        DIA_LOG_WARNING("No manifest loaded, skipping validation");
        return false;
    }
    
    // Clear previous results
    mPluginData->validationResult.errors.Clear();
    mPluginData->validationResult.warnings.Clear();
    
    // Use ApplicationManifestValidator
    Dia::Application::ManifestValidator validator;
    
    bool isValid = validator.Validate(
        mPluginData->manifest,
        &mPluginData->validationResult.errors,
        &mPluginData->validationResult.warnings
    );
    
    mPluginData->validationResult.isValid = isValid;
    
    // Convert errors to JSON format with line numbers and paths
    Json::Value resultJson;
    resultJson["is_valid"] = isValid;
    
    Json::Value errorsJson(Json::arrayValue);
    for (const auto& error : mPluginData->validationResult.errors) {
        Json::Value errorJson;
        errorJson["message"] = error.message;
        errorJson["path"] = error.path;  // e.g., "processing_units[0].phases[2]"
        errorJson["line"] = error.lineNumber;
        errorJson["severity"] = "error";
        errorsJson.append(errorJson);
    }
    resultJson["errors"] = errorsJson;
    
    Json::Value warningsJson(Json::arrayValue);
    for (const auto& warning : mPluginData->validationResult.warnings) {
        Json::Value warningJson;
        warningJson["message"] = warning.message;
        warningJson["path"] = warning.path;
        warningJson["line"] = warning.lineNumber;
        warningJson["severity"] = "warning";
        warningsJson.append(warningJson);
    }
    resultJson["warnings"] = warningsJson;
    
    // Notify UI
    mEditorModel->NotifyObservers(StringCRC("validation_complete"), resultJson);
    
    DIA_LOG("Validation complete: %d errors, %d warnings",
            mPluginData->validationResult.errors.GetSize(),
            mPluginData->validationResult.warnings.GetSize());
    
    return isValid;
}
```

### ValidationPanel Component

**ValidationPanel.tsx:**
```typescript
import React from 'react';
import { ValidationResult, ValidationError } from './types';
import { useManifestStore } from './ManifestStore';

interface ValidationPanelProps {
    result: ValidationResult;
}

export const ValidationPanel: React.FC<ValidationPanelProps> = ({ result }) => {
    const { setSelectedNode } = useManifestStore();
    
    const handleErrorClick = (error: ValidationError) => {
        // Parse path to extract node ID
        // e.g., "processing_units[0].phases[2].modules[1]"
        const nodeId = parsePathToNodeId(error.path);
        
        if (nodeId) {
            setSelectedNode(nodeId);
            
            // Scroll to node in view
            window.DiaEditor.executeCommand('diaapp-editor', 'scroll_to_node', {
                node_id: nodeId,
            });
        }
    };
    
    const allIssues = [
        ...result.errors.map(e => ({ ...e, severity: 'error' })),
        ...result.warnings.map(w => ({ ...w, severity: 'warning' })),
    ];
    
    if (allIssues.length === 0) {
        return (
            <div className="validation-panel success">
                ✅ No validation errors
            </div>
        );
    }
    
    return (
        <div className="validation-panel">
            <div className="panel-header">
                <h3>Validation Issues</h3>
                <span className="issue-count">
                    {result.errors.length} error(s), {result.warnings.length} warning(s)
                </span>
            </div>
            
            <div className="issue-list">
                {allIssues.map((issue, index) => (
                    <div
                        key={index}
                        className={`issue-item ${issue.severity}`}
                        onClick={() => handleErrorClick(issue)}
                    >
                        <span className="icon">
                            {issue.severity === 'error' ? '❌' : '⚠️'}
                        </span>
                        <div className="issue-content">
                            <div className="message">{issue.message}</div>
                            <div className="location">
                                {issue.path} {issue.line > 0 && `(line ${issue.line})`}
                            </div>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};

function parsePathToNodeId(path: string): string | null {
    // Parse JSON path to node ID
    // "processing_units[0].phases[2]" → "MainProcessingUnit_UpdatePhase"
    // Implementation depends on manifest structure
    // ...
    return null;
}
```

### Inline Error Display in Flow View

**FlowView.tsx (extended):**
```typescript
const getNodeStyle = (node: Node, validationResult: ValidationResult): React.CSSProperties => {
    const hasError = validationResult.errors.some(error =>
        error.path.includes(node.id)
    );
    
    const hasWarning = validationResult.warnings.some(warning =>
        warning.path.includes(node.id)
    );
    
    return {
        border: hasError ? '3px solid #f44336' : (hasWarning ? '2px dashed #ff9800' : undefined),
        boxShadow: hasError ? '0 0 10px rgba(244, 67, 54, 0.5)' : undefined,
    };
};

// Apply style to PhaseNode
<PhaseNode
    data={node.data}
    style={getNodeStyle(node, validationResult)}
/>
```

### Inline Error Display in Tree View

**TreeNodeRenderer.tsx (extended):**
```typescript
const getNodeIcon = (node: TreeNode, validationResult: ValidationResult): string => {
    const hasError = validationResult.errors.some(error =>
        error.path.includes(node.id)
    );
    
    const hasWarning = validationResult.warnings.some(warning =>
        warning.path.includes(node.id)
    );
    
    if (hasError) return '❌';
    if (hasWarning) return '⚠️';
    
    // Default icons
    switch (node.type) {
        case 'processing_unit': return '📦';
        case 'phase': return '🔄';
        case 'module': return '⚙️';
        default: return '';
    }
};
```

### Error Count Badge

**Toolbar.tsx:**
```typescript
const ErrorBadge: React.FC = () => {
    const { validationResult } = useManifestStore();
    
    if (!validationResult || validationResult.errors.length === 0) {
        return null;
    }
    
    return (
        <div className="error-badge" title={`${validationResult.errors.length} error(s)`}>
            ❌ {validationResult.errors.length}
        </div>
    );
};
```

### Manual Validation Button

**ManifestEditor.tsx:**
```typescript
const handleValidateNow = () => {
    window.DiaEditor.executeCommand('diaapp-editor', 'validate', {});
};

<button onClick={handleValidateNow} title="Validate Now (bypass debounce)">
    Validate
</button>
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Validation logic using ApplicationManifestValidator
- `Dia/DiaApplicationEditor/UI/ValidationPanel.tsx` - Error panel component
- `Dia/DiaApplicationEditor/UI/useValidation.ts` - Debounced validation hook
- `Dia/DiaApplicationEditor/UI/FlowView.tsx` - Inline error display in flow view
- `Dia/DiaApplicationEditor/UI/TreeView.tsx` - Inline error display in tree view

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaApplicationEditor | DAED-001 | Reuse ApplicationManifestValidator | ✅ **Compliant** - Uses validator directly |
| DiaApplicationEditor | DAED-005 | Display validation errors inline | ✅ **Compliant** - Inline + panel |
| DiaApplicationEditor | DAED-006 | Runtime config changes debounced (500ms) | ✅ **Compliant** - Applied to validation too |

**Decision 51 Compliance:**
- **Debounced validation (500ms)** - ✅ Implemented via useDebounce hook

**Decision 52 Compliance:**
- **Inline annotations + error panel** - ✅ Both implemented; inline shows visual cues, panel shows full details

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Debounce timing? | Decision 51 | ✅ 500ms - matches config debounce; prevents lag during typing |
| 2 | Inline or panel errors? | Decision 52 | ✅ Both - inline for quick feedback, panel for full details |
| 3 | Block save on errors? | Decision 52 | ✅ Yes - critical errors block save; warnings allow save |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | 500ms too slow? | ✅ No - Decisions 51-52 - feels responsive; prevents excessive validation during typing |
| 2 | UX | Inline errors enough? | ✅ No - panel provides full details and click-to-navigate |
| 3 | Validation Logic | Reuse ApplicationManifestValidator? | ✅ Yes - DAED-001 - single source of truth for validation rules |
| 4 | Manual Trigger | Needed? | ✅ Yes - allows immediate validation without waiting for debounce |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
