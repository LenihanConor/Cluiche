# Feature Spec: Phase Transition Editor

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Phase Transition Editor** | (this document) |

## Problem Statement

Enables visual editing of phase transitions in Flow View by dragging connections between phase nodes, providing an intuitive way to define application state machine flow.

## Acceptance Criteria

- [x] Drag from phase node's bottom handle to another phase's top handle
- [x] Create transition edge on connection drop
- [x] Delete transition by selecting edge and pressing Delete key
- [x] Edit transition condition by double-clicking edge label
- [x] Validate transitions (no self-loops, no duplicate transitions)
- [x] Show error for invalid transitions (red edge, error message)
- [x] Undo/redo support for transition add/remove (Phase 7+)
- [x] Mark manifest dirty when transition added or removed
- [x] Prevent adding transition if target phase doesn't exist
- [x] Context menu on edge: Edit Condition, Delete Transition

## Design

### React-Flow Connection Handling

**FlowView.tsx (extended):**
```typescript
import React, { useCallback } from 'react';
import ReactFlow, {
    Node,
    Edge,
    Connection,
    useReactFlow,
    addEdge,
} from 'react-flow-renderer';
import { useManifestStore } from './ManifestStore';

export const FlowView: React.FC<FlowViewProps> = ({ manifest, ...props }) => {
    const reactFlowInstance = useReactFlow();
    const { setDirty } = useManifestStore();
    
    // Handle new connection (drag-to-connect)
    const onConnect = useCallback((connection: Connection) => {
        const { source, target } = connection;
        
        // Validate connection
        if (source === target) {
            alert('Cannot create self-loop transition');
            return;
        }
        
        // Check for duplicate
        const existingEdge = edges.find(e => 
            e.source === source && e.target === target
        );
        if (existingEdge) {
            alert('Transition already exists');
            return;
        }
        
        // Create transition in C++
        window.DiaEditor.notifyDataChanged('transition_added', {
            from_phase: source,
            to_phase: target,
            condition: '',  // Empty initially, user can edit
        });
        
        // Add edge to UI
        const newEdge: Edge = {
            id: `${source}_to_${target}`,
            source,
            target,
            type: 'smoothstep',
            animated: false,
            label: '',
            style: { stroke: '#888', strokeWidth: 2 },
        };
        
        setEdges(edges => addEdge(newEdge, edges));
        setDirty(true);
    }, [edges, setEdges, setDirty]);
    
    // Handle edge click (select for editing)
    const onEdgeClick = useCallback((event: React.MouseEvent, edge: Edge) => {
        setSelectedEdgeId(edge.id);
    }, []);
    
    // Handle edge double-click (edit condition)
    const onEdgeDoubleClick = useCallback((event: React.MouseEvent, edge: Edge) => {
        const currentCondition = edge.label as string || '';
        const newCondition = prompt('Enter transition condition:', currentCondition);
        
        if (newCondition !== null) {
            window.DiaEditor.notifyDataChanged('transition_condition_changed', {
                edge_id: edge.id,
                condition: newCondition,
            });
            
            // Update edge label
            setEdges(edges => edges.map(e => 
                e.id === edge.id ? { ...e, label: newCondition } : e
            ));
            setDirty(true);
        }
    }, [setEdges, setDirty]);
    
    // Handle Delete key (remove selected edge)
    useEffect(() => {
        const handleKeyDown = (e: KeyboardEvent) => {
            if (e.key === 'Delete' && selectedEdgeId) {
                // Confirm deletion
                if (confirm('Delete this transition?')) {
                    window.DiaEditor.notifyDataChanged('transition_removed', {
                        edge_id: selectedEdgeId,
                    });
                    
                    setEdges(edges => edges.filter(e => e.id !== selectedEdgeId));
                    setSelectedEdgeId(null);
                    setDirty(true);
                }
            }
        };
        
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, [selectedEdgeId, setEdges, setDirty]);
    
    return (
        <ReactFlow
            nodes={nodes}
            edges={edges}
            onConnect={onConnect}
            onEdgeClick={onEdgeClick}
            onEdgeDoubleClick={onEdgeDoubleClick}
            connectionMode="loose"
            connectionLineType="smoothstep"
            nodeTypes={nodeTypes}
        >
            <Background />
            <Controls />
            <MiniMap />
        </ReactFlow>
    );
};
```

### Edge Context Menu

**EdgeContextMenu.tsx:**
```typescript
import React from 'react';
import { Menu, Item, useContextMenu } from 'react-contexify';

const EDGE_MENU_ID = 'edge-context-menu';

interface EdgeContextMenuProps {
    edgeId: string;
    currentCondition: string;
}

export const EdgeContextMenu: React.FC<EdgeContextMenuProps> = ({ 
    edgeId, 
    currentCondition 
}) => {
    const handleEditCondition = () => {
        const newCondition = prompt('Enter transition condition:', currentCondition);
        
        if (newCondition !== null) {
            window.DiaEditor.notifyDataChanged('transition_condition_changed', {
                edge_id: edgeId,
                condition: newCondition,
            });
        }
    };
    
    const handleDelete = () => {
        if (confirm('Delete this transition?')) {
            window.DiaEditor.notifyDataChanged('transition_removed', {
                edge_id: edgeId,
            });
        }
    };
    
    return (
        <Menu id={EDGE_MENU_ID}>
            <Item onClick={handleEditCondition}>Edit Condition</Item>
            <Item onClick={handleDelete}>Delete Transition</Item>
        </Menu>
    );
};
```

### C++ Transition Handling

**DiaApplicationEditor::HandleTransitionAdded:**
```cpp
void DiaApplicationEditor::HandleTransitionAdded(const Json::Value& data) {
    std::string fromPhase = data["from_phase"].asString();
    std::string toPhase = data["to_phase"].asString();
    std::string condition = data["condition"].asString();
    
    // Parse phase IDs: "MainProcessingUnit_InitPhase"
    size_t sepPos = fromPhase.find('_');
    std::string puId = fromPhase.substr(0, sepPos);
    std::string fromPhaseId = fromPhase.substr(sepPos + 1);
    std::string toPhaseId = toPhase.substr(sepPos + 1);
    
    // Find phase in manifest
    Phase* phase = FindPhase(puId.c_str(), fromPhaseId.c_str());
    if (!phase) {
        DIA_LOG_ERROR("Phase not found: %s", fromPhaseId.c_str());
        return;
    }
    
    // Add transition
    PhaseTransition transition;
    transition.toPhase = StringCRC(toPhaseId.c_str());
    transition.condition = condition.c_str();
    
    phase->transitions.Add(transition);
    
    // Mark dirty
    MarkDirty();
    
    DIA_LOG("Transition added: %s -> %s", fromPhaseId.c_str(), toPhaseId.c_str());
}
```

**DiaApplicationEditor::HandleTransitionRemoved:**
```cpp
void DiaApplicationEditor::HandleTransitionRemoved(const Json::Value& data) {
    std::string edgeId = data["edge_id"].asString();
    
    // Parse edge ID: "MainProcessingUnit_InitPhase_to_MainProcessingUnit_UpdatePhase"
    // Extract from_phase and to_phase
    
    // Find and remove transition from manifest
    // ... (implementation details)
    
    MarkDirty();
}
```

**DiaApplicationEditor::HandleTransitionConditionChanged:**
```cpp
void DiaApplicationEditor::HandleTransitionConditionChanged(const Json::Value& data) {
    std::string edgeId = data["edge_id"].asString();
    std::string newCondition = data["condition"].asString();
    
    // Find transition and update condition
    // ... (implementation details)
    
    MarkDirty();
}
```

### Validation

**Transition Validation:**
```typescript
const validateTransition = (source: string, target: string): string | null => {
    // No self-loops
    if (source === target) {
        return 'Cannot create self-loop transition';
    }
    
    // Check for duplicate
    const existingEdge = edges.find(e => 
        e.source === source && e.target === target
    );
    if (existingEdge) {
        return 'Transition already exists';
    }
    
    // Check if target phase exists
    const targetNode = nodes.find(n => n.id === target);
    if (!targetNode) {
        return 'Target phase does not exist';
    }
    
    return null;  // Valid
};
```

### Error Visualization

**Invalid Transition Display:**
```typescript
// Show invalid edge in red
const getEdgeStyle = (edge: Edge, validationErrors: any[]): React.CSSProperties => {
    const hasError = validationErrors.some(err => 
        err.path.includes(edge.id)
    );
    
    return {
        stroke: hasError ? '#f44336' : '#888',
        strokeWidth: hasError ? 3 : 2,
    };
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/FlowView.tsx` - Connection handlers
- `Dia/DiaApplicationEditor/UI/EdgeContextMenu.tsx` - Edge actions menu
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Transition add/remove/edit handlers

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Transition IDs use StringCRC in C++ |
| DiaApplicationEditor | DAED-003 | Phase transitions only editable in Flow View | ✅ **Compliant** - Transitions only editable here |

**Decision 47 Compliance:**
- **Drag-to-connect transitions in react-flow** - ✅ Implemented via onConnect handler

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | How to create transitions? | Decision 47 | ✅ Drag from phase node handle to another phase |
| 2 | How to edit condition? | Decision 47 | ✅ Double-click edge label or context menu |
| 3 | How to delete? | Decision 47 | ✅ Select edge + Delete key, or context menu |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | UX | Drag-to-connect intuitive? | ✅ Yes - Decision 47 - standard react-flow pattern; handles on nodes guide user |
| 2 | Validation | When to validate? | ✅ On connection attempt; also during real-time validation (Decisions 51-52) |
| 3 | Condition Editing | Inline or dialog? | ✅ Dialog (prompt) for Phase 5; inline editor in Phase 7+ |
| 4 | Deletion | Confirmation? | ✅ Yes - confirm before deleting to prevent accidental removal |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
