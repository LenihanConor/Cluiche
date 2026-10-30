import { describe, it, expect } from 'vitest';
import { hexDistance, cellsWithin, cellsInBox } from './cellGeometry';

const W = 8;
const idx = (q: number, r: number) => r * W + q;

describe('hexDistance (flat-top odd-q)', () => {
  it('is zero for the same cell', () => {
    expect(hexDistance(idx(3, 2), idx(3, 2), W)).toBe(0);
  });
  it('even column to odd-column neighbours (r-1 side is not shifted)', () => {
    expect(hexDistance(idx(0, 0), idx(1, 0), W)).toBe(1);
    expect(hexDistance(idx(2, 2), idx(3, 2), W)).toBe(1);
    expect(hexDistance(idx(2, 2), idx(3, 1), W)).toBe(1);
  });
  it('odd column to even-column neighbours (shifted down half a cell)', () => {
    expect(hexDistance(idx(1, 0), idx(2, 1), W)).toBe(1);
    expect(hexDistance(idx(1, 0), idx(2, 0), W)).toBe(1);
    expect(hexDistance(idx(0, 0), idx(1, 1), W)).toBe(2);
  });
  it('vertical and long diagonal', () => {
    expect(hexDistance(idx(4, 0), idx(4, 5), W)).toBe(5);
    expect(hexDistance(idx(0, 0), idx(3, 3), W)).toBe(5);
  });
  it('is symmetric', () => {
    expect(hexDistance(idx(1, 4), idx(6, 0), W)).toBe(hexDistance(idx(6, 0), idx(1, 4), W));
  });
});

describe('cellsWithin', () => {
  it('radius 0 is the centre only', () => {
    expect(cellsWithin(idx(3, 3), 0, W, 8)).toEqual([idx(3, 3)]);
  });
  it('radius 1 of an even-column cell has 7 cells', () => {
    const c = cellsWithin(idx(2, 3), 1, W, 8);
    expect(c.length).toBe(7);
    expect(c).toEqual(expect.arrayContaining([idx(2, 2), idx(2, 4), idx(1, 2), idx(1, 3), idx(3, 2), idx(3, 3), idx(2, 3)]));
  });
  it('radius 1 of an odd-column cell has 7 cells', () => {
    const c = cellsWithin(idx(3, 3), 1, W, 8);
    expect(c.length).toBe(7);
    expect(c).toEqual(expect.arrayContaining([idx(3, 2), idx(3, 4), idx(2, 3), idx(2, 4), idx(4, 3), idx(4, 4), idx(3, 3)]));
  });
  it('radius 2 of an interior cell has 19 cells', () => {
    expect(cellsWithin(idx(3, 3), 2, W, 8).length).toBe(19);
  });
  it('clips at the grid edge and is sorted by index', () => {
    const c = cellsWithin(idx(0, 0), 1, W, 8);
    expect(c).toEqual([idx(0, 0), idx(1, 0), idx(0, 1)]);
  });
});

describe('cellsInBox', () => {
  it('single cell', () => {
    expect(cellsInBox(idx(2, 2), idx(2, 2), W)).toEqual([idx(2, 2)]);
  });
  it('covers the offset rectangle, row-major', () => {
    expect(cellsInBox(idx(1, 1), idx(3, 2), W)).toEqual([idx(1, 1), idx(2, 1), idx(3, 1), idx(1, 2), idx(2, 2), idx(3, 2)]);
  });
  it('is symmetric in argument order and works for opposite corners', () => {
    expect(cellsInBox(idx(3, 2), idx(1, 1), W)).toEqual(cellsInBox(idx(1, 1), idx(3, 2), W));
    expect(cellsInBox(idx(1, 2), idx(3, 1), W)).toEqual(cellsInBox(idx(1, 1), idx(3, 2), W));
  });
});
