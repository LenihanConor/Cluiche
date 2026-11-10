import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, fireEvent } from '@testing-library/react';
import { CellGridCanvas } from './CellGridCanvas';
import { computeLayout, cellCentre } from './cellGeometry';
import type { CellColouring } from './cellColour';
import type { ViewState } from './cellGeometry';

beforeEach(() => {
  vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockImplementation(() => ({
    fillStyle: '', strokeStyle: '', lineWidth: 1,
    setTransform() {}, beginPath() {}, moveTo() {}, lineTo() {}, closePath() {}, clearRect() {},
    fill() {}, stroke() {}, fillRect() {}, strokeRect() {},
  }) as any);
});
afterEach(() => vi.restoreAllMocks());

const colouring: CellColouring = { palette: { entries: [], fallback: 0x808080ff, wrapped: false } };
const PX = 400;
const props = { topology: 'square' as const, width: 4, height: 4, values: new Uint16Array(16), colouring, pixelWidth: PX, pixelHeight: PX };

describe('CellGridCanvas view (zoom and pan)', () => {
  it('wheel up zooms in and keeps the point under the cursor fixed', () => {
    const onViewChange = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={{ zoom: 1, panX: 0, panY: 0 }} onViewChange={onViewChange} />);
    const cx = 300, cy = 120;
    fireEvent.wheel(getByTestId('cell-grid-canvas'), { deltaY: -200, clientX: cx, clientY: cy });
    expect(onViewChange).toHaveBeenCalledTimes(1);
    const next: ViewState = onViewChange.mock.calls[0][0];
    expect(next.zoom).toBeGreaterThan(1);

    // The world position under the cursor is unchanged: find the cell under the cursor before and after.
    const before = computeLayout({ topology: 'square', width: 4, height: 4, boxW: PX, boxH: PX, view: { zoom: 1, panX: 0, panY: 0 } });
    const after = computeLayout({ topology: 'square', width: 4, height: 4, boxW: PX, boxH: PX, view: next });
    const wb = [(cx - before.originX) / before.cellSize, (cy - before.originY) / before.cellSize];
    const wa = [(cx - after.originX) / after.cellSize, (cy - after.originY) / after.cellSize];
    expect(wa[0]).toBeCloseTo(wb[0]);
    expect(wa[1]).toBeCloseTo(wb[1]);
  });

  it('wheel down zooms out, clamped to the zoom limits', () => {
    const onViewChange = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={{ zoom: 0.25, panX: 0, panY: 0 }} onViewChange={onViewChange} />);
    fireEvent.wheel(getByTestId('cell-grid-canvas'), { deltaY: 500, clientX: 200, clientY: 200 });
    expect(onViewChange).not.toHaveBeenCalled(); // already at the minimum
  });

  it('wheel does nothing without onViewChange', () => {
    const { getByTestId } = render(<CellGridCanvas {...props} />);
    expect(() => fireEvent.wheel(getByTestId('cell-grid-canvas'), { deltaY: -100 })).not.toThrow();
  });

  it('middle-button drag pans by the pointer delta', () => {
    const onViewChange = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={{ zoom: 2, panX: 10, panY: 20 }} onViewChange={onViewChange} />);
    const el = getByTestId('cell-grid-canvas');
    fireEvent.mouseDown(el, { button: 1, clientX: 100, clientY: 100 });
    fireEvent.mouseMove(el, { clientX: 130, clientY: 90 });
    expect(onViewChange).toHaveBeenLastCalledWith({ zoom: 2, panX: 40, panY: 10 });
    fireEvent.mouseUp(el);
    onViewChange.mockClear();
    fireEvent.mouseMove(el, { clientX: 200, clientY: 200 });
    expect(onViewChange).not.toHaveBeenCalled();
  });

  it('space + left-button drag pans', () => {
    const onViewChange = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={{ zoom: 1, panX: 0, panY: 0 }} onViewChange={onViewChange} />);
    const el = getByTestId('cell-grid-canvas');
    fireEvent.keyDown(window, { code: 'Space' });
    fireEvent.mouseDown(el, { button: 0, clientX: 50, clientY: 50 });
    fireEvent.mouseMove(el, { clientX: 60, clientY: 55 });
    expect(onViewChange).toHaveBeenLastCalledWith({ zoom: 1, panX: 10, panY: 5 });
    fireEvent.mouseUp(el);
    fireEvent.keyUp(window, { code: 'Space' });
  });

  it('plain left drag does not pan', () => {
    const onViewChange = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={{ zoom: 1, panX: 0, panY: 0 }} onViewChange={onViewChange} />);
    const el = getByTestId('cell-grid-canvas');
    fireEvent.mouseDown(el, { button: 0, clientX: 50, clientY: 50 });
    fireEvent.mouseMove(el, { clientX: 80, clientY: 80 });
    expect(onViewChange).not.toHaveBeenCalled();
  });

  it('a pan drag does not produce a cell click', () => {
    const onCellClick = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={{ zoom: 1, panX: 0, panY: 0 }} onViewChange={vi.fn()} onCellClick={onCellClick} />);
    const el = getByTestId('cell-grid-canvas');
    fireEvent.mouseDown(el, { button: 1, clientX: 100, clientY: 100 });
    fireEvent.mouseMove(el, { clientX: 120, clientY: 100 });
    fireEvent.mouseUp(el);
    fireEvent.click(el, { clientX: 120, clientY: 100 });
    expect(onCellClick).not.toHaveBeenCalled();
  });

  it('hover and click hit-test under zoom and pan', () => {
    const view: ViewState = { zoom: 2.5, panX: 37, panY: -21 };
    const layout = computeLayout({ topology: 'square', width: 4, height: 4, boxW: PX, boxH: PX, view });
    const onCellClick = vi.fn();
    const { getByTestId } = render(<CellGridCanvas {...props} view={view} onCellClick={onCellClick} />);
    const c = cellCentre(layout, 10);
    fireEvent.click(getByTestId('cell-grid-canvas'), { clientX: c.x, clientY: c.y });
    expect(onCellClick).toHaveBeenCalledWith({ q: 2, r: 2, index: 10 });
  });
});
