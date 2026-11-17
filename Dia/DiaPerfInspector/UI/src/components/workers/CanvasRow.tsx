import { useEffect, useRef, useState } from 'react';
import type { MinimalCtx } from '../../model/drawLane';

interface Props {
    height: number;
    /** Pure draw callback; must be stable (useCallback / useMemo) or the canvas redraws on every render. */
    draw: (ctx: MinimalCtx, width: number, height: number) => void;
    onClick?: (x: number, width: number) => void;
    testId: string;
    cursor?: string;
}

/** A full-width canvas that measures itself and redraws through a pure draw function (same approach as LaneRow). */
export function CanvasRow({ height, draw, onClick, testId, cursor = 'default' }: Props) {
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
        canvas.height = Math.floor(height * dpr);
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        draw(ctx as unknown as MinimalCtx, width, height);
    }, [draw, width, height]);

    return (
        <div ref={holderRef} style={{ minWidth: 0 }}>
            <canvas
                ref={canvasRef}
                data-testid={testId}
                onClick={onClick ? (e) => {
                    const rect = e.currentTarget.getBoundingClientRect();
                    onClick(e.clientX - rect.left, rect.width > 0 ? rect.width : width);
                } : undefined}
                style={{ display: 'block', width: '100%', height, background: '#1a1a1a', cursor }}
            />
        </div>
    );
}
