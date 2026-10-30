import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { render, fireEvent } from '@testing-library/react';
import { CellGridCanvas } from './CellGridCanvas';
import { computeLayout, cellCentre, cellsInBox } from './cellGeometry';
import type { Topology } from './cellGeometry';
import type { CellColouring } from './cellColour';

beforeEach(() => {
  vi.spyOn(HTMLCanvasElement.prototype, 'getContext').mockImplementation(() => ({
    fillStyle: '', strokeStyle: '', lineWidth: 1,
    setTransform() {}, beginPath() {}, moveTo() {}, lineTo() {}, closePath() {}, clearRect() {},
    fill() {}, stroke() {}, fillRect() {}, strokeRect() {},
  }) as any);
});
afterEach(() => vi.restoreAllMocks());

const colouring: CellColouring = { palette: { entries: [], fallback: 0x808080ff, wrapped: false } };
const W = 8, H = 8, PX = 480;
const make = (topology: Topology, extra: Record<string, unknown>) => {
  const layout = computeLayout({ topology, width: W, height: H, boxW: PX, boxH: PX });
  const utils = render(
    <CellGridCanvas topology={topology} width={W} height={H} values={new Uint16Array(W * H)} colouring={colouring} pixelWidth={PX} pixelHeight={PX} {...extra} />,
  );
  return { layout, el: utils.getByTestId('cell-grid-canvas') };
};
const at = (L: ReturnType<typeof computeLayout>, i: number) => cellCentre(L, i);

describe('CellGridCanvas box gesture', () => {
  for (const topology of ['hex-flat-odd-q', 'square'] as Topology[]) {
    it(`${topology}: left-drag reports the offset rectangle on mouseup`, () => {
      const onBox = vi.fn();
      const { layout, el } = make(topology, { onBox });
      const a = at(layout, 1 * W + 1), b = at(layout, 3 * W + 4);
      fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
      fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
      expect(onBox).not.toHaveBeenCalled();
      fireEvent.mouseUp(el, { clientX: b.x, clientY: b.y });
      expect(onBox).toHaveBeenCalledTimes(1);
      expect(onBox).toHaveBeenCalledWith(cellsInBox(1 * W + 1, 3 * W + 4, W), false);
    });
  }

  it('shift makes the box additive', () => {
    const onBox = vi.fn();
    const { layout, el } = make('square', { onBox });
    const a = at(layout, 0), b = at(layout, 9);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
    fireEvent.mouseUp(el, { clientX: b.x, clientY: b.y, shiftKey: true });
    expect(onBox).toHaveBeenCalledWith(cellsInBox(0, 9, W), true);
  });

  it('a click without movement is a click, not a box', () => {
    const onBox = vi.fn();
    const onCellClick = vi.fn();
    const { layout, el } = make('square', { onBox, onCellClick });
    const a = at(layout, 10);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    fireEvent.mouseUp(el, { clientX: a.x, clientY: a.y });
    fireEvent.click(el, { clientX: a.x, clientY: a.y });
    expect(onBox).not.toHaveBeenCalled();
    expect(onCellClick).toHaveBeenCalledWith({ q: 2, r: 1, index: 10 });
  });

  it('a box drag does not also produce a cell click', () => {
    const onCellClick = vi.fn();
    const { layout, el } = make('square', { onBox: vi.fn(), onCellClick });
    const a = at(layout, 0), b = at(layout, 18);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
    fireEvent.mouseUp(el, { clientX: b.x, clientY: b.y });
    fireEvent.click(el, { clientX: b.x, clientY: b.y });
    expect(onCellClick).not.toHaveBeenCalled();
  });

  it('a drag that starts outside the grid reports no box', () => {
    const onBox = vi.fn();
    const { layout, el } = make('hex-flat-odd-q', { onBox });
    const b = at(layout, 20);
    fireEvent.mouseDown(el, { button: 0, clientX: 0, clientY: 0 });
    fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
    fireEvent.mouseUp(el, { clientX: b.x, clientY: b.y });
    expect(onBox).not.toHaveBeenCalled();
  });
});

describe('CellGridCanvas stroke gesture', () => {
  it('reports start, move and end; each cell once', () => {
    const onStroke = vi.fn();
    const onBox = vi.fn();
    const { layout, el } = make('square', { onStroke, onBox, strokeActive: true });
    const a = at(layout, 0), b = at(layout, 3);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    expect(onStroke).toHaveBeenLastCalledWith([0], 'start');
    fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
    fireEvent.mouseMove(el, { clientX: b.x - 2, clientY: b.y });
    fireEvent.mouseUp(el, { clientX: b.x, clientY: b.y });
    const moves = onStroke.mock.calls.filter(c => c[1] === 'move').flatMap(c => c[0] as number[]);
    expect(moves).toEqual([1, 2, 3]);
    expect(onStroke).toHaveBeenLastCalledWith([], 'end');
    expect(onBox).not.toHaveBeenCalled();
  });

  it('fills the gap when the pointer jumps several cells in one event', () => {
    const onStroke = vi.fn();
    const { layout, el } = make('hex-flat-odd-q', { onStroke, strokeActive: true });
    const a = at(layout, 2 * W + 1), b = at(layout, 2 * W + 6);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
    const cols = new Set(onStroke.mock.calls.flatMap(c => c[0] as number[]).map(i => i % W));
    for (const q of [1, 2, 3, 4, 5, 6]) expect(cols.has(q)).toBe(true);
  });

  it('ends the stroke when the pointer leaves', () => {
    const onStroke = vi.fn();
    const { layout, el } = make('square', { onStroke, strokeActive: true });
    const a = at(layout, 9);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    fireEvent.mouseLeave(el);
    expect(onStroke).toHaveBeenLastCalledWith([], 'end');
  });

  it('without strokeActive, onStroke is never called', () => {
    const onStroke = vi.fn();
    const { layout, el } = make('square', { onStroke, onBox: vi.fn() });
    const a = at(layout, 0), b = at(layout, 5);
    fireEvent.mouseDown(el, { button: 0, clientX: a.x, clientY: a.y });
    fireEvent.mouseMove(el, { clientX: b.x, clientY: b.y });
    fireEvent.mouseUp(el, { clientX: b.x, clientY: b.y });
    expect(onStroke).not.toHaveBeenCalled();
  });
});
