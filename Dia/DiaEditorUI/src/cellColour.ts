// Value -> colour mapping for CellGridCanvas. Reuses the ramp and palette types from ramps.ts so DiaValueSchema output
// feeds the canvas unchanged. A palette value is the enum ordinal (position in PaletteData.entries).

import { rampT, sampleRamp } from './ramps';
import type { PaletteData, ResolvedRamp, Rgba8 } from './ramps';

export type CellColouring = { ramp: ResolvedRamp } | { palette: PaletteData };

/** Colour for one value. Ramp: clamped to [min,max], NaN -> low end. Palette: unknown ordinal -> fallback. */
export function colourForValue(colouring: CellColouring, value: number): Rgba8 {
  if ('ramp' in colouring) return sampleRamp(colouring.ramp.ramp, rampT(colouring.ramp, value));
  const { entries, fallback } = colouring.palette;
  if (!Number.isInteger(value) || value < 0 || value >= entries.length) return fallback;
  return entries[value].colour;
}

/** Colours for a whole grid, one Rgba8 per cell. */
export function colourCells(values: Float32Array | Uint16Array, colouring: CellColouring): Uint32Array {
  const out = new Uint32Array(values.length);
  for (let i = 0; i < values.length; i++) out[i] = colourForValue(colouring, values[i]);
  return out;
}
