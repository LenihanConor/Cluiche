// Canvas 2D drawing for CellGridCanvas. Takes a context-like object so it can be exercised with a recording mock.

import { cellCentre, hexCorners } from './cellGeometry';
import type { Layout } from './cellGeometry';
import { rgba8ToHex } from './ramps';

export interface DrawContext {
  fillStyle: string | CanvasGradient | CanvasPattern;
  strokeStyle: string | CanvasGradient | CanvasPattern;
  lineWidth: number;
  beginPath(): void;
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
  closePath(): void;
  fill(): void;
  stroke(): void;
  fillRect(x: number, y: number, w: number, h: number): void;
  strokeRect(x: number, y: number, w: number, h: number): void;
  clearRect(x: number, y: number, w: number, h: number): void;
}

type Corners = { x: number; y: number }[];

// Appends one hex as a closed subpath (no beginPath), so many cells can share one fill or stroke call.
function addHexSubpath(ctx: DrawContext, layout: Layout, index: number, corners: Corners): void {
  const c = cellCentre(layout, index);
  ctx.moveTo(c.x + corners[0].x, c.y + corners[0].y);
  for (let k = 1; k < corners.length; k++) ctx.lineTo(c.x + corners[k].x, c.y + corners[k].y);
  ctx.closePath();
}

/** Fills one cell with the current fillStyle. */
export function fillCell(ctx: DrawContext, layout: Layout, index: number, corners: Corners): void {
  if (layout.topology === 'square') {
    const c = cellCentre(layout, index);
    const h = layout.cellSize / 2;
    ctx.fillRect(c.x - h, c.y - h, layout.cellSize, layout.cellSize);
    return;
  }
  ctx.beginPath();
  addHexSubpath(ctx, layout, index, corners);
  ctx.fill();
}

/** Strokes one cell outline with the current strokeStyle and lineWidth. */
export function strokeCell(ctx: DrawContext, layout: Layout, index: number, corners: Corners): void {
  if (layout.topology === 'square') {
    const c = cellCentre(layout, index);
    const h = layout.cellSize / 2;
    ctx.strokeRect(c.x - h, c.y - h, layout.cellSize, layout.cellSize);
    return;
  }
  ctx.beginPath();
  addHexSubpath(ctx, layout, index, corners);
  ctx.stroke();
}

export function cornersFor(layout: Layout): Corners {
  return hexCorners(layout.cellSize);
}

/** Fill layer: one cell per entry of `colours` (packed Rgba8). Hex cells are batched into one path per colour. */
export function drawFill(ctx: DrawContext, layout: Layout, colours: Uint32Array): void {
  const corners = cornersFor(layout);
  if (layout.topology === 'square') {
    let last = -1;
    for (let i = 0; i < colours.length; i++) {
      if (colours[i] !== last) {
        last = colours[i];
        ctx.fillStyle = rgba8ToHex(last);
      }
      fillCell(ctx, layout, i, corners);
    }
    return;
  }
  const byColour = new Map<number, number[]>();
  for (let i = 0; i < colours.length; i++) {
    const list = byColour.get(colours[i]);
    if (list) list.push(i);
    else byColour.set(colours[i], [i]);
  }
  for (const [colour, cells] of byColour) {
    ctx.fillStyle = rgba8ToHex(colour);
    ctx.beginPath();
    for (const i of cells) addHexSubpath(ctx, layout, i, corners);
    ctx.fill();
  }
}

/** Outline layer for a set of cells (hex: one stroke call for the whole set). */
export function drawOutlines(ctx: DrawContext, layout: Layout, cells: Iterable<number>, colour: string, lineWidth: number): void {
  const corners = cornersFor(layout);
  ctx.strokeStyle = colour;
  ctx.lineWidth = lineWidth;
  const max = layout.width * layout.height;
  if (layout.topology === 'square') {
    for (const i of cells) if (i >= 0 && i < max) strokeCell(ctx, layout, i, corners);
    return;
  }
  ctx.beginPath();
  for (const i of cells) if (i >= 0 && i < max) addHexSubpath(ctx, layout, i, corners);
  ctx.stroke();
}

/** Translucent fill over a set of cells (selection tint, tool footprint). `colour` should carry its own alpha. */
export function drawTint(ctx: DrawContext, layout: Layout, cells: Iterable<number>, colour: string): void {
  const corners = cornersFor(layout);
  ctx.fillStyle = colour;
  const max = layout.width * layout.height;
  if (layout.topology === 'square') {
    for (const i of cells) if (i >= 0 && i < max) fillCell(ctx, layout, i, corners);
    return;
  }
  ctx.beginPath();
  for (const i of cells) if (i >= 0 && i < max) addHexSubpath(ctx, layout, i, corners);
  ctx.fill();
}
