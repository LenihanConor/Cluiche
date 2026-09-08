import React, { useEffect, useRef, useState, useCallback } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { bridgeRequest } from './bridge';
import type { StageV2 } from './types';

const NODE_R       = 26;
const PADDING_X    = 80;
const PADDING_Y    = 80;
const NODE_SPACING = 160;

interface NodePos { cx: number; cy: number; }

interface DragState {
    name: string;
    startMouse: { x: number; y: number };
    startPos: NodePos;
}

// Compute tangent exit/entry points on the circle boundary so arrows always
// terminate at the node edge regardless of angle after dragging.
function circleEdgeAnchors(src: NodePos, tgt: NodePos) {
    const dx = tgt.cx - src.cx;
    const dy = tgt.cy - src.cy;
    const len = Math.sqrt(dx * dx + dy * dy) || 1;
    const nx = dx / len;
    const ny = dy / len;
    return {
        start: { x: src.cx + nx * NODE_R, y: src.cy + ny * NODE_R },
        end:   { x: tgt.cx - nx * NODE_R, y: tgt.cy - ny * NODE_R },
    };
}

interface StagesTabProps {
    navigatedStageId?: string | null;
}

export const StagesTab: React.FC<StagesTabProps> = ({ navigatedStageId }) => {
    const stages       = useManifestStoreV2(s => s.manifest?.stages ?? []);
    const initialStage = useManifestStoreV2(s => s.manifest?.initialStage ?? '');
    const isLive       = useLiveStoreV2(s => s.connectionState === 'connected');
    const activeStage  = useLiveStoreV2(s => s.activeStage);

    const [highlightedStage, setHighlightedStage] = useState<string | null>(null);
    const [positions, setPositions] = useState<Map<string, NodePos>>(new Map());
    const [dragState, setDragState] = useState<DragState | null>(null);
    const svgRef = useRef<SVGSVGElement>(null);

    // Assign initial positions for any new stages; preserve existing positions
    useEffect(() => {
        if (stages.length === 0) return;
        setPositions(prev => {
            const next = new Map(prev);
            let hasNew = false;
            stages.forEach((s, i) => {
                if (!next.has(s.name)) {
                    next.set(s.name, { cx: PADDING_X + i * NODE_SPACING, cy: PADDING_Y });
                    hasNew = true;
                }
            });
            return hasNew ? next : prev;
        });
    }, [stages]);

    const transitionTo = (stageName: string) =>
        bridgeRequest('app.transitionTo', { stage: stageName });

    // Highlight the navigated stage when C++ pushes navigate_to_stage
    useEffect(() => {
        if (!navigatedStageId) return;
        setHighlightedStage(navigatedStageId);
        const timer = setTimeout(() => setHighlightedStage(null), 1500);
        return () => clearTimeout(timer);
    }, [navigatedStageId]);

    const handleMouseDown = useCallback((e: React.MouseEvent, name: string) => {
        e.preventDefault();
        e.stopPropagation();
        const pos = positions.get(name);
        if (!pos) return;
        setDragState({ name, startMouse: { x: e.clientX, y: e.clientY }, startPos: { ...pos } });
    }, [positions]);

    const handleMouseMove = useCallback((e: React.MouseEvent) => {
        if (!dragState) return;
        const dx = e.clientX - dragState.startMouse.x;
        const dy = e.clientY - dragState.startMouse.y;
        setPositions(prev => {
            const next = new Map(prev);
            next.set(dragState.name, {
                cx: dragState.startPos.cx + dx,
                cy: dragState.startPos.cy + dy,
            });
            return next;
        });
    }, [dragState]);

    const handleMouseUp = useCallback(() => {
        if (!dragState) return;
        const pos = positions.get(dragState.name);
        const moved = pos && (pos.cx !== dragState.startPos.cx || pos.cy !== dragState.startPos.cy);
        if (!moved && isLive && activeStage !== dragState.name) {
            transitionTo(dragState.name);
        }
        setDragState(null);
    }, [dragState, positions, isLive, activeStage]);

    if (stages.length === 0) {
        return (
            <div
                data-testid="stages-empty"
                style={{
                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                    height: '100%', color: '#555', fontSize: 13,
                }}
            >
                No stages defined — add stages via the sidebar to begin.
            </div>
        );
    }

    return (
        <svg
            ref={svgRef}
            width="100%"
            height="100%"
            data-testid="stages-tab"
            style={{ display: 'block', background: '#1e1e1e', cursor: dragState ? 'grabbing' : 'default' }}
            onMouseMove={handleMouseMove}
            onMouseUp={handleMouseUp}
        >
            <style>{`
                @keyframes dash-flow {
                    from { stroke-dashoffset: 0; }
                    to   { stroke-dashoffset: -16; }
                }
                @keyframes stage-pulse {
                    0%   { r: ${NODE_R + 4}; opacity: 0.6; }
                    100% { r: ${NODE_R + 12}; opacity: 0; }
                }
                @keyframes stage-navigate-flash {
                    0%   { opacity: 1; }
                    100% { opacity: 0; }
                }
                .auto-edge-live { animation: dash-flow 0.8s linear infinite; }
                .stage-pulse-ring { animation: stage-pulse 1.6s ease-out infinite; }
                .stage-navigate-ring { animation: stage-navigate-flash 1.5s ease-out forwards; }
            `}</style>

            <defs>
                <marker id="arrow-static" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
                    <path d="M0,0 L0,6 L8,3 z" fill="#888" />
                </marker>
                <marker id="arrow-live" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
                    <path d="M0,0 L0,6 L8,3 z" fill="#3cb370" />
                </marker>
            </defs>

            {/* Edges — rendered before nodes so they sit behind */}
            {stages.map(stage => {
                const srcPos = positions.get(stage.name);
                if (!srcPos) return null;
                const isSourceActive = isLive && activeStage === stage.name && stage.autoAdvance;

                return stage.transitions.map(targetName => {
                    const tgtPos = positions.get(targetName);
                    if (!tgtPos) return null;

                    const edgeKey = `${stage.name}->${targetName}`;
                    const stroke  = isSourceActive ? '#3cb370' : '#888';
                    const dash    = isSourceActive ? '8 4' : undefined;
                    const cls     = isSourceActive ? 'auto-edge-live' : 'auto-edge';
                    const marker  = `url(#arrow-${isSourceActive ? 'live' : 'static'})`;
                    const { start, end } = circleEdgeAnchors(srcPos, tgtPos);
                    const isForward = tgtPos.cx > srcPos.cx;

                    if (isForward) {
                        return (
                            <line
                                key={edgeKey}
                                data-testid="stage-edge"
                                data-edge-from={stage.name}
                                data-edge-to={targetName}
                                data-live-edge={isSourceActive ? 'true' : 'false'}
                                x1={start.x} y1={start.y}
                                x2={end.x}   y2={end.y}
                                stroke={stroke}
                                strokeWidth={2}
                                strokeDasharray={dash}
                                className={cls}
                                markerEnd={marker}
                            />
                        );
                    } else {
                        // Back-edge: arc above both nodes
                        const mx = (srcPos.cx + tgtPos.cx) / 2;
                        const my = Math.min(srcPos.cy, tgtPos.cy) - 50;
                        return (
                            <path
                                key={edgeKey}
                                data-testid="stage-edge"
                                data-edge-from={stage.name}
                                data-edge-to={targetName}
                                data-live-edge={isSourceActive ? 'true' : 'false'}
                                d={`M ${start.x} ${start.y} Q ${mx} ${my} ${end.x} ${end.y}`}
                                fill="none"
                                stroke={stroke}
                                strokeWidth={2}
                                strokeDasharray={dash}
                                className={cls}
                                markerEnd={marker}
                            />
                        );
                    }
                });
            })}

            {/* Nodes */}
            {stages.map(stage => {
                const pos = positions.get(stage.name);
                if (!pos) return null;
                const { cx, cy } = pos;
                const name        = stage.name;
                const isInitial   = name === initialStage;
                const isAutoAdv   = stage.autoAdvance;
                const isActive    = isLive && activeStage === name;
                const isNavigated = name === highlightedStage;
                const isDragging  = dragState?.name === name;

                const ringStroke = isInitial ? '#3cb370' : isAutoAdv ? '#f0a030' : '#555';
                const ringWidth  = isInitial ? 3 : 2;
                const ringDash   = !isInitial && !isAutoAdv ? '3 3' : undefined;
                const nodeFill   = isActive ? '#1e3a26' : '#2d2d2d';

                const subLabel = isActive
                    ? '● active'
                    : isInitial
                        ? `initial · ${isAutoAdv ? 'auto' : 'manual'}`
                        : isAutoAdv ? 'auto' : 'manual';
                const subLabelColor = isActive ? '#3cb370' : isInitial ? '#3cb370' : isAutoAdv ? '#f0a030' : '#666';

                return (
                    <g
                        key={name}
                        data-testid="stage-node"
                        data-stage-name={name}
                        data-is-initial={isInitial}
                        data-is-auto={isAutoAdv}
                        data-active={isActive}
                        data-navigated={isNavigated}
                        onMouseDown={(e) => handleMouseDown(e, name)}
                        style={{ cursor: isDragging ? 'grabbing' : 'grab' }}
                    >
                        {isActive && (
                            <circle
                                cx={cx} cy={cy}
                                r={NODE_R + 4}
                                fill="none"
                                stroke="#3cb370"
                                strokeWidth={2}
                                opacity={0.6}
                                className="stage-pulse-ring"
                            />
                        )}
                        {isNavigated && (
                            <circle
                                cx={cx} cy={cy}
                                r={NODE_R + 8}
                                fill="none"
                                stroke="#5ab4f5"
                                strokeWidth={3}
                                className="stage-navigate-ring"
                            />
                        )}
                        <circle
                            cx={cx} cy={cy} r={NODE_R}
                            fill={nodeFill}
                            stroke={ringStroke}
                            strokeWidth={ringWidth}
                            strokeDasharray={ringDash}
                        />
                        <text x={cx} y={cy + 4} textAnchor="middle" fontSize={12} fill="#eee">
                            {name}
                        </text>
                        <text x={cx} y={cy + NODE_R + 16} textAnchor="middle" fontSize={10} fill={subLabelColor}>
                            {subLabel}
                        </text>
                    </g>
                );
            })}
        </svg>
    );
};
