import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render } from '@testing-library/react';
import { CellGridCanvas } from './CellGridCanvas';
import type { CellColouring } from './cellColour';
import { theme } from './theme';

// Ordered op log per canvas: ['fill:#ff0000', 'stroke:#0e639c', ...] so layer order can be asserted.
let logs: string[][] = [];

beforeEach(() => {
  logs = [];
  vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockImplementation(function (this: HTMLCanvasElement) {
    if ((this as any).__ctx) return (this as any).__ctx;
    const log: string[] = [];
    logs.push(log);
    const ctx: any = {
      fillStyle: '', strokeStyle: '', lineWidth: 1,
      setTransform() {}, beginPath() {}, moveTo() {}, lineTo() {}, closePath() {}, clearRect() {},
      fill() {}, stroke() {},
      fillRect() { log.push(`fill:${ctx.fillStyle}`); },
      strokeRect() { log.push(`stroke:${ctx.strokeStyle}`); },
    };
    (this as any).__ctx = ctx;
    return ctx;
  } as any);
});
afterEach(() => vi.restoreAllMocks());

const colouring: CellColouring = {
  palette: { entries: [{ id: 'a', colour: 0xff0000ff }], fallback: 0x808080ff, wrapped: false },
};
const props = { topology: 'square' as const, width: 3, height: 3, values: new Uint16Array(9), colouring, pixelWidth: 150, pixelHeight: 150 };
const tints = (log: string[]) => log.filter(l => l.startsWith('fill:rgba'));

describe('CellGridCanvas extension layers', () => {
  it('draws a selection tint on exactly the selected cells', () => {
    render(<CellGridCanvas {...props} selection={new Set([0, 4, 8])} />);
    expect(tints(logs[0]).length).toBe(3);
    expect(tints(logs[1]).length).toBe(0);
  });

  it('draws an outline on exactly the outlined cells, distinct from the highlight', () => {
    render(<CellGridCanvas {...props} outline={new Set([1, 2])} highlight={new Set([5])} />);
    expect(logs[0].filter(l => l === `stroke:${theme.accentHover}`).length).toBe(2);
    expect(logs[0].filter(l => l === `stroke:${theme.warning}`).length).toBe(1);
  });

  it('draws the footprint translucent on the overlay, not the base', () => {
    render(<CellGridCanvas {...props} footprint={new Set([3, 4])} />);
    expect(tints(logs[1]).length).toBe(2);
    expect(tints(logs[0]).length).toBe(0);
  });

  it('layers in order: fill, outline, highlight, selection', () => {
    render(<CellGridCanvas {...props} selection={new Set([0])} outline={new Set([1])} highlight={new Set([2])} />);
    const log = logs[0];
    const lastFill = log.map(l => l.startsWith('fill:#')).lastIndexOf(true);
    const outline = log.indexOf(`stroke:${theme.accentHover}`);
    const highlight = log.indexOf(`stroke:${theme.warning}`);
    const tint = log.findIndex(l => l.startsWith('fill:rgba'));
    expect(lastFill).toBeLessThan(outline);
    expect(outline).toBeLessThan(highlight);
    expect(highlight).toBeLessThan(tint);
  });

  it('ignores out-of-range indices', () => {
    render(<CellGridCanvas {...props} selection={new Set([-1, 99])} />);
    expect(tints(logs[0]).length).toBe(0);
  });
});
