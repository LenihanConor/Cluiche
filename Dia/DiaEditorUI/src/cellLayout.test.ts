import { describe, it, expect } from 'vitest';
import { computeLayout, cellCentre, cellFromPoint, hexCorners } from './cellGeometry';
import type { Topology } from './cellGeometry';

const topologies: Topology[] = ['hex-flat-odd-q', 'square'];
const view = { zoom: 2.5, panX: 37, panY: -21 };
const sizes: Array<[number, number]> = [[3, 3], [104, 64], [1, 1], [5, 1]];

describe('layout and hit-testing', () => {
  for (const topology of topologies) {
    for (const [w, h] of sizes) {
      for (const v of [undefined, view]) {
        it(`${topology} ${w}x${h} ${v ? 'zoom+pan' : 'fit'}: every centre round-trips`, () => {
          const L = computeLayout({ topology, width: w, height: h, boxW: 800, boxH: 600, view: v });
          for (let i = 0; i < w * h; i++) {
            const c = cellCentre(L, i);
            expect(cellFromPoint(L, c.x, c.y)).toEqual({ q: i % w, r: Math.floor(i / w), index: i });
          }
        });
      }
    }

    it(`${topology}: fit keeps the whole grid inside the box`, () => {
      const L = computeLayout({ topology, width: 104, height: 64, boxW: 800, boxH: 600 });
      for (const i of [0, 103, 104 * 63, 104 * 64 - 1, 1, 104 * 63 + 1]) {
        const c = cellCentre(L, i);
        expect(c.x).toBeGreaterThan(0);
        expect(c.x).toBeLessThan(800);
        expect(c.y).toBeGreaterThan(0);
        expect(c.y).toBeLessThan(600);
      }
    });

    it(`${topology}: points outside the grid return null`, () => {
      const L = computeLayout({ topology, width: 4, height: 4, boxW: 400, boxH: 400 });
      expect(cellFromPoint(L, -50, 200)).toBeNull();
      expect(cellFromPoint(L, 200, 1000)).toBeNull();
      expect(cellFromPoint(L, 1000, 1000)).toBeNull();
    });
  }

  it('hex: points just inside each edge belong to the cell, just past it to the neighbour', () => {
    const L = computeLayout({ topology: 'hex-flat-odd-q', width: 6, height: 6, boxW: 600, boxH: 600 });
    const centre = 2 * 6 + 2; // (2,2), even column
    const c = cellCentre(L, centre);
    const inradius = (L.cellSize * Math.sqrt(3)) / 2;
    // Screen angles (y down) of the six edge normals, and the neighbour on that side.
    const cases: Array<[number, number]> = [
      [-90, 1 * 6 + 2], // up: (2,1)
      [90, 3 * 6 + 2], // down: (2,3)
      [150, 2 * 6 + 1], // down-left: (1,2)
      [-150, 1 * 6 + 1], // up-left: (1,1)
      [30, 2 * 6 + 3], // down-right: (3,2)
      [-30, 1 * 6 + 3], // up-right: (3,1)
    ];
    for (const [deg, neighbour] of cases) {
      const a = (deg * Math.PI) / 180;
      const dx = Math.cos(a), dy = Math.sin(a);
      expect(cellFromPoint(L, c.x + dx * inradius * 0.95, c.y + dy * inradius * 0.95)?.index).toBe(centre);
      expect(cellFromPoint(L, c.x + dx * inradius * 1.05, c.y + dy * inradius * 1.05)?.index).toBe(neighbour);
    }
  });

  it('square: cell edges', () => {
    const L = computeLayout({ topology: 'square', width: 4, height: 4, boxW: 400, boxH: 400 });
    const c = cellCentre(L, 5);
    const half = L.cellSize / 2;
    expect(cellFromPoint(L, c.x + half * 0.95, c.y)?.index).toBe(5);
    expect(cellFromPoint(L, c.x + half * 1.05, c.y)?.index).toBe(6);
    expect(cellFromPoint(L, c.x, c.y - half * 1.05)?.index).toBe(1);
  });

  it('zoom scales cell size about the box centre and pan translates', () => {
    const a = computeLayout({ topology: 'square', width: 4, height: 4, boxW: 400, boxH: 400 });
    const b = computeLayout({ topology: 'square', width: 4, height: 4, boxW: 400, boxH: 400, view: { zoom: 2, panX: 10, panY: 0 } });
    expect(b.cellSize).toBeCloseTo(a.cellSize * 2);
    const ca = cellCentre(a, 5);
    const cb = cellCentre(b, 5);
    expect(cb.x).toBeCloseTo(200 + (ca.x - 200) * 2 + 10);
    expect(cb.y).toBeCloseTo(200 + (ca.y - 200) * 2);
  });

  it('hexCorners returns six flat-top corners at the given radius', () => {
    const k = hexCorners(10);
    expect(k.length).toBe(6);
    expect(k[0].x).toBeCloseTo(10);
    expect(k[0].y).toBeCloseTo(0);
    expect(k[1].x).toBeCloseTo(5);
    expect(k[1].y).toBeCloseTo((10 * Math.sqrt(3)) / 2);
  });
});
