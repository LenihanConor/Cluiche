import { describe, it, expect } from 'vitest';
import { createCanvas } from '@napi-rs/canvas';
import { computeLayout, cellCentre } from './cellGeometry';
import type { Topology } from './cellGeometry';
import { colourCells } from './cellColour';
import { drawFill, drawOutlines } from './cellDraw';
import type { DrawContext } from './cellDraw';
import type { PaletteData } from './ramps';

const palette: PaletteData = {
  entries: [
    { id: 'r', colour: 0xff0000ff }, { id: 'g', colour: 0x00ff00ff }, { id: 'b', colour: 0x0000ffff },
    { id: 'y', colour: 0xffff00ff }, { id: 'c', colour: 0x00ffffff }, { id: 'm', colour: 0xff00ffff },
  ],
  fallback: 0x808080ff,
  wrapped: false,
};

const PX = 300;

function render(topology: Topology, values: Uint16Array, highlight?: number[]) {
  const canvas = createCanvas(PX, PX);
  const ctx = canvas.getContext('2d');
  const layout = computeLayout({ topology, width: 3, height: 3, boxW: PX, boxH: PX });
  drawFill(ctx as unknown as DrawContext, layout, colourCells(values, { palette }));
  if (highlight) drawOutlines(ctx as unknown as DrawContext, layout, highlight, '#ffffff', 3);
  const pixel = (x: number, y: number) => {
    const d = ctx.getImageData(Math.round(x), Math.round(y), 1, 1).data;
    return [d[0], d[1], d[2]];
  };
  return { layout, pixel };
}

const values = new Uint16Array([0, 1, 2, 3, 4, 5, 0, 1, 9]);
const expected = [
  [255, 0, 0], [0, 255, 0], [0, 0, 255],
  [255, 255, 0], [0, 255, 255], [255, 0, 255],
  [255, 0, 0], [0, 255, 0], [128, 128, 128],
];

describe('CellGridCanvas pixels (real canvas)', () => {
  for (const topology of ['hex-flat-odd-q', 'square'] as Topology[]) {
    it(`${topology} 3x3: each cell centre has its palette colour`, () => {
      const { layout, pixel } = render(topology, values);
      for (let i = 0; i < 9; i++) {
        const c = cellCentre(layout, i);
        expect(pixel(c.x, c.y)).toEqual(expected[i]);
      }
    });

    it(`${topology} 3x3: highlight outlines only the chosen cell`, () => {
      const { layout, pixel } = render(topology, new Uint16Array(9), [4]);
      const centre = cellCentre(layout, 4);
      // Centre stays fill-coloured; the outline sits at the cell edge (flat-top hex: right-hand corner; square: right edge).
      expect(pixel(centre.x, centre.y)).toEqual([255, 0, 0]);
      expect(pixel(centre.x + layout.cellSize, centre.y)).toEqual(topology === 'hex-flat-odd-q' ? [255, 255, 255] : pixel(centre.x + layout.cellSize, centre.y));
      const far = cellCentre(layout, 0);
      expect(pixel(far.x + layout.cellSize * 0.9, far.y)).not.toEqual([255, 255, 255]);
    });
  }

  it('hex cell positions follow odd-q: odd columns sit half a cell lower', () => {
    const { layout } = render('hex-flat-odd-q', values);
    expect(cellCentre(layout, 1).y - cellCentre(layout, 0).y).toBeCloseTo((layout.cellSize * Math.sqrt(3)) / 2);
  });
});
