import React, { useState, useRef, useCallback, useMemo, useEffect } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { TrafficLightDot } from './TrafficLightDot';
import type { ModuleV2, ProcessingUnitV2, StageV2 } from './types';

const ROW_HEIGHT = 28;
const OVERSCAN = 3;
const MODULE_COL_WIDTH = 160;
const ALL_COL_WIDTH = 60;
const STAGE_COL_WIDTH = 80;

interface ModulePresenceGridProps {
    containerHeight?: number;
}

type GridRow =
    | { kind: 'pu-header'; pu: ProcessingUnitV2 }
    | { kind: 'module'; module: ModuleV2; puId: string };

export const ModulePresenceGrid: React.FC<ModulePresenceGridProps> = ({ containerHeight }) => {
    const { manifest } = useManifestStoreV2();
    const { connectionState, activeStage } = useLiveStoreV2();
    const isLive = connectionState === 'connected';
    const [scrollTop, setScrollTop] = useState(0);
    const scrollRef = useRef<HTMLDivElement>(null);
    const rootRef = useRef<HTMLDivElement>(null);
    const [measuredHeight, setMeasuredHeight] = useState(containerHeight ?? 400);

    useEffect(() => {
        if (containerHeight !== undefined) return;
        const el = rootRef.current;
        if (!el || typeof ResizeObserver === 'undefined') return;
        const ro = new ResizeObserver((entries) => {
            for (const entry of entries) {
                const h = entry.contentRect.height;
                if (h > 0) setMeasuredHeight(h);
            }
        });
        ro.observe(el);
        return () => ro.disconnect();
    }, [containerHeight]);

    const effectiveHeight = containerHeight ?? measuredHeight;

    const stages: StageV2[] = manifest?.stages ?? [];

    const rows: GridRow[] = useMemo(() => {
        if (!manifest) return [];
        const result: GridRow[] = [];
        for (const pu of manifest.processingUnits) {
            result.push({ kind: 'pu-header', pu });
            for (const mod of pu.modules) {
                result.push({ kind: 'module', module: mod, puId: pu.instanceId });
            }
        }
        return result;
    }, [manifest]);

    const totalHeight = rows.length * ROW_HEIGHT;
    const visibleCount = Math.ceil(effectiveHeight / ROW_HEIGHT);
    const firstVisible = Math.max(0, Math.floor(scrollTop / ROW_HEIGHT) - OVERSCAN);
    const lastVisible = Math.min(rows.length - 1, Math.floor(scrollTop / ROW_HEIGHT) + visibleCount + OVERSCAN);

    const handleScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
        setScrollTop(e.currentTarget.scrollTop);
    }, []);

    const totalWidth = MODULE_COL_WIDTH + ALL_COL_WIDTH + stages.length * STAGE_COL_WIDTH;

    const stageNames = useMemo(() => stages.map(s => s.name), [stages]);

    const renderRow = (row: GridRow, index: number) => {
        if (row.kind === 'pu-header') {
            return (
                <div
                    key={`pu-${row.pu.instanceId}`}
                    data-testid="pu-group-header"
                    data-pu-id={row.pu.instanceId}
                    style={{
                        position: 'absolute',
                        top: index * ROW_HEIGHT,
                        left: 0,
                        width: totalWidth,
                        height: ROW_HEIGHT,
                        background: '#3a3a3a',
                        color: '#aaa',
                        padding: '0 8px',
                        fontSize: 11,
                        fontStyle: 'italic',
                        display: 'flex',
                        alignItems: 'center',
                        boxSizing: 'border-box',
                    }}
                >
                    {row.pu.instanceId}
                </div>
            );
        }

        const mod = row.module;
        const isEven = index % 2 === 0;
        const isAllSentinel = mod.stages.some(s => s.toLowerCase() === 'all');
        const inAllStages = isAllSentinel || (stageNames.length > 0 && stageNames.every(s => mod.stages.includes(s)));
        const isInStage = (stageName: string) => isAllSentinel || mod.stages.includes(stageName);

        return (
            <div
                key={`mod-${mod.instanceId}-${row.puId}`}
                style={{
                    position: 'absolute',
                    top: index * ROW_HEIGHT,
                    left: 0,
                    width: totalWidth,
                    height: ROW_HEIGHT,
                    background: isEven ? '#252526' : '#1e1e1e',
                    display: 'flex',
                    alignItems: 'center',
                    boxSizing: 'border-box',
                }}
            >
                {/* Module name column */}
                <div style={{
                    width: MODULE_COL_WIDTH,
                    minWidth: MODULE_COL_WIDTH,
                    padding: '0 8px',
                    color: '#ccc',
                    fontSize: 12,
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                }}>
                    {mod.instanceId}
                </div>

                {/* All column */}
                <div style={{
                    width: ALL_COL_WIDTH,
                    minWidth: ALL_COL_WIDTH,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    padding: '0 8px',
                    height: ROW_HEIGHT,
                }}>
                    {inAllStages && (
                        <span
                            data-testid="all-badge"
                            style={{
                                background: '#2e7d4a',
                                color: '#3cb370',
                                fontSize: 10,
                                fontWeight: 600,
                                padding: '1px 6px',
                                borderRadius: 8,
                            }}
                        >
                            all
                        </span>
                    )}
                </div>

                {/* Stage columns */}
                {stageNames.map(stageName => {
                    const isActiveCol = isLive && stageName === activeStage;
                    return (
                        <div
                            key={stageName}
                            data-testid="presence-cell"
                            data-module={mod.instanceId}
                            data-stage={stageName}
                            data-active-col={isActiveCol ? 'true' : undefined}
                            style={{
                                width: STAGE_COL_WIDTH,
                                minWidth: STAGE_COL_WIDTH,
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                padding: '0 8px',
                                height: ROW_HEIGHT,
                                background: isActiveCol ? '#0e2d4a' : undefined,
                            }}
                        >
                            {isInStage(stageName) && (
                                <TrafficLightDot state="green" size={8} />
                            )}
                        </div>
                    );
                })}
            </div>
        );
    };

    const visibleRows: React.ReactNode[] = [];
    for (let i = firstVisible; i <= lastVisible && i < rows.length; i++) {
        visibleRows.push(renderRow(rows[i], i));
    }

    return (
        <div
            ref={rootRef}
            data-testid="presence-grid"
            style={{
                background: '#1e1e1e',
                width: '100%',
                height: containerHeight ?? '100%',
                display: 'flex',
                flexDirection: 'column',
                overflow: 'hidden',
            }}
        >
            {/* Fixed header */}
            <div style={{
                display: 'flex',
                background: '#2d2d2d',
                height: ROW_HEIGHT,
                minHeight: ROW_HEIGHT,
                alignItems: 'center',
                borderBottom: '1px solid #3a3a3a',
            }}>
                <div style={{
                    width: MODULE_COL_WIDTH,
                    minWidth: MODULE_COL_WIDTH,
                    padding: '0 8px',
                    color: '#aaa',
                    fontSize: 11,
                    fontWeight: 600,
                }}>
                    Module
                </div>
                <div
                    data-testid="stage-col-header"
                    data-colname="All"
                    style={{
                        width: ALL_COL_WIDTH,
                        minWidth: ALL_COL_WIDTH,
                        padding: '0 8px',
                        color: '#aaa',
                        fontSize: 11,
                        fontWeight: 600,
                        textAlign: 'center',
                    }}
                >
                    All
                </div>
                {stages.map(stage => {
                    const isActiveCol = isLive && stage.name === activeStage;
                    return (
                        <div
                            key={stage.name}
                            data-testid="stage-col-header"
                            data-colname={stage.name}
                            data-active={isLive && stage.name === activeStage ? 'true' : 'false'}
                            style={{
                                width: STAGE_COL_WIDTH,
                                minWidth: STAGE_COL_WIDTH,
                                padding: '0 8px',
                                color: '#aaa',
                                fontSize: 11,
                                fontWeight: 600,
                                textAlign: 'center',
                                background: isActiveCol ? '#0e2d4a' : 'transparent',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                gap: 4,
                            }}
                        >
                            {stage.name}
                            {isActiveCol && (
                                <div style={{
                                    width: 6,
                                    height: 6,
                                    borderRadius: '50%',
                                    background: '#3cb370',
                                    flexShrink: 0,
                                }} />
                            )}
                        </div>
                    );
                })}
            </div>

            {/* Scrollable body */}
            <div
                ref={scrollRef}
                onScroll={handleScroll}
                style={{
                    flex: 1,
                    overflowY: 'auto',
                    position: 'relative',
                }}
            >
                <div style={{ height: totalHeight, position: 'relative' }}>
                    {visibleRows}
                </div>
            </div>
        </div>
    );
};
