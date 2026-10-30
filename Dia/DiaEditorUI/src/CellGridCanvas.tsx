import React, { useEffect, useMemo, useRef, useState } from 'react';
import { theme } from './theme';
import { cellFromPoint, computeLayout } from './cellGeometry';
import type { CellRef, Topology } from './cellGeometry';
import { colourCells } from './cellColour';
import type { CellColouring } from './cellColour';
import { cornersFor, drawFill, drawOutlines, strokeCell } from './cellDraw';
import type { DrawContext } from './cellDraw';

export interface CellGridCanvasProps {
  topology: Topology;
  /** Grid size in cells. */
  width: number;
  height: number;
  /** Length width*height, row-major (index = r*width + q). */
  values: Float32Array | Uint16Array;
  colouring: CellColouring;
  highlight?: ReadonlySet<number>;
  onHover?: (cell: CellRef | null) => void;
  onCellClick?: (cell: CellRef) => void;
  /** Canvas size in CSS pixels. When omitted the component fills its container. */
  pixelWidth?: number;
  pixelHeight?: number;
}

const kDefaultSize = { w: 640, h: 480 };

function useBoxSize(ref: React.RefObject<HTMLDivElement>, fixedW?: number, fixedH?: number) {
  const [measured, setMeasured] = useState(kDefaultSize);
  useEffect(() => {
    const el = ref.current;
    if (!el || (fixedW !== undefined && fixedH !== undefined) || typeof ResizeObserver === 'undefined') return;
    const ro = new ResizeObserver(entries => {
      const r = entries[0]?.contentRect;
      if (r && r.width > 0 && r.height > 0) setMeasured({ w: r.width, h: r.height });
    });
    ro.observe(el);
    return () => ro.disconnect();
  }, [ref, fixedW, fixedH]);
  return { w: fixedW ?? measured.w, h: fixedH ?? measured.h };
}

function context(canvas: HTMLCanvasElement | null, dpr: number): DrawContext | null {
  const ctx = canvas?.getContext('2d') as (CanvasRenderingContext2D & DrawContext) | null | undefined;
  if (!ctx) return null;
  ctx.setTransform?.(dpr, 0, 0, dpr, 0, 0);
  return ctx;
}

export const CellGridCanvas: React.FC<CellGridCanvasProps> = ({
  topology, width, height, values, colouring, highlight, onHover, onCellClick, pixelWidth, pixelHeight,
}) => {
  const rootRef = useRef<HTMLDivElement>(null);
  const baseRef = useRef<HTMLCanvasElement>(null);
  const overlayRef = useRef<HTMLCanvasElement>(null);
  const size = useBoxSize(rootRef, pixelWidth, pixelHeight);
  const [hovered, setHovered] = useState<number | null>(null);
  const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;

  const layout = useMemo(
    () => computeLayout({ topology, width, height, boxW: size.w, boxH: size.h }),
    [topology, width, height, size.w, size.h],
  );

  // Base layer: fill + highlight. Redraws only when its inputs change (hover is a separate layer).
  useEffect(() => {
    const ctx = context(baseRef.current, dpr);
    if (!ctx) return;
    ctx.clearRect(0, 0, size.w, size.h);
    drawFill(ctx, layout, colourCells(values, colouring));
    if (highlight && highlight.size > 0) drawOutlines(ctx, layout, highlight, theme.warning, 2);
  }, [layout, values, colouring, highlight, size.w, size.h, dpr]);

  // Overlay layer: hovered cell only.
  useEffect(() => {
    const ctx = context(overlayRef.current, dpr);
    if (!ctx) return;
    ctx.clearRect(0, 0, size.w, size.h);
    if (hovered === null) return;
    ctx.strokeStyle = theme.text;
    ctx.lineWidth = 1.5;
    strokeCell(ctx, layout, hovered, cornersFor(layout));
  }, [layout, hovered, size.w, size.h, dpr]);

  const cellAt = (e: React.MouseEvent): CellRef | null => {
    const rect = rootRef.current?.getBoundingClientRect();
    return cellFromPoint(layout, e.clientX - (rect?.left ?? 0), e.clientY - (rect?.top ?? 0));
  };

  const handleMove = (e: React.MouseEvent) => {
    const cell = cellAt(e);
    const index = cell ? cell.index : null;
    if (index === hovered) return;
    setHovered(index);
    onHover?.(cell);
  };

  const handleLeave = () => {
    if (hovered === null) return;
    setHovered(null);
    onHover?.(null);
  };

  const handleClick = (e: React.MouseEvent) => {
    const cell = cellAt(e);
    if (cell) onCellClick?.(cell);
  };

  const canvasStyle: React.CSSProperties = { position: 'absolute', left: 0, top: 0, width: size.w, height: size.h };
  return (
    <div
      ref={rootRef}
      data-testid="cell-grid-canvas"
      data-topology={topology}
      style={{ position: 'relative', width: pixelWidth ?? '100%', height: pixelHeight ?? '100%', background: theme.bg, overflow: 'hidden' }}
      onMouseMove={handleMove}
      onMouseLeave={handleLeave}
      onClick={handleClick}
    >
      <canvas ref={baseRef} data-testid="cell-grid-canvas-base" width={size.w * dpr} height={size.h * dpr} style={canvasStyle} />
      <canvas
        ref={overlayRef}
        data-testid="cell-grid-canvas-overlay"
        width={size.w * dpr}
        height={size.h * dpr}
        style={{ ...canvasStyle, pointerEvents: 'none' }}
      />
    </div>
  );
};
