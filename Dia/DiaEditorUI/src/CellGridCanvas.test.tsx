import React from 'react';
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, fireEvent } from '@testing-library/react';
import { CellGridCanvas } from './CellGridCanvas';
import { computeLayout, cellCentre } from './cellGeometry';
import type { CellColouring } from './cellColour';
import type { PaletteData } from './ramps';

interface Rec { fills: string[]; strokes: string[]; strokeStyles: string[]; clears: number; fillRects: number; strokeRects: number }

let recs: Rec[] = [];

function makeCtx(): any {
  const rec: Rec = { fills: [], strokes: [], strokeStyles: [], clears: 0, fillRects: 0, strokeRects: 0 };
  recs.push(rec);
  const ctx: any = {
    fillStyle: '', strokeStyle: '', lineWidth: 1,
    setTransform() {}, beginPath() {}, moveTo() {}, lineTo() {}, closePath() {},
    fill() { rec.fills.push(String(ctx.fillStyle)); },
    stroke() { rec.strokes.push(String(ctx.strokeStyle)); },
    fillRect() { rec.fillRects++; rec.fills.push(String(ctx.fillStyle)); },
    strokeRect() { rec.strokeRects++; rec.strokes.push(String(ctx.strokeStyle)); },
    clearRect() { rec.clears++; },
  };
  return ctx;
}

const palette: PaletteData = {
  entries: [{ id: 'a', colour: 0xff0000ff }, { id: 'b', colour: 0x00ff00ff }],
  fallback: 0x808080ff,
  wrapped: false,
};
const colouring: CellColouring = { palette };

beforeEach(() => {
  recs = [];
  vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockImplementation(function (this: HTMLCanvasElement) {
    const existing = (this as any).__ctx;
    if (existing) return existing;
    const c = makeCtx();
    (this as any).__ctx = c;
    return c;
  } as any);
});
afterEach(() => vi.restoreAllMocks());

const base = () => recs[0];
const overlay = () => recs[1];

describe('CellGridCanvas rendering', () => {
  it('fills one hex per cell with its palette colour', () => {
    const values = new Uint16Array([0, 1, 1, 5]);
    render(<CellGridCanvas topology="hex-flat-odd-q" width={2} height={2} values={values} colouring={colouring} pixelWidth={200} pixelHeight={200} />);
    expect(base().fills).toEqual(['#ff0000', '#00ff00', '#00ff00', '#808080']);
  });

  it('fills squares with fillRect', () => {
    const values = new Uint16Array([0, 1, 1, 0]);
    render(<CellGridCanvas topology="square" width={2} height={2} values={values} colouring={colouring} pixelWidth={200} pixelHeight={200} />);
    expect(base().fillRects).toBe(4);
  });

  it('outlines exactly the highlight set', () => {
    const values = new Uint16Array(9);
    render(
      <CellGridCanvas topology="square" width={3} height={3} values={values} colouring={colouring} highlight={new Set([1, 4])} pixelWidth={150} pixelHeight={150} />,
    );
    expect(base().strokeRects).toBe(2);
  });

  it('draws nothing for an empty highlight', () => {
    render(<CellGridCanvas topology="square" width={2} height={2} values={new Uint16Array(4)} colouring={colouring} highlight={new Set()} pixelWidth={100} pixelHeight={100} />);
    expect(base().strokeRects).toBe(0);
  });

  it('does not redraw when re-rendered with the same props', () => {
    const values = new Uint16Array(4);
    const hl = new Set<number>();
    const el = <CellGridCanvas topology="hex-flat-odd-q" width={2} height={2} values={values} colouring={colouring} highlight={hl} pixelWidth={100} pixelHeight={100} />;
    const { rerender } = render(el);
    const clears = base().clears;
    rerender(el);
    rerender(<CellGridCanvas topology="hex-flat-odd-q" width={2} height={2} values={values} colouring={colouring} highlight={hl} pixelWidth={100} pixelHeight={100} />);
    expect(base().clears).toBe(clears);
  });

  it('redraws when values change', () => {
    const { rerender } = render(<CellGridCanvas topology="square" width={2} height={2} values={new Uint16Array(4)} colouring={colouring} pixelWidth={100} pixelHeight={100} />);
    const clears = base().clears;
    rerender(<CellGridCanvas topology="square" width={2} height={2} values={new Uint16Array([1, 1, 1, 1])} colouring={colouring} pixelWidth={100} pixelHeight={100} />);
    expect(base().clears).toBe(clears + 1);
  });
});

describe('CellGridCanvas pointer events', () => {
  const W = 4, H = 4, PX = 400;
  const layout = computeLayout({ topology: 'hex-flat-odd-q', width: W, height: H, boxW: PX, boxH: PX });
  const setup = (props: Partial<React.ComponentProps<typeof CellGridCanvas>> = {}) =>
    render(<CellGridCanvas topology="hex-flat-odd-q" width={W} height={H} values={new Uint16Array(W * H)} colouring={colouring} pixelWidth={PX} pixelHeight={PX} {...props} />);

  it('onHover reports the cell under the pointer, once per cell change', () => {
    const onHover = vi.fn();
    const { getByTestId } = setup({ onHover });
    const c = cellCentre(layout, 6);
    fireEvent.mouseMove(getByTestId('cell-grid-canvas'), { clientX: c.x, clientY: c.y });
    fireEvent.mouseMove(getByTestId('cell-grid-canvas'), { clientX: c.x + 1, clientY: c.y });
    expect(onHover).toHaveBeenCalledTimes(1);
    expect(onHover).toHaveBeenLastCalledWith({ q: 2, r: 1, index: 6 });
  });

  it('onHover reports null outside the grid and on leave', () => {
    const onHover = vi.fn();
    const { getByTestId } = setup({ onHover });
    const el = getByTestId('cell-grid-canvas');
    const c = cellCentre(layout, 0);
    fireEvent.mouseMove(el, { clientX: c.x, clientY: c.y });
    fireEvent.mouseMove(el, { clientX: PX - 1, clientY: PX - 1 });
    expect(onHover).toHaveBeenLastCalledWith(null);
    fireEvent.mouseMove(el, { clientX: c.x, clientY: c.y });
    fireEvent.mouseLeave(el);
    expect(onHover).toHaveBeenLastCalledWith(null);
  });

  it('hover draws on the overlay without redrawing the base layer', () => {
    const { getByTestId } = setup();
    const baseClears = base().clears;
    const c = cellCentre(layout, 5);
    fireEvent.mouseMove(getByTestId('cell-grid-canvas'), { clientX: c.x, clientY: c.y });
    expect(base().clears).toBe(baseClears);
    expect(overlay().strokes.length).toBe(1);
  });

  it('onCellClick reports the clicked cell and ignores clicks outside the grid', () => {
    const onCellClick = vi.fn();
    const { getByTestId } = setup({ onCellClick });
    const el = getByTestId('cell-grid-canvas');
    const c = cellCentre(layout, 9);
    fireEvent.click(el, { clientX: c.x, clientY: c.y });
    expect(onCellClick).toHaveBeenCalledWith({ q: 1, r: 2, index: 9 });
    fireEvent.click(el, { clientX: PX - 1, clientY: PX - 1 });
    expect(onCellClick).toHaveBeenCalledTimes(1);
  });
});

describe('CellGridCanvas boundary', () => {
  it('imports nothing from CoW, DiaCellLayers or CellField (AC 7)', () => {
    const sources: Record<string, string> = (import.meta as any).glob(
      ['./CellGridCanvas.tsx', './cellDraw.ts', './cellGeometry.ts', './cellColour.ts'],
      { as: 'raw', eager: true },
    );
    expect(Object.keys(sources).length).toBe(4);
    for (const src of Object.values(sources)) {
      const imports = src.split(/\r?\n/).filter((l: string) => /^\s*import /.test(l));
      for (const l of imports) expect(l).not.toMatch(/CoW|DiaCellLayers|CellField/i);
    }
  });
});
