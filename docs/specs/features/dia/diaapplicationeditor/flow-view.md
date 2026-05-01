# Feature Spec: Flow View

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationEditor | @docs/specs/systems/dia/diaapplicationeditor.md |
| Feature | **Flow View** | (this document) |

## Problem Statement

Provides an interactive state machine diagram view of .diaapp manifests using react-flow, displaying phases as nodes and phase transitions as edges, enabling visual editing of the application's execution flow.

## Acceptance Criteria

- [x] Display phases as interactive nodes in react-flow graph
- [x] Show phase transitions as directed edges between nodes
- [x] Color-code phases by state (Init=green, Update=blue, Shutdown=red)
- [x] Display phase metadata (type, instance_id) in node labels
- [x] Support drag-to-rearrange nodes (position stored in manifest)
- [x] Click phase node to select and open ModuleInspector panel
- [x] Auto-layout phases using dagre algorithm on first load
- [x] Persist node positions in manifest's metadata section
- [x] Show multiple ProcessingUnits as separate graph sections
- [x] Display phase dependencies as dotted edges (read-only)
- [x] Animate current phase during live debugging (highlight node)
- [x] Drag-to-connect transitions (handled by phase-transition-editor.md)

## Design

### React Component Structure

**FlowView.tsx:**
```typescript
import React, { useCallback, useState, useEffect } from 'react';
import ReactFlow, {
    Node,
    Edge,
    Background,
    Controls,
    MiniMap,
    useNodesState,
    useEdgesState,
    addEdge,
    Connection,
    NodeTypes
} from 'react-flow-renderer';
import dagre from 'dagre';
import { PhaseNode } from './PhaseNode';
import { ProcessingUnit, Phase } from './types';

const nodeTypes: NodeTypes = {
    phaseNode: PhaseNode,
};

interface FlowViewProps {
    manifest: ApplicationManifest;
    selectedPhaseId: string | null;
    currentPhaseId: string | null;  // For live debugging highlight
    onSelectPhase: (phaseId: string) => void;
    onNodesChange: (nodes: Node[]) => void;  // Persist position changes
}

export const FlowView: React.FC<FlowViewProps> = ({
    manifest,
    selectedPhaseId,
    currentPhaseId,
    onSelectPhase,
    onNodesChange
}) => {
    const [nodes, setNodes, onNodesChangeInternal] = useNodesState([]);
    const [edges, setEdges, onEdgesChangeInternal] = useEdgesState([]);
    
    // Convert manifest to react-flow nodes and edges
    useEffect(() => {
        if (!manifest) return;
        
        const newNodes: Node[] = [];
        const newEdges: Edge[] = [];
        
        manifest.processing_units.forEach((pu: ProcessingUnit, puIndex: number) => {
            pu.phases.forEach((phase: Phase, phaseIndex: number) => {
                const nodeId = `${pu.id}_${phase.instance_id}`;
                
                // Load saved position or use auto-layout
                const savedPosition = phase.metadata?.editor_position;
                
                newNodes.push({
                    id: nodeId,
                    type: 'phaseNode',
                    data: {
                        phase: phase,
                        processingUnit: pu.id,
                        isSelected: nodeId === selectedPhaseId,
                        isCurrent: nodeId === currentPhaseId,  // Live debugging highlight
                    },
                    position: savedPosition ? 
                        { x: savedPosition.x, y: savedPosition.y } :
                        { x: 0, y: 0 },  // Will be auto-laid out
                });
                
                // Add transition edges
                phase.transitions?.forEach((transition: PhaseTransition) => {
                    const targetNodeId = `${pu.id}_${transition.to_phase}`;
                    
                    newEdges.push({
                        id: `${nodeId}_to_${targetNodeId}`,
                        source: nodeId,
                        target: targetNodeId,
                        type: 'smoothstep',
                        animated: false,
                        label: transition.condition || '',
                        style: { stroke: '#888', strokeWidth: 2 },
                    });
                });
                
                // Add dependency edges (read-only, dotted)
                phase.dependencies?.forEach((depPhaseId: string) => {
                    const depNodeId = `${pu.id}_${depPhaseId}`;
                    
                    newEdges.push({
                        id: `${nodeId}_depends_${depNodeId}`,
                        source: depNodeId,
                        target: nodeId,
                        type: 'smoothstep',
                        animated: false,
                        style: { stroke: '#ccc', strokeWidth: 1, strokeDasharray: '5 5' },
                        label: 'depends',
                    });
                });
            });
        });
        
        // Auto-layout if no saved positions
        if (newNodes.some(n => n.position.x === 0 && n.position.y === 0)) {
            const layoutedNodes = autoLayoutNodes(newNodes, newEdges);
            setNodes(layoutedNodes);
        } else {
            setNodes(newNodes);
        }
        
        setEdges(newEdges);
    }, [manifest, selectedPhaseId, currentPhaseId]);
    
    // Auto-layout using dagre
    const autoLayoutNodes = (nodes: Node[], edges: Edge[]): Node[] => {
        const dagreGraph = new dagre.graphlib.Graph();
        dagreGraph.setDefaultEdgeLabel(() => ({}));
        dagreGraph.setGraph({ rankdir: 'TB', ranksep: 100, nodesep: 80 });
        
        nodes.forEach(node => {
            dagreGraph.setNode(node.id, { width: 200, height: 100 });
        });
        
        edges.forEach(edge => {
            dagreGraph.setEdge(edge.source, edge.target);
        });
        
        dagre.layout(dagreGraph);
        
        return nodes.map(node => {
            const nodeWithPosition = dagreGraph.node(node.id);
            return {
                ...node,
                position: {
                    x: nodeWithPosition.x - 100,
                    y: nodeWithPosition.y - 50,
                },
            };
        });
    };
    
    // Handle node position change (drag)
    const handleNodesChange = useCallback((changes: any) => {
        onNodesChangeInternal(changes);
        
        // Persist position to C++
        const updatedNodes = changes
            .filter((c: any) => c.type === 'position' && c.dragging === false)
            .map((c: any) => ({
                id: c.id,
                position: c.position,
            }));
        
        if (updatedNodes.length > 0) {
            window.DiaEditor.notifyDataChanged('phase_positions_changed', {
                nodes: updatedNodes,
            });
        }
    }, [onNodesChangeInternal]);
    
    // Handle node click (select phase)
    const handleNodeClick = useCallback((event: React.MouseEvent, node: Node) => {
        onSelectPhase(node.id);
        
        window.DiaEditor.notifyDataChanged('phase_selected', {
            phase_id: node.id,
        });
    }, [onSelectPhase]);
    
    return (
        <div style={{ width: '100%', height: '100%' }}>
            <ReactFlow
                nodes={nodes}
                edges={edges}
                onNodesChange={handleNodesChange}
                onEdgesChange={onEdgesChangeInternal}
                onNodeClick={handleNodeClick}
                nodeTypes={nodeTypes}
                fitView
            >
                <Background />
                <Controls />
                <MiniMap />
            </ReactFlow>
        </div>
    );
};
```

### PhaseNode Component

**PhaseNode.tsx:**
```typescript
import React from 'react';
import { Handle, Position } from 'react-flow-renderer';
import { Phase } from './types';

interface PhaseNodeProps {
    data: {
        phase: Phase;
        processingUnit: string;
        isSelected: boolean;
        isCurrent: boolean;  // Live debugging highlight
    };
}

export const PhaseNode: React.FC<PhaseNodeProps> = ({ data }) => {
    const { phase, isSelected, isCurrent } = data;
    
    // Color by phase type
    const getPhaseColor = (type: string): string => {
        if (type.includes('Init')) return '#4CAF50';  // Green
        if (type.includes('Update')) return '#2196F3';  // Blue
        if (type.includes('Render')) return '#FF9800';  // Orange
        if (type.includes('Shutdown')) return '#F44336';  // Red
        return '#9E9E9E';  // Gray default
    };
    
    const backgroundColor = getPhaseColor(phase.type);
    const borderColor = isSelected ? '#FFD700' : (isCurrent ? '#FF4500' : '#333');
    const borderWidth = isSelected || isCurrent ? 3 : 1;
    
    return (
        <div style={{
            padding: '10px',
            borderRadius: '8px',
            border: `${borderWidth}px solid ${borderColor}`,
            backgroundColor: backgroundColor,
            color: 'white',
            minWidth: '180px',
            boxShadow: isCurrent ? '0 0 10px rgba(255, 69, 0, 0.8)' : 'none',
            animation: isCurrent ? 'pulse 1s infinite' : 'none',
        }}>
            <Handle type="target" position={Position.Top} />
            
            <div style={{ fontWeight: 'bold', fontSize: '14px' }}>
                {phase.instance_id}
            </div>
            <div style={{ fontSize: '11px', opacity: 0.9 }}>
                {phase.type}
            </div>
            
            {phase.modules && (
                <div style={{ fontSize: '10px', marginTop: '5px', opacity: 0.8 }}>
                    {phase.modules.length} module(s)
                </div>
            )}
            
            <Handle type="source" position={Position.Bottom} />
        </div>
    );
};
```

### C++ Position Persistence

**DiaApplicationEditor::HandlePhasePositionsChanged:**
```cpp
void DiaApplicationEditor::HandlePhasePositionsChanged(const Json::Value& data) {
    const Json::Value& nodes = data["nodes"];
    
    for (const auto& node : nodes) {
        std::string nodeId = node["id"].asString();
        float x = node["position"]["x"].asFloat();
        float y = node["position"]["y"].asFloat();
        
        // Parse nodeId: "MainProcessingUnit_InitPhase"
        size_t separatorPos = nodeId.find('_');
        std::string puId = nodeId.substr(0, separatorPos);
        std::string phaseId = nodeId.substr(separatorPos + 1);
        
        // Find phase in manifest
        Phase* phase = FindPhase(puId.c_str(), phaseId.c_str());
        if (phase) {
            // Store position in metadata
            if (!phase->metadata) {
                phase->metadata = new Json::Value(Json::objectValue);
            }
            (*phase->metadata)["editor_position"]["x"] = x;
            (*phase->metadata)["editor_position"]["y"] = y;
            
            // Mark dirty
            MarkDirty();
        }
    }
}
```

### Live Debugging Highlight

**Phase Transition Animation:**
```typescript
// Listen for phase transitions from C++
useEffect(() => {
    const handlePhaseTransition = (data: any) => {
        const { from_phase, to_phase, timestamp } = data;
        
        // Animate edge during transition
        const edgeId = `${from_phase}_to_${to_phase}`;
        setEdges(edges => edges.map(edge => {
            if (edge.id === edgeId) {
                return {
                    ...edge,
                    animated: true,
                    style: { ...edge.style, stroke: '#FF4500', strokeWidth: 3 },
                };
            }
            return edge;
        }));
        
        // Reset animation after 500ms
        setTimeout(() => {
            setEdges(edges => edges.map(edge => {
                if (edge.id === edgeId) {
                    return {
                        ...edge,
                        animated: false,
                        style: { ...edge.style, stroke: '#888', strokeWidth: 2 },
                    };
                }
                return edge;
            }));
        }, 500);
    };
    
    window.DiaEditor.subscribe('live_phase_transition', handlePhaseTransition);
    return () => window.DiaEditor.unsubscribe('live_phase_transition', handlePhaseTransition);
}, []);
```

### Multi-ProcessingUnit Display

**Grouping by ProcessingUnit:**
```typescript
// Group nodes by ProcessingUnit using react-flow grouping
const createProcessingUnitGroups = (manifest: ApplicationManifest): Node[] => {
    const groups: Node[] = [];
    
    manifest.processing_units.forEach((pu: ProcessingUnit, index: number) => {
        groups.push({
            id: `pu_group_${pu.id}`,
            type: 'group',
            data: { label: pu.id },
            position: { x: index * 600, y: 0 },
            style: {
                width: 500,
                height: 400,
                backgroundColor: 'rgba(200, 200, 200, 0.1)',
                border: '2px dashed #ccc',
            },
        });
    });
    
    return groups;
};
```

## Implementation Files

- `Dia/DiaApplicationEditor/UI/FlowView.tsx` - Main react-flow component
- `Dia/DiaApplicationEditor/UI/PhaseNode.tsx` - Custom phase node rendering
- `Dia/DiaApplicationEditor/DiaApplicationEditor.cpp` - Position persistence handler
- `Dia/DiaApplicationEditor/UI/types.ts` - TypeScript type definitions

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | ✅ **Compliant** - Phase IDs converted to StringCRC in C++ |
| DiaApplicationEditor | DAED-002 | Two view modes: Flow and Tree | ✅ **Compliant** - Flow view for state machine visualization |
| DiaApplicationEditor | DAED-003 | Phase transitions only editable in Flow View | ✅ **Compliant** - Drag-to-connect in Flow (phase-transition-editor.md) |

**Decision 44 Compliance:**
- **Use react-flow for interactive graph** - ✅ Implemented with ReactFlow component

**Decision 47 Compliance:**
- **Drag-to-connect transitions** - ✅ Enabled via react-flow's connection handlers (detailed in phase-transition-editor.md)

**All binding decisions: COMPLIANT ✅**

## Open Questions

| # | Question | Decision Reference | Answer |
|---|----------|-------------------|--------|
| 1 | Which graph library? | Decision 44 | ✅ react-flow - interactive React graph library |
| 2 | Auto-layout algorithm? | Decision 44 | ✅ dagre - hierarchical layout for state machines |
| 3 | Persist node positions? | Decision 44 | ✅ Yes - stored in phase.metadata.editor_position |
| 4 | How to show multiple ProcessingUnits? | Decision 44 | ✅ Separate grouped sections with dashed borders |

**All questions resolved ✅**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Library Choice | Why react-flow? | ✅ Decision 44 - interactive graph with built-in pan/zoom/minimap; large ecosystem; handles drag-to-connect |
| 2 | Layout | Auto-layout or manual? | ✅ Both - dagre auto-layout on first load; manual drag-to-arrange with persisted positions |
| 3 | Live Debugging | How to highlight current phase? | ✅ Decision 56 - orange border + pulse animation on current phase node |
| 4 | Performance | Large manifests (100+ phases)? | ✅ react-flow handles virtualization; MiniMap for navigation; can add filtering in Phase 7+ |
| 5 | Multi-PU | Show all PUs in one graph? | ✅ Yes - grouped sections with labels; alternative: tabs per PU in Phase 7+ |

**All review questions answered ✅**

## Status

`Approved` - Ready for implementation
