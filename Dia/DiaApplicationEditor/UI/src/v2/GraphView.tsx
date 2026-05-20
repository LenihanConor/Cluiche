import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { bridgeRequest } from './bridge';
import type { ProcessingUnitV2, StreamV2 } from './types';

export type TLNodeState = 'grey' | 'amber' | 'green' | 'red';

interface GraphViewProps {
    onStreamLabelClick?: (streamId: string) => void;
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

export const GraphView: React.FC<GraphViewProps> = ({ onStreamLabelClick }) => {
    const { manifest } = useManifestStoreV2();
    const [positions, setPositions] = useState<Map<string, Position>>(new Map());
    const [selected, setSelected] = useState<string | null>(null);
    const [dragState, setDragState] = useState<DragState | null>(null);
    const svgRef = useRef<SVGSVGElement>(null);

    // Initialize positions for new PUs
    useEffect(() => {
        if (!manifest?.processingUnits) return;

        setPositions((prev) => {
            const newPositions = new Map(prev);
            let hasNew = false;

            // Compute grid positions for all PUs
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

        setDragState({
            puId,
            startMouse: { x: e.clientX, y: e.clientY },
            startPos: { x: pos.x, y: pos.y },
        });
    }, [positions]);

    const handleMouseMove = useCallback((e: React.MouseEvent) => {
        if (!dragState) return;

        const dx = e.clientX - dragState.startMouse.x;
        const dy = e.clientY - dragState.startMouse.y;

        setPositions((prev) => {
            const next = new Map(prev);
            next.set(dragState.puId, {
                x: dragState.startPos.x + dx,
                y: dragState.startPos.y + dy,
            });
            return next;
        });
    }, [dragState]);

    const handleMouseUp = useCallback(() => {
        if (dragState) {
            // Check if it was a click (no movement) => select
            const pos = positions.get(dragState.puId);
            if (pos && pos.x === dragState.startPos.x && pos.y === dragState.startPos.y) {
                setSelected(dragState.puId);
            }
            setDragState(null);
        }
    }, [dragState, positions]);

    const handleNodeClick = useCallback((puId: string) => {
        setSelected(puId);
    }, []);

    const handleGhostClick = useCallback(() => {
        bridgeRequest('manifest.applyCommand', {
            commandType: 'AddPU',
            instanceId: 'NewPU',
            frequencyHz: 60,
            dedicatedThread: false,
        });
    }, []);

    const renderStreamEdge = (stream: StreamV2) => {
        const fromPos = positions.get(stream.fromPU);
        const toPos = positions.get(stream.toPU);
        if (!fromPos || !toPos) return null;

        const from = getNodeCenter(fromPos);
        const to = getNodeCenter(toPos);

        const midX = (from.x + to.x) / 2;
        const midY = (from.y + to.y) / 2;

        // Control points for cubic bezier
        const cx1 = midX;
        const cy1 = from.y;
        const cx2 = midX;
        const cy2 = to.y;

        const d = `M ${from.x} ${from.y} C ${cx1} ${cy1}, ${cx2} ${cy2}, ${to.x} ${to.y}`;

        return (
            <g key={stream.id} data-testid="stream-edge" data-stream-id={stream.id}>
                <path
                    d={d}
                    stroke="#4a9eff"
                    strokeWidth={1.5}
                    fill="none"
                    markerEnd="url(#arrowhead)"
                />
                <text
                    x={midX}
                    y={midY - 6}
                    fill="#4a9eff"
                    fontSize={10}
                    textAnchor="middle"
                    style={{ cursor: onStreamLabelClick ? 'pointer' : 'default' }}
                    onClick={(e) => {
                        e.stopPropagation();
                        onStreamLabelClick?.(stream.id);
                    }}
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
                {/* PU instanceId */}
                <text
                    x={pos.x + 10}
                    y={pos.y + 20}
                    fill="white"
                    fontSize={12}
                    fontWeight="bold"
                >
                    {pu.instanceId}
                </text>
                {/* Frequency and thread info */}
                <text
                    x={pos.x + 10}
                    y={pos.y + 38}
                    fill="#999"
                    fontSize={10}
                >
                    {pu.frequencyHz} Hz &middot; {pu.dedicatedThread ? 'Dedicated' : 'Shared'}
                </text>
                {/* Module count */}
                <text
                    x={pos.x + 10}
                    y={pos.y + 54}
                    fill="#999"
                    fontSize={10}
                >
                    {pu.modules.length} module{pu.modules.length !== 1 ? 's' : ''}
                </text>
                {/* Traffic light circle at top-right corner */}
                <circle
                    cx={pos.x + NODE_WIDTH - 12}
                    cy={pos.y + 12}
                    r={4}
                    fill={TL_COLORS[liveState]}
                />
            </g>
        );
    };

    const renderGhostNode = () => {
        const pus = manifest?.processingUnits ?? [];
        // Place ghost after last PU position
        let ghostPos: Position;

        if (pus.length === 0) {
            ghostPos = { x: OFFSET_X, y: OFFSET_Y };
        } else {
            const cols = pus.length <= 6 ? 1 : Math.ceil(Math.sqrt(pus.length));
            const nextIndex = pus.length;
            const col = nextIndex % cols;
            const row = Math.floor(nextIndex / cols);
            ghostPos = {
                x: col * COL_SPACING + OFFSET_X,
                y: row * ROW_SPACING + OFFSET_Y,
            };
        }

        return (
            <g
                data-testid="ghost-node"
                onClick={handleGhostClick}
                style={{ cursor: 'pointer' }}
            >
                <rect
                    x={ghostPos.x}
                    y={ghostPos.y}
                    width={NODE_WIDTH}
                    height={NODE_HEIGHT}
                    fill="none"
                    rx={6}
                    stroke="#555"
                    strokeWidth={1}
                    strokeDasharray="6 3"
                />
                <text
                    x={ghostPos.x + NODE_WIDTH / 2}
                    y={ghostPos.y + NODE_HEIGHT / 2 + 4}
                    fill="#777"
                    fontSize={12}
                    textAnchor="middle"
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
                <marker
                    id="arrowhead"
                    markerWidth="10"
                    markerHeight="7"
                    refX="9"
                    refY="3.5"
                    orient="auto"
                >
                    <polygon points="0 0, 10 3.5, 0 7" fill="#4a9eff" />
                </marker>
            </defs>

            {/* Stream edges */}
            {manifest?.streams.map((stream) => renderStreamEdge(stream))}

            {/* PU nodes */}
            {manifest?.processingUnits.map((pu) => renderPUNode(pu))}

            {/* Ghost node */}
            {renderGhostNode()}
        </svg>
    );
};
