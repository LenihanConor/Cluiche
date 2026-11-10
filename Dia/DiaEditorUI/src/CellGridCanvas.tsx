import React, { useEffect, useMemo, useRef, useState } from 'react';
import { theme } from './theme';
import { cellFromPoint, cellsInBox, computeLayout } from './cellGeometry';
import type { CellRef, Topology, ViewState } from './cellGeometry';
import { colourCells } from './cellColour';
import type { CellColouring } from './cellColour';
import { cornersFor, drawFill, drawOutlines, drawTint, strokeCell } from './cellDraw';
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
  /** Cells drawn with a selection tint. */
  selection?: ReadonlySet<number>;
  /** Cells drawn with an outline (for example edited cells). */
  outline?: ReadonlySet<number>;
  /** Cells under a tool preview, drawn translucent. */
  footprint?: ReadonlySet<number>;
  /** Controlled zoom and pan. When unset the grid fits the box. */
  view?: ViewState;
  /** Wheel zoom (about the cursor) and middle-button / space+left-button drag pan. */
  onViewChange?: (view: ViewState) => void;
  /** Cells of a finished left-drag box (offset-coordinate rectangle); additive when shift was held. */
  onBox?: (cells: number[], additive: boolean) => void;
  /** Cells along the pointer path while a stroke is active; each cell is reported once per stroke. */
  onStroke?: (cells: number[], phase: 'start' | 'move' | 'end') => void;
  /** When true, left-drag reports strokes instead of a box. */
  strokeActive?: boolean;
  onHover?: (cell: CellRef | null) => void;
  onCellClick?: (cell: CellRef) => void;
  /** Canvas size in CSS pixels. When omitted the component fills its container. */
  pixelWidth?: number;
  pixelHeight?: number;
}

const kSelectionTint = 'rgba(14, 99, 156, 0.45)';
const kFootprintTint = 'rgba(255, 255, 255, 0.35)';
const kDragThresholdPx = 3;
const kMinZoom = 0.25;
const kMaxZoom = 32;
const kWheelZoomRate = 0.0015;
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
  topology, width, height, values, colouring, highlight, selection, outline, footprint, view, onViewChange, onBox, onStroke, strokeActive, onHover, onCellClick, pixelWidth, pixelHeight,
}) => {
  const rootRef = useRef<HTMLDivElement>(null);
  const baseRef = useRef<HTMLCanvasElement>(null);
  const overlayRef = useRef<HTMLCanvasElement>(null);
  const size = useBoxSize(rootRef, pixelWidth, pixelHeight);
  const [hovered, setHovered] = useState<number | null>(null);
  const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1;

  const layout = useMemo(
    () => computeLayout({ topology, width, height, boxW: size.w, boxH: size.h, view }),
    [topology, width, height, size.w, size.h, view?.zoom, view?.panX, view?.panY],
  );

  // Keep the latest view/callback readable from the native wheel listener without re-subscribing.
  const viewRef = useRef({ view, onViewChange, size });
  viewRef.current = { view, onViewChange, size };
  const panRef = useRef<{ x: number; y: number; moved: boolean } | null>(null);
  const spaceRef = useRef(false);
  const lastPanMoved = useRef(false);
  const dragRef = useRef<{
    mode: 'stroke' | 'box';
    startX: number; startY: number; lastX: number; lastY: number;
    anchor: number | null; last: number | null; moved: boolean; seen: Set<number>;
  } | null>(null);

  useEffect(() => {
    const el = rootRef.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      const { view: v, onViewChange: emit, size: sz } = viewRef.current;
      if (!emit) return;
      e.preventDefault();
      const cur = v ?? { zoom: 1, panX: 0, panY: 0 };
      const zoom = Math.min(kMaxZoom, Math.max(kMinZoom, cur.zoom * Math.exp(-e.deltaY * kWheelZoomRate)));
      if (zoom === cur.zoom) return;
      const rect = el.getBoundingClientRect();
      const px = e.clientX - rect.left - sz.w / 2;
      const py = e.clientY - rect.top - sz.h / 2;
      const k = zoom / cur.zoom;
      emit({ zoom, panX: px - (px - cur.panX) * k, panY: py - (py - cur.panY) * k });
    };
    const onKey = (e: KeyboardEvent) => { if (e.code === 'Space') spaceRef.current = e.type === 'keydown'; };
    el.addEventListener('wheel', onWheel, { passive: false });
    window.addEventListener('keydown', onKey);
    window.addEventListener('keyup', onKey);
    return () => {
      el.removeEventListener('wheel', onWheel);
      window.removeEventListener('keydown', onKey);
      window.removeEventListener('keyup', onKey);
    };
  }, []);

  // Base layer: fill + highlight. Redraws only when its inputs change (hover is a separate layer).
  useEffect(() => {
    const ctx = context(baseRef.current, dpr);
    if (!ctx) return;
    ctx.clearRect(0, 0, size.w, size.h);
    drawFill(ctx, layout, colourCells(values, colouring));
    if (outline && outline.size > 0) drawOutlines(ctx, layout, outline, theme.accentHover, 1.5);
    if (highlight && highlight.size > 0) drawOutlines(ctx, layout, highlight, theme.warning, 2);
    if (selection && selection.size > 0) drawTint(ctx, layout, selection, kSelectionTint);
  }, [layout, values, colouring, highlight, selection, outline, size.w, size.h, dpr]);

  // Overlay layer: tool footprint and hovered cell. Both change with the pointer, so they stay off the base layer.
  useEffect(() => {
    const ctx = context(overlayRef.current, dpr);
    if (!ctx) return;
    ctx.clearRect(0, 0, size.w, size.h);
    if (footprint && footprint.size > 0) drawTint(ctx, layout, footprint, kFootprintTint);
    if (hovered === null) return;
    ctx.strokeStyle = theme.text;
    ctx.lineWidth = 1.5;
    strokeCell(ctx, layout, hovered, cornersFor(layout));
  }, [layout, hovered, footprint, size.w, size.h, dpr]);

  const cellAt = (e: React.MouseEvent): CellRef | null => {
    const rect = rootRef.current?.getBoundingClientRect();
    return cellFromPoint(layout, e.clientX - (rect?.left ?? 0), e.clientY - (rect?.top ?? 0));
  };

  const handleMouseDown = (e: React.MouseEvent) => {
    lastPanMoved.current = false;
    startDrag(e);
    if (!onViewChange) return;
    if (e.button === 1 || (e.button === 0 && spaceRef.current)) {
      e.preventDefault();
      panRef.current = { x: e.clientX, y: e.clientY, moved: false };
    }
  };

  const startDrag = (e: React.MouseEvent) => {
    if (e.button !== 0 || spaceRef.current || (!onBox && !(strokeActive && onStroke))) return;
    const cell = cellAt(e);
    const mode = strokeActive && onStroke ? 'stroke' : 'box';
    if (mode === 'box' && !cell) return;
    dragRef.current = {
      mode, startX: e.clientX, startY: e.clientY, lastX: e.clientX, lastY: e.clientY,
      anchor: cell ? cell.index : null, last: cell ? cell.index : null, moved: false, seen: new Set(),
    };
    if (mode === 'stroke' && cell) {
      dragRef.current.seen.add(cell.index);
      onStroke?.([cell.index], 'start');
    }
  };

  // Cells crossed by the pointer path from the previous to the current position, not yet reported this stroke.
  const strokeCells = (d: NonNullable<typeof dragRef.current>, e: React.MouseEvent): number[] => {
    const dist = Math.hypot(e.clientX - d.lastX, e.clientY - d.lastY);
    const steps = Math.max(1, Math.ceil(dist / Math.max(1, layout.cellSize / 3)));
    const out: number[] = [];
    for (let k = 1; k <= steps; k++) {
      const t = k / steps;
      const cell = cellAt({ clientX: d.lastX + (e.clientX - d.lastX) * t, clientY: d.lastY + (e.clientY - d.lastY) * t } as React.MouseEvent);
      if (cell && !d.seen.has(cell.index)) { d.seen.add(cell.index); out.push(cell.index); }
    }
    return out;
  };

  const updateDrag = (e: React.MouseEvent): boolean => {
    const d = dragRef.current;
    if (!d) return false;
    if (!d.moved && Math.hypot(e.clientX - d.startX, e.clientY - d.startY) < kDragThresholdPx) return true;
    d.moved = true;
    if (d.mode === 'stroke') {
      const fresh = strokeCells(d, e);
      if (fresh.length > 0) onStroke?.(fresh, 'move');
    } else {
      const cell = cellAt(e);
      if (cell) d.last = cell.index;
    }
    d.lastX = e.clientX; d.lastY = e.clientY;
    return true;
  };

  const endDrag = (e: React.MouseEvent) => {
    const d = dragRef.current;
    dragRef.current = null;
    if (!d) return;
    if (d.mode === 'stroke') {
      onStroke?.([], 'end');
    } else if (d.moved && d.anchor !== null) {
      const cell = cellAt(e);
      const end = cell ? cell.index : d.last;
      if (end !== null) onBox?.(cellsInBox(d.anchor, end, width), e.shiftKey);
    }
    lastPanMoved.current = lastPanMoved.current || d.moved;
  };

  const endPan = () => {
    lastPanMoved.current = panRef.current?.moved ?? false;
    panRef.current = null;
  };

  const handleMove = (e: React.MouseEvent) => {
    const pan = panRef.current;
    if (pan && onViewChange) {
      const cur = view ?? { zoom: 1, panX: 0, panY: 0 };
      const dx = e.clientX - pan.x, dy = e.clientY - pan.y;
      pan.x = e.clientX; pan.y = e.clientY; pan.moved = pan.moved || dx !== 0 || dy !== 0;
      onViewChange({ ...cur, panX: cur.panX + dx, panY: cur.panY + dy });
      return;
    }
    updateDrag(e);
    const cell = cellAt(e);
    const index = cell ? cell.index : null;
    if (index === hovered) return;
    setHovered(index);
    onHover?.(cell);
  };

  const handleLeave = (e: React.MouseEvent) => {
    endPan();
    endDrag(e);
    if (hovered === null) return;
    setHovered(null);
    onHover?.(null);
  };

  const handleClick = (e: React.MouseEvent) => {
    if (lastPanMoved.current) { lastPanMoved.current = false; return; }
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
      onMouseDown={handleMouseDown}
      onMouseUp={e => { endPan(); endDrag(e); }}
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
