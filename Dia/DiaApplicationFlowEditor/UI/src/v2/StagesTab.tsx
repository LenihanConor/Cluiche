import React, { useEffect, useRef, useState } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { bridgeRequest } from './bridge';
import type { StageV2 } from './types';

const NODE_R       = 26;
const NODE_SPACING = 160;
const PADDING_X    = 80;
const PADDING_Y    = 80;
const SVG_HEIGHT   = 200;

interface NodeLayout { name: string; cx: number; cy: number; }

function buildLayout(stages: StageV2[]): NodeLayout[] {
    return stages.map((s, i) => ({
        name: s.name,
        cx: PADDING_X + i * NODE_SPACING,
        cy: PADDING_Y,
    }));
}

function svgWidth(nodeCount: number): number {
    if (nodeCount === 0) return 300;
    return PADDING_X * 2 + Math.max(0, nodeCount - 1) * NODE_SPACING;
}

interface StagesTabProps {
    navigatedStageId?: string | null;
}

export const StagesTab: React.FC<StagesTabProps> = ({ navigatedStageId }) => {
    const stages       = useManifestStoreV2(s => s.manifest?.stages ?? []);
    const initialStage = useManifestStoreV2(s => s.manifest?.initialStage ?? '');
    const isLive       = useLiveStoreV2(s => s.connectionState === 'connected');
    const activeStage  = useLiveStoreV2(s => s.activeStage);

    // Track which stage is currently highlighted by navigate_to_stage
    const [highlightedStage, setHighlightedStage] = useState<string | null>(null);
    const scrollContainerRef = useRef<HTMLDivElement>(null);

    const transitionTo = (stageName: string) =>
        bridgeRequest('app.transitionTo', { stage: stageName });

    // Scroll to and highlight the navigated stage when C++ pushes navigate_to_stage
    useEffect(() => {
        if (!navigatedStageId) return;

        const idx = stages.findIndex(s => s.name === navigatedStageId);
        if (idx === -1) return;

        // Scroll the container so the target node is centred in view
        const container = scrollContainerRef.current;
        if (container) {
            const nodeCx = PADDING_X + idx * NODE_SPACING;
            const targetScrollLeft = nodeCx - container.clientWidth / 2;
            container.scrollTo({ left: Math.max(0, targetScrollLeft), behavior: 'smooth' });
        }

        // Apply highlight: set the stage, then clear it after the CSS transition completes
        setHighlightedStage(navigatedStageId);
        const timer = setTimeout(() => setHighlightedStage(null), 1500);
        return () => clearTimeout(timer);
    }, [navigatedStageId, stages]);

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

    const layout = buildLayout(stages);
    const stageByName = new Map(stages.map(s => [s.name, s]));
    const layoutByName = new Map(layout.map(n => [n.name, n]));
    const width = svgWidth(stages.length);

    return (
        <div
            ref={scrollContainerRef}
            data-testid="stages-tab"
            style={{ overflowX: 'auto', padding: 8, height: '100%', boxSizing: 'border-box' }}
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
                .auto-edge-live {
                    animation: dash-flow 0.8s linear infinite;
                }
                .stage-pulse-ring {
                    animation: stage-pulse 1.6s ease-out infinite;
                }
                .stage-navigate-ring {
                    animation: stage-navigate-flash 1.5s ease-out forwards;
                }
            `}</style>

            <svg
                width={width}
                height={SVG_HEIGHT}
                style={{ display: 'block', minWidth: width }}
            >
                <defs>
                    <marker id="arrow-static" markerWidth="8" markerHeight="8"
                        refX="6" refY="3" orient="auto">
                        <path d="M0,0 L0,6 L8,3 z" fill="#888" />
                    </marker>
                    <marker id="arrow-live" markerWidth="8" markerHeight="8"
                        refX="6" refY="3" orient="auto">
                        <path d="M0,0 L0,6 L8,3 z" fill="#3cb370" />
                    </marker>
                </defs>

                {/* Edges */}
                {stages.map(stage => {
                    const srcLayout = layoutByName.get(stage.name);
                    if (!srcLayout) return null;
                    const isSourceActive = isLive && activeStage === stage.name && stage.autoAdvance;

                    return stage.transitions.map(targetName => {
                        const tgtLayout = layoutByName.get(targetName);
                        if (!tgtLayout) return null;

                        const x1 = srcLayout.cx + NODE_R;
                        const x2 = tgtLayout.cx - NODE_R;
                        const y1 = srcLayout.cy;
                        const y2 = tgtLayout.cy;

                        // For non-horizontal edges (back-edges) draw a curve slightly above
                        const isForward = tgtLayout.cx > srcLayout.cx;
                        const edgeKey = `${stage.name}->${targetName}`;

                        if (isForward) {
                            return (
                                <line
                                    key={edgeKey}
                                    data-testid="stage-edge"
                                    data-edge-from={stage.name}
                                    data-edge-to={targetName}
                                    data-live-edge={isSourceActive ? 'true' : 'false'}
                                    x1={x1} y1={y1} x2={x2} y2={y2}
                                    stroke={isSourceActive ? '#3cb370' : '#888'}
                                    strokeWidth={2}
                                    strokeDasharray={isSourceActive ? '8 4' : undefined}
                                    className={isSourceActive ? 'auto-edge-live' : 'auto-edge'}
                                    markerEnd={`url(#arrow-${isSourceActive ? 'live' : 'static'})`}
                                />
                            );
                        } else {
                            // Back-edge: arc above the nodes
                            const mx = (srcLayout.cx + tgtLayout.cx) / 2;
                            const my = srcLayout.cy - 50;
                            return (
                                <path
                                    key={edgeKey}
                                    data-testid="stage-edge"
                                    data-edge-from={stage.name}
                                    data-edge-to={targetName}
                                    data-live-edge={isSourceActive ? 'true' : 'false'}
                                    d={`M ${srcLayout.cx} ${srcLayout.cy - NODE_R} Q ${mx} ${my} ${tgtLayout.cx} ${tgtLayout.cy - NODE_R}`}
                                    fill="none"
                                    stroke={isSourceActive ? '#3cb370' : '#888'}
                                    strokeWidth={2}
                                    strokeDasharray={isSourceActive ? '8 4' : undefined}
                                    className={isSourceActive ? 'auto-edge-live' : 'auto-edge'}
                                    markerEnd={`url(#arrow-${isSourceActive ? 'live' : 'static'})`}
                                />
                            );
                        }
                    });
                })}

                {/* Nodes */}
                {layout.map(({ name, cx, cy }) => {
                    const stage = stageByName.get(name);
                    if (!stage) return null;
                    const isInitial    = name === initialStage;
                    const isAutoAdv    = stage.autoAdvance;
                    const isActive     = isLive && activeStage === name;
                    const isNavigated  = name === highlightedStage;

                    const ringStroke  = isInitial ? '#3cb370' : isAutoAdv ? '#f0a030' : '#555';
                    const ringWidth   = isInitial ? 3 : 2;
                    const ringDash    = !isInitial && !isAutoAdv ? '3 3' : undefined;
                    const nodeFill    = isActive ? '#1e3a26' : '#2d2d2d';

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
                            onClick={isLive && !isActive ? () => transitionTo(name) : undefined}
                            style={{ cursor: isLive && !isActive ? 'pointer' : 'default' }}
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
                            <text
                                x={cx} y={cy + 4}
                                textAnchor="middle"
                                fontSize={12}
                                fill="#eee"
                            >
                                {name}
                            </text>
                            <text
                                x={cx} y={cy + NODE_R + 16}
                                textAnchor="middle"
                                fontSize={10}
                                fill={subLabelColor}
                            >
                                {subLabel}
                            </text>
                        </g>
                    );
                })}
            </svg>
        </div>
    );
};
