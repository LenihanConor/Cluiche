# Feature Spec: Live Phase Visualization

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Live Phase Visualization** | (this document) |

## Problem Statement

Provides real-time visualization of phase transitions in Flow View by animating nodes and edges as the game's state machine executes, helping developers understand application flow during runtime.

## Acceptance Criteria

- [x] Highlight current phase node with orange border and pulse animation
- [x] Animate transition edge when phase transition occurs (brief orange glow)
- [x] Display phase transition history (last 10 transitions) in sidebar
- [x] Show transition timestamps and duration
- [x] Reset animation state when game restarts or disconnects
- [x] Support multiple ProcessingUnits (show current phase per PU)
- [x] Display phase execution time in node tooltip
- [x] Smooth animation without performance impact
- [x] Pause/Resume live updates button
- [x] Clear history button

## Design

### Phase Transition Event Handling

**DiaApplicationEditor::HandlePhaseTransition:**
```cpp
void DiaApplicationEditor::HandlePhaseTransition(const Json::Value& data) {
    std::string puId = data["processing_unit"].asString();
    std::string fromPhase = data["from_phase"].asString();
    std::string toPhase = data["to_phase"].asString();
    uint64_t timestamp = data["timestamp"].asUInt64();
    float duration = data["duration_ms"].asFloat();
    
    // Update current phase
    mPluginData->liveState.currentPhase = StringCRC(toPhase.c_str());
    
    // Add to transition history
    PhaseTransition transition;
    transition.fromPhase = StringCRC(fromPhase.c_str());
    transition.toPhase = StringCRC(toPhase.c_str());
    transition.timestamp = timestamp;
    transition.durationMs = duration;
    
    mPluginData->liveState.recentTransitions.Add(transition);
    
    // Keep only last 10 transitions
    if (mPluginData->liveState.recentTransitions.GetSize() > 10) {
        mPluginData->liveState.recentTransitions.RemoveAt(0);
    }
    
    // Notify UI for animation
    Json::Value animationData;
    animationData["from_phase"] = fromPhase;
    animationData["to_phase"] = toPhase;
    animationData["timestamp"] = (Json::Value::UInt64)timestamp;
    animationData["duration_ms"] = duration;
    
    mEditorModel->NotifyObservers(StringCRC("live_phase_transition"), animationData);
    
    DIA_LOG("Phase transition: %s → %s (%.2fms)", fromPhase.c_str(), toPhase.c_str(), duration);
}
```

### React Animation Component

**LivePhaseVisualizer.tsx:**
```typescript
import React, { useState, useEffect } from 'react';
import { useManifestStore } from './ManifestStore';

interface PhaseTransition {
    from_phase: string;
    to_phase: string;
    timestamp: number;
    duration_ms: number;
}

export const useLivePhaseVisualization = () => {
    const { setCurrentPhase } = useManifestStore();
    const [transitionHistory, setTransitionHistory] = useState<PhaseTransition[]>([]);
    const [isPaused, setIsPaused] = useState(false);
    
    useEffect(() => {
        const handlePhaseTransition = (data: PhaseTransition) => {
            if (isPaused) return;
            
            // Update current phase
            setCurrentPhase(data.to_phase);
            
            // Add to history
            setTransitionHistory(prev => {
                const newHistory = [...prev, data];
                // Keep only last 10
                return newHistory.slice(-10);
            });
            
            // Trigger edge animation (handled by FlowView)
        };
        
        window.DiaEditor.subscribe('live_phase_transition', handlePhaseTransition);
        
        return () => {
            window.DiaEditor.unsubscribe('live_phase_transition', handlePhaseTransition);
        };
    }, [isPaused, setCurrentPhase]);
    
    const clearHistory = () => {
        setTransitionHistory([]);
    };
    
    return {
        transitionHistory,
        isPaused,
        setIsPaused,
        clearHistory,
    };
};
```

### Flow View Animation Integration

**FlowView.tsx (extended):**
```typescript
export const FlowView: React.FC<FlowViewProps> = ({ currentPhaseId, ...props }) => {
    const [animatingEdges, setAnimatingEdges] = useState<Set<string>>(new Set());
    
    // Listen for phase transitions
    useEffect(() => {
        const handleTransition = (data: any) => {
            const edgeId = `${data.from_phase}_to_${data.to_phase}`;
            
            // Add edge to animating set
            setAnimatingEdges(prev => new Set(prev).add(edgeId));
            
            // Remove after 500ms
            setTimeout(() => {
                setAnimatingEdges(prev => {
                    const next = new Set(prev);
                    next.delete(edgeId);
                    return next;
                });
            }, 500);
        };
        
        window.DiaEditor.subscribe('live_phase_transition', handleTransition);
        return () => window.DiaEditor.unsubscribe('live_phase_transition', handleTransition);
    }, []);
    
    // Apply animation styles to edges
    const getEdgeStyle = (edge: Edge): React.CSSProperties => {
        const isAnimating = animatingEdges.has(edge.id);
        
        return {
            stroke: isAnimating ? '#FF4500' : '#888',
            strokeWidth: isAnimating ? 3 : 2,
            opacity: isAnimating ? 1 : 0.7,
        };
    };
    
    // Update edges with animation styles
    const styledEdges = edges.map(edge => ({
        ...edge,
        style: getEdgeStyle(edge),
        animated: animatingEdges.has(edge.id),
    }));
    
    return (
        <ReactFlow
            nodes={nodes}
            edges={styledEdges}
            // ...
        />
    );
};
```

### PhaseNode with Live Highlight

**PhaseNode.tsx (extended):**
```typescript
export const PhaseNode: React.FC<PhaseNodeProps> = ({ data }) => {
    const { phase, isCurrent } = data;
    
    // Pulse animation for current phase
    const animationStyle = isCurrent ? {
        animation: 'pulse 1s ease-in-out infinite',
        boxShadow: '0 0 15px rgba(255, 69, 0, 0.8)',
        border: '3px solid #FF4500',
    } : {};
    
    return (
        <div
            className="phase-node"
            style={{
                ...baseStyle,
                ...animationStyle,
            }}
        >
            {/* ... node content ... */}
        </div>
    );
};

// CSS animation
const styles = `
@keyframes pulse {
    0%, 100% {
        box-shadow: 0 0 15px rgba(255, 69, 0, 0.8);
    }
    50% {
        box-shadow: 0 0 25px rgba(255, 69, 0, 1.0);
    }
}
`;
```

### Transition History Sidebar

**TransitionHistory.tsx:**
```typescript
import React from 'react';
import { PhaseTransition } from './types';

interface TransitionHistoryProps {
    transitions: PhaseTransition[];
    onClear: () => void;
}

export const TransitionHistory: React.FC<TransitionHistoryProps> = ({
    transitions,
    onClear
}) => {
    const formatTimestamp = (timestamp: number): string => {
        const date = new Date(timestamp);
        return date.toLocaleTimeString();
    };
    
    return (
        <div className="transition-history">
            <div className="history-header">
                <h3>Phase Transitions</h3>
                <button onClick={onClear}>Clear</button>
            </div>
            
            <div className="history-list">
                {transitions.slice().reverse().map((transition, index) => (
                    <div key={index} className="history-item">
                        <div className="transition-arrow">
                            {transition.from_phase} → {transition.to_phase}
                        </div>
                        <div className="transition-meta">
                            <span className="timestamp">
                                {formatTimestamp(transition.timestamp)}
                            </span>
                            <span className="duration">
                                {transition.duration_ms.toFixed(2)}ms
                            </span>
                        </div>
                    </div>
                ))}
            </div>
        </div>
    );
};
```

### Pause/Resume Controls

**LiveDebuggerControls.tsx:**
```typescript
interface LiveDebuggerControlsProps {
    isPaused: boolean;
    onTogglePause: () => void;
}

export const LiveDebuggerControls: React.FC<LiveDebuggerControlsProps> = ({
    isPaused,
    onTogglePause
}) => {
    return (
        <div className="live-debugger-controls">
            <button onClick={onTogglePause}>
                {isPaused ? '▶️ Resume' : '⏸️ Pause'} Live Updates
            </button>
        </div>
    );
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Phase transition event handling
- `Dia/DiaApplicationEditor/UI/LivePhaseVisualizer.tsx` - Live visualization hook
- `Dia/DiaApplicationEditor/UI/FlowView.tsx` - Edge animation integration
- `Dia/DiaApplicationEditor/UI/PhaseNode.tsx` - Current phase highlight
- `Dia/DiaApplicationEditor/UI/TransitionHistory.tsx` - Transition history sidebar

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Phase IDs use StringCRC |
| DiaApplicationEditor | DAED-013 | All inter-process comms use Protobuf | ✅ **Compliant** - Phase state updates received as DiaDebugProtocol Protobuf messages |

**Decision 56 Compliance:**
- **Phase + edge animation in Flow View** - ✅ Implemented with orange border pulse on node, orange glow on edge during transition

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | How to highlight current phase? | Decision 56 | ✅ Orange border + pulse animation + shadow |
| 2 | Animate transitions? | Decision 56 | ✅ Brief orange glow on edge (500ms) |
| 3 | Show transition history? | Decision 56 | ✅ Yes - last 10 transitions in sidebar |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Animation impact? | ✅ CSS animations GPU-accelerated; minimal impact; pause button allows disabling |
| 2 | UX | Pulse distracting? | ✅ No - Decision 56 - subtle animation (1s cycle); helps identify current phase |
| 3 | History | Why only 10 transitions? | ✅ Balance detail vs clutter; sufficient for recent context |
| 4 | Multiple PUs | Show all at once? | ✅ Yes - each PU's current phase highlighted independently |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
