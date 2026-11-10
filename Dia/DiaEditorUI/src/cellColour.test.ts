import { describe, it, expect } from 'vitest';
import { colourForValue, colourCells } from './cellColour';
import type { CellColouring } from './cellColour';
import type { RampData, ResolvedRamp, PaletteData } from './ramps';

const greyRamp: RampData = {
  id: 'test.grey',
  kind: 'continuous',
  stops: [{ t: 0, colour: 0x000000ff }, { t: 1, colour: 0xffffffff }],
  colours: [],
};
const ramp: ResolvedRamp = { ramp: greyRamp, min: 10, max: 20, hasRange: true, fallback: false };
const rampColouring: CellColouring = { ramp };

const palette: PaletteData = {
  entries: [{ id: 'a', colour: 0xff0000ff }, { id: 'b', colour: 0x00ff00ff }, { id: 'c', colour: 0x0000ffff }],
  fallback: 0x808080ff,
  wrapped: false,
};
const paletteColouring: CellColouring = { palette };

describe('ramp colouring', () => {
  it('maps min, max and midpoint', () => {
    expect(colourForValue(rampColouring, 10)).toBe(0x000000ff);
    expect(colourForValue(rampColouring, 20)).toBe(0xffffffff);
    expect(colourForValue(rampColouring, 15)).toBe(0x808080ff);
  });
  it('clamps values outside the range', () => {
    expect(colourForValue(rampColouring, -100)).toBe(0x000000ff);
    expect(colourForValue(rampColouring, 1e9)).toBe(0xffffffff);
  });
  it('maps NaN to the low end', () => {
    expect(colourForValue(rampColouring, NaN)).toBe(0x000000ff);
  });
});

describe('palette colouring (value is the enum ordinal)', () => {
  it('maps ordinals to entries', () => {
    expect(colourForValue(paletteColouring, 0)).toBe(0xff0000ff);
    expect(colourForValue(paletteColouring, 2)).toBe(0x0000ffff);
  });
  it('uses fallback for unknown, negative, fractional and NaN values', () => {
    for (const v of [3, 99, -1, 1.5, NaN]) expect(colourForValue(paletteColouring, v)).toBe(0x808080ff);
  });
});

describe('colourCells', () => {
  it('colours a Uint16Array palette grid', () => {
    const out = colourCells(new Uint16Array([0, 1, 2, 7]), paletteColouring);
    expect(Array.from(out)).toEqual([0xff0000ff, 0x00ff00ff, 0x0000ffff, 0x808080ff]);
  });
  it('colours a Float32Array ramp grid', () => {
    const out = colourCells(new Float32Array([10, 15, 20]), rampColouring);
    expect(Array.from(out)).toEqual([0x000000ff, 0x808080ff, 0xffffffff]);
  });
});
