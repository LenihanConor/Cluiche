import { describe, it, expect } from 'vitest';
import { createCanvas } from '@napi-rs/canvas';
import { computeLayout } from './cellGeometry';
import type { Topology } from './cellGeometry';
import { colourCells } from './cellColour';
import { drawFill, drawOutlines, drawTint } from './cellDraw';
import type { DrawContext } from './cellDraw';
import type { RampData, ResolvedRamp } from './ramps';

const W = 104, H = 64;
const ramp: ResolvedRamp = {
  ramp: {
    id: 'perf', kind: 'continuous', colours: [],
    stops: [{ t: 0, colour: 0x000000ff }, { t: 0.5, colour: 0xff0000ff }, { t: 1, colour: 0xffffffff }],
  } as RampData,
  min: 0, max: 1, hasRange: true, fallback: false,
};

function everyNth(n: number): Set<number> {
  const s = new Set<number>();
  for (let i = 0; i < W * H; i += n) s.add(i);
  return s;
}

// Budgets are for @napi-rs/canvas, a CPU (software) rasteriser. The editor's browser canvas is GPU-backed, so these are
// a regression guard, not the shipped number: re-check on a real editor build. The spec target is 16 ms per frame.
describe('CellGridCanvas draw performance (104x64, software canvas)', () => {
  for (const topology of ['hex-flat-odd-q', 'square'] as Topology[]) {
    it(`${topology}: fill layer, then selection/outline/footprint layers, within budget`, () => {
      const canvas = createCanvas(1024, 768);
      const ctx = canvas.getContext('2d') as unknown as DrawContext;
      const layout = computeLayout({ topology, width: W, height: H, boxW: 1024, boxH: 768 });
      const values = new Float32Array(W * H).map((_, i) => (i % 97) / 96);
      const selection = everyNth(3), outline = everyNth(5), footprint = everyNth(7);

      const fillFrame = () => drawFill(ctx, layout, colourCells(values, { ramp }));
      const layerFrame = () => {
        drawOutlines(ctx, layout, outline, '#0e639c', 1.5);
        drawTint(ctx, layout, selection, 'rgba(14, 99, 156, 0.45)');
        drawTint(ctx, layout, footprint, 'rgba(255, 255, 255, 0.35)');
      };
      const median = (f: () => void) => {
        for (let i = 0; i < 3; i++) f(); // warm up
        const times: number[] = [];
        for (let i = 0; i < 10; i++) {
          const t0 = performance.now();
          f();
          times.push(performance.now() - t0);
        }
        times.sort((a, b) => a - b);
        return times[Math.floor(times.length / 2)];
      };

      const fill = median(fillFrame);
      const layers = median(layerFrame);
      // Square: whole frame under 16 ms. Hex: layers under 16 ms; the 6,656-polygon fill gets a software-raster allowance.
      expect(layers).toBeLessThan(16);
      expect(fill).toBeLessThan(topology === 'square' ? 16 - layers : 40);
    });
  }
});
