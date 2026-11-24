import fixture from './__fixtures__/jobs.fixture.json';
import { buildHeatStrip, buildStepBars, parseJobsWindow, parseRawJobs, rowsByWorker } from './jobs';
import { drawHeatStrip, drawStepRow, drawWorkerRow, hitStep, kReadyColour, kStallColour } from './drawWorkers';
import { kBackgroundColour, moduleColour } from './drawLane';
import type { MinimalCtx } from './drawLane';

function recorder() {
    const rects: { x: number; y: number; w: number; h: number; fill: string }[] = [];
    const ctx: MinimalCtx = {
        fillStyle: '', strokeStyle: '',
        fillRect(x, y, w, h) { rects.push({ x, y, w, h, fill: String(this.fillStyle) }); },
        beginPath() {}, moveTo() {}, lineTo() {}, stroke() {}, setLineDash() {},
    };
    return { ctx, rects };
}

const win = parseJobsWindow(fixture.window)!;
const sim = win.units[0];
const domain = { t0: 50, t1: 60 };
const names = new Map(sim.owners.map((o) => [o.crc, o.name] as [number, string]));
const raw = parseRawJobs(fixture.raw, names)!;

describe('drawHeatStrip', () => {
    it('paints the background then one owner-coloured rect per segment, time aligned to the domain', () => {
        const { ctx, rects } = recorder();
        const strip = buildHeatStrip(sim, domain);
        drawHeatStrip(ctx, strip, { width: 1000, height: 40, domain });
        expect(rects[0]).toMatchObject({ x: 0, y: 0, w: 1000, h: 40, fill: kBackgroundColour });
        const segs = strip.buckets.reduce((a, b) => a + b.segments.length, 0);
        expect(rects).toHaveLength(1 + segs);
        // bucket 0 spans 50.0-50.2 s over 1000 px = x 0..20, first segment is the weather owner colour
        expect(rects[1].x).toBeCloseTo(0, 6);
        expect(rects[1].w).toBeCloseTo(19.5, 6);
        expect(rects[1].fill).toBe(moduleColour(sim.owners[0].colourIndex));
        for (const r of rects.slice(1)) {
            expect(r.y).toBeGreaterThanOrEqual(-1e-9);
            expect(r.y + r.h).toBeLessThanOrEqual(40 + 1e-6);
        }
    });
    it('the tallest bucket reaches the top and stacks do not overlap', () => {
        const { ctx, rects } = recorder();
        const strip = buildHeatStrip(sim, domain);
        drawHeatStrip(ctx, strip, { width: 1000, height: 40, domain });
        const top = Math.min(...rects.slice(1).map((r) => r.y));
        expect(top).toBeCloseTo(0, 5);
    });
    it('draws only the background for an empty strip and nothing for a zero-size canvas', () => {
        const a = recorder();
        drawHeatStrip(a.ctx, buildHeatStrip(win.units[1], domain), { width: 100, height: 20, domain });
        expect(a.rects).toHaveLength(1);
        const b = recorder();
        drawHeatStrip(b.ctx, buildHeatStrip(sim, domain), { width: 0, height: 20, domain });
        expect(b.rects).toHaveLength(0);
    });
});

describe('drawWorkerRow', () => {
    it('draws a slice per record inside the ns domain with the owner colour and clips the rest', () => {
        const rows = rowsByWorker(raw);
        const w4 = rows.find((r) => r.worker === 4)!;
        const { ctx, rects } = recorder();
        const d = { t0: w4.slices[0].startNs, t1: w4.slices[w4.slices.length - 1].endNs };
        drawWorkerRow(ctx, w4.slices, { width: 800, height: 13, domain: d });
        expect(rects).toHaveLength(1 + w4.slices.length);
        expect(rects[1].x).toBeCloseTo(0, 6);
        expect(rects[1].fill).toBe(moduleColour(w4.slices[0].colourIndex));
        const none = recorder();
        drawWorkerRow(none.ctx, w4.slices, { width: 800, height: 13, domain: { t0: 0, t1: 1000 } });
        expect(none.rects).toHaveLength(1);
    });
});

describe('drawStepRow and hitStep', () => {
    const rows = buildStepBars(win.steps);
    const eco = rows.channels[1].bars[0];
    const weather = rows.channels[0].bars[0];

    it('late steps get a stall block, on-time steps a ready tick', () => {
        const a = recorder();
        drawStepRow(a.ctx, [eco], { width: 1000, height: 16, domain: rows.domain });
        expect(a.rects.some((r) => r.fill === kStallColour)).toBe(true);
        expect(a.rects.some((r) => r.fill === kReadyColour)).toBe(false);
        const b = recorder();
        drawStepRow(b.ctx, [weather], { width: 1000, height: 16, domain: rows.domain });
        expect(b.rects.some((r) => r.fill === kReadyColour)).toBe(true);
        expect(b.rects.some((r) => r.fill === kStallColour)).toBe(false);
    });
    it('the stall block starts at the apply key', () => {
        const { ctx, rects } = recorder();
        drawStepRow(ctx, [eco], { width: 1000, height: 16, domain: rows.domain });
        const stall = rects.find((r) => r.fill === kStallColour)!;
        const xa = ((eco.apply - rows.domain.t0) / (rows.domain.t1 - rows.domain.t0)) * 1000;
        expect(stall.x).toBeCloseTo(xa, 6);
    });
    it('hit-tests kick..max(apply, ready)', () => {
        const w = 1000;
        const xAt = (t: number) => ((t - rows.domain.t0) / (rows.domain.t1 - rows.domain.t0)) * w;
        expect(hitStep(rows.channels[1].bars, xAt(eco.apply + 1e6), w, rows.domain)?.key).toBe(eco.key);
        expect(hitStep(rows.channels[1].bars, xAt(eco.ready + 50e6), w, rows.domain)).toBeNull();
        expect(hitStep([], 5, w, rows.domain)).toBeNull();
        expect(hitStep(rows.channels[1].bars, 5, 0, rows.domain)).toBeNull();
    });
});
