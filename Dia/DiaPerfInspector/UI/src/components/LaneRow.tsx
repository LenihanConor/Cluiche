import { useEffect, useRef, useState } from 'react';
import type { Lane, TimeDomain, TripRange } from '../model/laneModel';
import { bucketize, tripSpans } from '../model/laneModel';
import { findFrameAt } from '../model/laneModel';
import { selectionRect, timeAtX } from '../model/selection';
import { drawLane } from '../model/drawLane';
import type { MinimalCtx } from '../model/drawLane';

export const kLaneHeight = 74;

interface Props {
    lane: Lane;
    domain: TimeDomain;
    trips: TripRange[];
    /** Absolute entry index selected on this lane, or null. */
    selectedIndex?: number | null;
    onSelect?: (unit: string, index: number) => void;
}

export function LaneRow({ lane, domain, trips, selectedIndex = null, onSelect }: Props) {
    const holderRef = useRef<HTMLDivElement>(null);
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [width, setWidth] = useState(0);

    useEffect(() => {
        const el = holderRef.current;
        if (!el) return;
        setWidth(Math.floor(el.clientWidth));
        if (typeof ResizeObserver === 'undefined') return;
        const ro = new ResizeObserver((entries) => {
            const w = entries[0]?.contentRect.width;
            if (typeof w === 'number') setWidth(Math.floor(w));
        });
        ro.observe(el);
        return () => ro.disconnect();
    }, []);

    useEffect(() => {
        const canvas = canvasRef.current;
        const ctx = canvas?.getContext('2d');
        if (!canvas || !ctx || width < 1) return;
        const dpr = window.devicePixelRatio || 1;
        canvas.width = Math.floor(width * dpr);
        canvas.height = Math.floor(kLaneHeight * dpr);
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        const buckets = bucketize(lane, width, domain, trips);
        drawLane(ctx as unknown as MinimalCtx, lane, buckets, {
            width, height: kLaneHeight, domain, spans: tripSpans(lane, trips, domain),
        });
    }, [lane, domain, trips, width]);

    const onClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
        if (!onSelect) return;
        const rect = e.currentTarget.getBoundingClientRect();
        const w = rect.width > 0 ? rect.width : width;
        const index = findFrameAt(lane, timeAtX(e.clientX - rect.left, w, domain));
        if (index !== null) onSelect(lane.unit, index);
    };

    const outline = selectedIndex === null ? null : selectionRect(lane, selectedIndex, domain, width);

    return (
        <div
            data-testid="lane-row"
            data-unit={lane.unit}
            style={{ display: 'grid', gridTemplateColumns: '84px 1fr', alignItems: 'center', marginBottom: 2 }}
        >
            <div style={{ color: '#9cdcfe', fontWeight: 600 }}>
                {lane.unit}
                <small style={{ display: 'block', color: '#808080', fontWeight: 400 }}>
                    {lane.hz} Hz
                </small>
                <small style={{ display: 'block', color: '#808080', fontWeight: 400 }}>
                    budget {lane.budgetMs.toFixed(1)} ms
                </small>
            </div>
            <div ref={holderRef} style={{ minWidth: 0, position: 'relative' }}>
                <canvas
                    ref={canvasRef}
                    data-testid="lane-canvas"
                    onClick={onClick}
                    style={{ display: 'block', width: '100%', height: kLaneHeight, background: '#1a1a1a', cursor: 'crosshair' }}
                />
                {outline && (
                    <div
                        data-testid="lane-selection"
                        style={{
                            position: 'absolute', top: 0, height: kLaneHeight, left: outline.left, width: outline.width,
                            boxSizing: 'border-box', border: '1px solid #ffffff', pointerEvents: 'none',
                        }}
                    />
                )}
            </div>
        </div>
    );
}
