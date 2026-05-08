import React, { useCallback, useEffect, useRef, useState } from 'react';
import ReactFlow, {
    Background,
    Controls,
    MiniMap,
    useNodesState,
    useEdgesState,
    addEdge,
    NodeTypes,
    NodeChange,
    Connection,
    EdgeChange,
    applyEdgeChanges,
} from 'reactflow';
import 'reactflow/dist/style.css';
import dagre from '@dagrejs/dagre';
import { PhaseNode, PHASE_COLORS } from './PhaseNode';
import type { PhaseNodeData } from './PhaseNode';
import type { Node, Edge } from 'reactflow';
import { useManifestStore } from './ManifestStore';
import { useUndoStore } from './UndoStore';
import type { ManifestData, ProcessingUnitData } from './types';

const nodeTypes: NodeTypes = { phaseNode: PhaseNode };

const NODE_W = 180;
const NODE_H = 80;

function buildGraph(
    manifest: ManifestData,
    selectedNode: string | null,
    currentPhaseId: string | null,
): { nodes: Node<PhaseNodeData>[]; edges: Edge[] } {
    const nodes: Node<PhaseNodeData>[] = [];
    const edges: Edge[] = [];

    manifest.processing_units.forEach((pu: ProcessingUnitData, puIdx: number) => {
        const isImported = !!pu._source;
        nodes.push({
            id: `__pu_${pu.instance_id}`,
            type: 'group',
            data: {} as unknown as PhaseNodeData,
            position: { x: puIdx * 700, y: 0 },
            style: {
                width: 600, height: 500,
                background: isImported ? 'rgba(100,100,255,0.03)' : 'rgba(255,255,255,0.03)',
                border: isImported ? '2px dashed #666' : '1px dashed #444',
                borderRadius: 8,
            },
            selectable: false,
            draggable: false,
        });

        pu.phases.forEach((phase, phaseIdx) => {
            const nodeId = `${pu.instance_id}_${phase.instance_id}`;
            const phaseModules = pu.modules.filter(m => m.phases?.includes(phase.instance_id));

            // Restore saved editor position if present
            const savedPos = (phase.config as { editor_position?: { x: number; y: number } } | undefined)?.editor_position;

            nodes.push({
                id: nodeId,
                type: 'phaseNode',
                parentNode: `__pu_${pu.instance_id}`,
                extent: 'parent',
                data: {
                    phase,
                    modules: phaseModules,
                    puId: pu.instance_id,
                    isSelected: nodeId === selectedNode,
                    isCurrent: nodeId === currentPhaseId,
                },
                position: savedPos
                    ? { x: savedPos.x, y: savedPos.y }
                    : { x: 50 + (phaseIdx % 3) * 200, y: 50 + Math.floor(phaseIdx / 3) * 150 },
            });
        });

        pu.transitions.forEach(t => {
            const src = `${pu.instance_id}_${t.from}`;
            const tgt = `${pu.instance_id}_${t.to}`;
            edges.push({
                id: `${src}__to__${tgt}`,
                source: src,
                target: tgt,
                type: 'smoothstep',
                style: { stroke: '#888', strokeWidth: 1.5 },
                markerEnd: { type: 'arrowclosed' as const },
            });
        });
    });

    return { nodes, edges };
}

function autoLayout(nodes: Node<PhaseNodeData>[], edges: Edge[]): Node<PhaseNodeData>[] {
    const g = new dagre.graphlib.Graph();
    g.setDefaultEdgeLabel(() => ({}));
    g.setGraph({ rankdir: 'TB', ranksep: 80, nodesep: 60 });

    nodes.filter(n => n.type === 'phaseNode').forEach(n => g.setNode(n.id, { width: NODE_W, height: NODE_H }));
    edges.forEach(e => { try { g.setEdge(e.source, e.target); } catch { /* skip disconnected */ } });
    dagre.layout(g);

    return nodes.map(n => {
        if (n.type !== 'phaseNode') return n;
        const pos = g.node(n.id);
        if (!pos) return n;
        return { ...n, position: { x: pos.x - NODE_W / 2, y: pos.y - NODE_H / 2 } };
    });
}

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

interface EdgeMenu { x: number; y: number; edge: Edge }

export const FlowView: React.FC = () => {
    const manifest        = useManifestStore(s => s.manifest);
    const selectedNode    = useManifestStore(s => s.selectedNode);
    const setSelectedNode = useManifestStore(s => s.setSelectedNode);
    const setDirty        = useManifestStore(s => s.setDirty);

    const [nodes, setNodes, onNodesChange] = useNodesState<PhaseNodeData>([]);
    const [edges, setEdges, onEdgesChange] = useEdgesState([]);
    const [edgeMenu, setEdgeMenu] = useState<EdgeMenu | null>(null);
    const [selectedEdgeId, setSelectedEdgeId] = useState<string | null>(null);
    const didLayout = useRef(false);

    useEffect(() => {
        if (!manifest) return;
        const { nodes: rawNodes, edges: rawEdges } = buildGraph(manifest as ManifestData, selectedNode, null);
        const laidOut = !didLayout.current ? autoLayout(rawNodes, rawEdges) : rawNodes;
        didLayout.current = true;
        setNodes(laidOut);
        setEdges(rawEdges);
    }, [manifest, selectedNode, setNodes, setEdges]);

    const handleNodesChange = useCallback((changes: NodeChange[]) => {
        onNodesChange(changes);
        const moved = changes.filter((c): c is NodeChange & { type: 'position'; id: string; position?: { x: number; y: number } } =>
            c.type === 'position' && !(c as { dragging?: boolean }).dragging && !!(c as { position?: unknown }).position
        );
        if (moved.length > 0) {
            sendToPlugin('phase_positions_changed', {
                nodes: moved.map(c => ({ id: c.id, position: c.position })),
            });
        }
    }, [onNodesChange]);

    const handleEdgesChange = useCallback((changes: EdgeChange[]) => {
        // Filter out 'remove' changes — we handle deletion explicitly with confirmation
        const nonRemove = changes.filter(c => c.type !== 'remove');
        onEdgesChange(nonRemove);
    }, [onEdgesChange]);

    const handleConnect = useCallback((connection: Connection) => {
        const { source, target } = connection;
        if (!source || !target) return;
        if (source === target) return; // no self-loops
        const edgeId = `${source}__to__${target}`;
        if (edges.some(e => e.id === edgeId)) return; // no duplicates

        useManifestStore.getState().pushUndo('Add transition');
        const newEdge: Edge = {
            id: edgeId,
            source,
            target,
            type: 'smoothstep',
            style: { stroke: '#888', strokeWidth: 1.5 },
            markerEnd: { type: 'arrowclosed' as const },
        };
        setEdges(eds => addEdge(newEdge, eds));
        sendToPlugin('transition_added', { from_phase: source, to_phase: target });
        setDirty(true);
    }, [edges, setEdges, setDirty]);

    const handleEdgeClick = useCallback((_: React.MouseEvent, edge: Edge) => {
        setSelectedEdgeId(edge.id);
        setEdgeMenu(null);
    }, []);

    const handleEdgeContextMenu = useCallback((e: React.MouseEvent, edge: Edge) => {
        e.preventDefault();
        setEdgeMenu({ x: e.clientX, y: e.clientY, edge });
    }, []);

    const handleEdgeDoubleClick = useCallback((_: React.MouseEvent, edge: Edge) => {
        const current = (edge.label as string) ?? '';
        const next = window.prompt('Transition condition (leave blank for none):', current);
        if (next === null) return;
        setEdges(eds => eds.map(e => e.id === edge.id ? { ...e, label: next || undefined } : e));
        sendToPlugin('transition_condition_changed', { from_phase: edge.source, to_phase: edge.target, condition: next });
        setDirty(true);
    }, [setEdges, setDirty]);

    const deleteEdge = useCallback((edgeId: string) => {
        const edge = edges.find(e => e.id === edgeId);
        if (!edge) return;
        if (!window.confirm('Delete this transition?')) return;
        useManifestStore.getState().pushUndo('Remove transition');
        setEdges(eds => eds.filter(e => e.id !== edgeId));
        sendToPlugin('transition_removed', { from_phase: edge.source, to_phase: edge.target });
        setSelectedEdgeId(null);
        setEdgeMenu(null);
        setDirty(true);
    }, [edges, setEdges, setDirty]);

    const handleNodeClick = useCallback((_: React.MouseEvent, node: Node<PhaseNodeData>) => {
        if (node.type !== 'phaseNode') return;
        setSelectedNode(node.id);
        sendToPlugin('node_selected', { node_id: node.id });
    }, [setSelectedNode]);

    // Delete key removes selected edge
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            if (e.key === 'Delete' && selectedEdgeId) deleteEdge(selectedEdgeId);
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, [selectedEdgeId, deleteEdge]);

    if (!manifest) return null;

    return (
        <div style={{ width: '100%', height: '100%', position: 'relative' }} onClick={edgeMenu ? () => setEdgeMenu(null) : undefined}>
            <ReactFlow
                nodes={nodes}
                edges={edges.map(e => ({ ...e, selected: e.id === selectedEdgeId }))}
                onNodesChange={handleNodesChange}
                onEdgesChange={handleEdgesChange}
                onConnect={handleConnect}
                onEdgeClick={handleEdgeClick}
                onEdgeDoubleClick={handleEdgeDoubleClick}
                onEdgeContextMenu={handleEdgeContextMenu}
                onNodeClick={handleNodeClick}
                nodeTypes={nodeTypes}
                fitView
                proOptions={{ hideAttribution: true }}
            >
                <Background color="#333" gap={16} />
                <Controls />
                <MiniMap nodeColor={n => n.data?.isSelected ? '#ffd600' : '#37474f'} style={{ background: '#1e1e1e' }} />
            </ReactFlow>

            <div style={{
                position: 'absolute', bottom: 8, left: 8,
                display: 'flex', gap: 12, padding: '6px 10px',
                background: 'rgba(30,30,30,0.9)', border: '1px solid #444',
                borderRadius: 4, zIndex: 10,
            }}>
                {PHASE_COLORS.map(({ label, color, description }) => (
                    <span key={label} title={description} style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11, color: '#ccc', cursor: 'default' }}>
                        <span style={{ width: 10, height: 10, background: color, display: 'inline-block', borderRadius: 2 }} />
                        {label}
                    </span>
                ))}
            </div>

            {edgeMenu && (
                <div
                    style={{
                        position: 'fixed', top: edgeMenu.y, left: edgeMenu.x,
                        background: '#252526', border: '1px solid #444', borderRadius: 3,
                        padding: '4px 0', zIndex: 1000, minWidth: 160,
                    }}
                    onClick={e => e.stopPropagation()}
                >
                    <button style={menuItemStyle} onClick={() => handleEdgeDoubleClick({} as React.MouseEvent, edgeMenu.edge)}>
                        Edit Condition
                    </button>
                    <button style={menuItemStyle} onClick={() => deleteEdge(edgeMenu.edge.id)}>
                        Delete Transition
                    </button>
                </div>
            )}
        </div>
    );
};

const menuItemStyle: React.CSSProperties = {
    display: 'block', width: '100%', background: 'none', border: 'none',
    color: '#ccc', padding: '6px 16px', textAlign: 'left', cursor: 'pointer', fontSize: 12,
};
