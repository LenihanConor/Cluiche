import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { LiveModuleState } from './useLiveStoreV2';
import { bridgeRequest } from './bridge';
import type { ProcessingUnitV2, StreamV2 } from './types';

export type TLNodeState = 'grey' | 'amber' | 'green' | 'red';

interface GraphViewProps {
    onStreamLabelClick?: (streamId: string) => void;
    onPUSelect?: (puId: string | null) => void;
}

interface Position {
    x: number;
    y: number;
}

interface DragState {
    puId: string;
    startMouse: Position;
    startPos: Position;
}

const NODE_WIDTH = 180;
const NODE_HEIGHT = 80;
const COL_SPACING = 220;
const ROW_SPACING = 160;
const OFFSET_X = 40;
const OFFSET_Y = 40;

const TL_COLORS: Record<TLNodeState, string> = {
    grey: '#666',
    amber: '#f0a030',
    green: '#3cb370',
    red: '#c0392b',
};

const STREAM_COLORS: Record<string, string> = {
    EventStream: '#4a9eff',
    FrameStream: '#e8a838',
    ServiceStream: '#c8a0e0',
};

function getStreamColor(kind: string): string {
    return STREAM_COLORS[kind] ?? '#4a9eff';
}

function getArrowMarkerId(kind: string): string {
    if (kind === 'FrameStream') return 'arrowhead-frame';
    if (kind === 'ServiceStream') return 'arrowhead-service';
    return 'arrowhead';
}

function getPULiveState(puId: string, connectionState: string, liveModules: LiveModuleState[]): TLNodeState {
    if (connectionState === 'disconnected') return 'grey';
    if (connectionState === 'connecting') return 'amber';
    const puModules = liveModules.filter(m => m.puId === puId);
    if (puModules.length === 0) return 'grey';
    return puModules.some(m => m.isActive) ? 'green' : 'grey';
}

function computeGridPositions(pus: ProcessingUnitV2[]): Map<string, Position> {
    const positions = new Map<string, Position>();
    const count = pus.length;
    const cols = count <= 6 ? 1 : Math.ceil(Math.sqrt(count));

    pus.forEach((pu, index) => {
        const col = index % cols;
        const row = Math.floor(index / cols);
        positions.set(pu.instanceId, {
            x: col * COL_SPACING + OFFSET_X,
            y: row * ROW_SPACING + OFFSET_Y,
        });
    });

    return positions;
}

function getNodeCenter(pos: Position): Position {
    return {
        x: pos.x + NODE_WIDTH / 2,
        y: pos.y + NODE_HEIGHT / 2,
    };
}

// Returns start (exit from source rect edge) and end (entry to target rect edge)
// so paths terminate at node boundaries and arrowheads are always visible.
function getEdgeAnchors(fromPos: Position, toPos: Position): { start: Position; end: Position } {
    const from = getNodeCenter(fromPos);
    const to = getNodeCenter(toPos);

    const dx = to.x - from.x;
    const dy = to.y - from.y;
    const len = Math.sqrt(dx * dx + dy * dy) || 1;
    const nx = dx / len;
    const ny = dy / len;

    const hw = NODE_WIDTH / 2;
    const hh = NODE_HEIGHT / 2;
    // Scale to reach the rect boundary in direction (nx, ny)
    const sx = nx !== 0 ? hw / Math.abs(nx) : Infinity;
    const sy = ny !== 0 ? hh / Math.abs(ny) : Infinity;
    const scale = Math.min(sx, sy);

    return {
        start: { x: from.x + nx * scale, y: from.y + ny * scale },
        end:   { x: to.x   - nx * scale, y: to.y   - ny * scale },
    };
}

export const GraphView: React.FC<GraphViewProps> = ({ onStreamLabelClick, onPUSelect }) => {
    const { manifest } = useManifestStoreV2();
    const { connectionState, modules: liveModules } = useLiveStoreV2();
    const [positions, setPositions] = useState<Map<string, Position>>(new Map());
    const [selected, setSelected] = useState<string | null>(null);
    const [dragState, setDragState] = useState<DragState | null>(null);
    const svgRef = useRef<SVGSVGElement>(null);

    useEffect(() => {
        if (!manifest?.processingUnits) return;

        setPositions((prev) => {
            const newPositions = new Map(prev);
            let hasNew = false;
            const gridPositions = computeGridPositions(manifest.processingUnits);

            for (const pu of manifest.processingUnits) {
                if (!newPositions.has(pu.instanceId)) {
                    const gridPos = gridPositions.get(pu.instanceId);
                    if (gridPos) {
                        newPositions.set(pu.instanceId, gridPos);
                        hasNew = true;
                    }
                }
            }

            return hasNew ? newPositions : prev;
        });
    }, [manifest?.processingUnits]);

    const handleMouseDown = useCallback((e: React.MouseEvent, puId: string) => {
        e.preventDefault();
        e.stopPropagation();
        const pos = positions.get(puId);
        if (!pos) return;
        setDragState({ puId, startMouse: { x: e.clientX, y: e.clientY }, startPos: { x: pos.x, y: pos.y } });
    }, [positions]);

    const handleMouseMove = useCallback((e: React.MouseEvent) => {
        if (!dragState) return;
        const dx = e.clientX - dragState.startMouse.x;
        const dy = e.clientY - dragState.startMouse.y;
        setPositions((prev) => {
            const next = new Map(prev);
            next.set(dragState.puId, { x: dragState.startPos.x + dx, y: dragState.startPos.y + dy });
            return next;
        });
    }, [dragState]);

    const handleMouseUp = useCallback(() => {
        if (dragState) {
            const pos = positions.get(dragState.puId);
            if (pos && pos.x === dragState.startPos.x && pos.y === dragState.startPos.y) {
                setSelected(dragState.puId);
                onPUSelect?.(dragState.puId);
            }
            setDragState(null);
        }
    }, [dragState, positions, onPUSelect]);

    const handleNodeClick = useCallback((puId: string) => {
        setSelected(puId);
        onPUSelect?.(puId);
    }, [onPUSelect]);

    const handleGhostClick = useCallback(() => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddPU',
            instanceId: 'NewPU',
            frequencyHz: 60,
            dedicatedThread: false,
        });
    }, []);

    // Lane layout for regular (non-service) streams sharing the same undirected PU pair
    const edgeLayout = React.useMemo(() => {
        const groups = new Map<string, StreamV2[]>();
        for (const s of manifest?.streams ?? []) {
            if (s.kind === 'ServiceStream') continue;
            const key = [s.fromPU, s.toPU].sort().join('::');
            const arr = groups.get(key) ?? [];
            arr.push(s);
            groups.set(key, arr);
        }
        const layout = new Map<string, { laneIndex: number; laneCount: number }>();
        for (const arr of groups.values()) {
            arr.forEach((s, i) => layout.set(s.id, { laneIndex: i, laneCount: arr.length }));
        }
        return layout;
    }, [manifest?.streams]);

    // Lane layout for ServiceStreams grouped by provider PU (fan them out vertically)
    const serviceEdgeLanes = React.useMemo(() => {
        const groups = new Map<string, StreamV2[]>();
        for (const s of manifest?.streams ?? []) {
            if (s.kind !== 'ServiceStream') continue;
            const arr = groups.get(s.fromPU) ?? [];
            arr.push(s);
            groups.set(s.fromPU, arr);
        }
        const layout = new Map<string, { laneIndex: number; laneCount: number }>();
        for (const arr of groups.values()) {
            arr.forEach((s, i) => layout.set(s.id, { laneIndex: i, laneCount: arr.length }));
        }
        return layout;
    }, [manifest?.streams]);

    const renderStreamEdge = (stream: StreamV2) => {
        const fromPos = positions.get(stream.fromPU);
        const isService = stream.kind === 'ServiceStream';
        const color = getStreamColor(stream.kind as string);

        if (isService) {
            if (!fromPos) return null;
            const lane = serviceEdgeLanes.get(stream.id) ?? { laneIndex: 0, laneCount: 1 };
            const laneOffset = lane.laneIndex - (lane.laneCount - 1) / 2;
            const SERVICE_LANE_GAP = 22;

            // Stub exits from the right edge, fanned vertically
            const startX = fromPos.x + NODE_WIDTH;
            const startY = fromPos.y + NODE_HEIGHT / 2 + laneOffset * SERVICE_LANE_GAP;
            const endX = startX + 64;

            return (
                <g key={stream.id} data-testid="stream-edge" data-stream-id={stream.id}>
                    <line
                        x1={startX}
                        y1={startY}
                        x2={endX}
                        y2={startY}
                        stroke={color}
                        strokeWidth={1.5}
                        strokeDasharray="4 3"
                        markerEnd="url(#arrowhead-service)"
                    />
                    <text
                        x={endX + 4}
                        y={startY + 4}
                        fill={color}
                        fontSize={9}
                        textAnchor="start"
                        style={{ cursor: onStreamLabelClick ? 'pointer' : 'default', paintOrder: 'stroke' }}
                        stroke="#1e1e1e"
                        strokeWidth={2}
                        onClick={(e) => { e.stopPropagation(); onStreamLabelClick?.(stream.id); }}
                    >
                        {stream.id}
                    </text>
                </g>
            );
        }

        const toPos = positions.get(stream.toPU);
        if (!fromPos || !toPos) return null;

        const { start, end } = getEdgeAnchors(fromPos, toPos);
        const lane = edgeLayout.get(stream.id) ?? { laneIndex: 0, laneCount: 1 };
        const laneOffset = lane.laneIndex - (lane.laneCount - 1) / 2;
        const FAN_SPACING = 36;
        const LABEL_GAP = 14;

        const midX = (start.x + end.x) / 2 + laneOffset * FAN_SPACING;
        const midY = (start.y + end.y) / 2;
        const d = `M ${start.x} ${start.y} C ${midX} ${start.y}, ${midX} ${end.y}, ${end.x} ${end.y}`;
        const labelY = midY - 6 + laneOffset * LABEL_GAP;
        const arrowId = getArrowMarkerId(stream.kind as string);

        return (
            <g key={stream.id} data-testid="stream-edge" data-stream-id={stream.id}>
                <path
                    d={d}
                    stroke={color}
                    strokeWidth={1.5}
                    fill="none"
                    markerEnd={`url(#${arrowId})`}
                />
                <text
                    x={midX}
                    y={labelY}
                    fill={color}
                    fontSize={10}
                    textAnchor="middle"
                    style={{ cursor: onStreamLabelClick ? 'pointer' : 'default', paintOrder: 'stroke' }}
                    stroke="#1e1e1e"
                    strokeWidth={3}
                    onClick={(e) => { e.stopPropagation(); onStreamLabelClick?.(stream.id); }}
                >
                    {stream.id}
                </text>
            </g>
        );
    };

    const renderPUNode = (pu: ProcessingUnitV2, liveState: TLNodeState = 'grey') => {
        const pos = positions.get(pu.instanceId);
        if (!pos) return null;
        const isSelected = selected === pu.instanceId;

        return (
            <g
                key={pu.instanceId}
                data-testid="pu-node"
                data-pu-id={pu.instanceId}
                data-selected={isSelected ? 'true' : undefined}
                onMouseDown={(e) => handleMouseDown(e, pu.instanceId)}
                onClick={() => handleNodeClick(pu.instanceId)}
                style={{ cursor: 'grab' }}
            >
                <rect
                    x={pos.x}
                    y={pos.y}
                    width={NODE_WIDTH}
                    height={NODE_HEIGHT}
                    fill="#2d2d2d"
                    rx={6}
                    stroke={isSelected ? '#4a9eff' : '#555'}
                    strokeWidth={isSelected ? 2 : 1}
                />
                <text x={pos.x + 10} y={pos.y + 20} fill="white" fontSize={12} fontWeight="bold">
                    {pu.instanceId}
                </text>
                <text x={pos.x + 10} y={pos.y + 38} fill="#999" fontSize={10}>
                    {pu.frequencyHz} Hz &middot; {pu.dedicatedThread ? 'Dedicated' : 'Shared'}
                </text>
                <text x={pos.x + 10} y={pos.y + 54} fill="#999" fontSize={10}>
                    {pu.modules.length} module{pu.modules.length !== 1 ? 's' : ''}
                </text>
                <circle cx={pos.x + NODE_WIDTH - 12} cy={pos.y + 12} r={4} fill={TL_COLORS[liveState]} />
            </g>
        );
    };

    const renderGhostNode = () => {
        const pus = manifest?.processingUnits ?? [];
        let ghostPos: Position;

        if (pus.length === 0) {
            ghostPos = { x: OFFSET_X, y: OFFSET_Y };
        } else {
            const cols = pus.length <= 6 ? 1 : Math.ceil(Math.sqrt(pus.length));
            const nextIndex = pus.length;
            ghostPos = {
                x: (nextIndex % cols) * COL_SPACING + OFFSET_X,
                y: Math.floor(nextIndex / cols) * ROW_SPACING + OFFSET_Y,
            };
        }

        return (
            <g data-testid="ghost-node" onClick={handleGhostClick} style={{ cursor: 'pointer' }}>
                <rect
                    x={ghostPos.x} y={ghostPos.y}
                    width={NODE_WIDTH} height={NODE_HEIGHT}
                    fill="none" rx={6}
                    stroke="#555" strokeWidth={1} strokeDasharray="6 3"
                />
                <text
                    x={ghostPos.x + NODE_WIDTH / 2}
                    y={ghostPos.y + NODE_HEIGHT / 2 + 4}
                    fill="#777" fontSize={12} textAnchor="middle"
                >
                    + Add PU
                </text>
            </g>
        );
    };

    return (
        <svg
            ref={svgRef}
            width="100%"
            height="100%"
            data-testid="graph-view"
            style={{ background: '#1e1e1e' }}
            onMouseMove={handleMouseMove}
            onMouseUp={handleMouseUp}
        >
            <defs>
                <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
                    <polygon points="0 0, 10 3.5, 0 7" fill="#4a9eff" />
                </marker>
                <marker id="arrowhead-frame" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
                    <polygon points="0 0, 10 3.5, 0 7" fill="#e8a838" />
                </marker>
                <marker id="arrowhead-service" markerWidth="8" markerHeight="6" refX="7" refY="3" orient="auto">
                    <polygon points="0 0, 8 3, 0 6" fill="#c8a0e0" />
                </marker>
            </defs>

            {manifest?.streams.map((stream) => renderStreamEdge(stream))}
            {manifest?.processingUnits.map((pu) => renderPUNode(pu, getPULiveState(pu.instanceId, connectionState, liveModules)))}
            {renderGhostNode()}
        </svg>
    );
};
