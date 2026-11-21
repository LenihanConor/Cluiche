// Pure canvas drawing for the Workers tab. No React or DOM imports: draws through the same tiny structural 2D-context
// interface as drawLane so tests can pass a recording fake. CSS px, caller applies devicePixelRatio.

import type { TimeDomain } from './laneModel';
import type { HeatStrip, RawSlice, StepBar } from './jobs';
import { kBackgroundColour, moduleColour } from './drawLane';
import type { MinimalCtx } from './drawLane';

export interface WorkerDrawOptions { width: number; height: number; domain: TimeDomain }

export const kStepOutlineAlpha = 'rgba(200, 200, 200, 0.55)';
export const kStallColour = '#f48771';
export const kReadyColour = '#7fd18b';
export const kStepRunColour = '#4ec9b0';
export const kSelectedColour = '#ffffff';

const xOf = (t: number, d: TimeDomain, width: number): number => ((t - d.t0) / (d.t1 - d.t0)) * width;

function clearBackground(ctx: MinimalCtx, o: WorkerDrawOptions): boolean {
    if (!(o.width > 0) || !(o.height > 0) || !(o.domain.t1 > o.domain.t0)) return false;
    ctx.fillStyle = kBackgroundColour;
    ctx.fillRect(0, 0, o.width, o.height);
    return true;
}

/** Owner-coloured stacked busy time per 200 ms bucket; bar height is relative to the largest visible bucket. */
export function drawHeatStrip(ctx: MinimalCtx, strip: HeatStrip, o: WorkerDrawOptions): void {
    if (!clearBackground(ctx, o)) return;
    if (!(strip.maxTotalMs > 0)) return;
    for (const b of strip.buckets) {
        const x0 = Math.max(0, xOf(b.t0, o.domain, o.width));
        const x1 = Math.min(o.width, xOf(b.t1, o.domain, o.width));
        const w = Math.max(1, x1 - x0 - 0.5);
        let acc = 0;
        for (const s of b.segments) {
            const h = Math.max(0.5, (s.busyMs / strip.maxTotalMs) * o.height);
            const y = Math.max(0, Math.min(o.height - h, o.height - ((acc + s.busyMs) / strip.maxTotalMs) * o.height));
            ctx.fillStyle = moduleColour(s.colourIndex);
            ctx.fillRect(x0, y, w, h);
            acc += s.busyMs;
        }
    }
}

/** One worker row of raw slices, owner coloured; `selectedOwner` dims everything else when given. Domain is in ns. */
export function drawWorkerRow(ctx: MinimalCtx, slices: RawSlice[], o: WorkerDrawOptions, highlight?: (s: RawSlice) => boolean): void {
    if (!clearBackground(ctx, o)) return;
    for (const s of slices) {
        if (s.endNs < o.domain.t0 || s.startNs > o.domain.t1) continue;
        const x = Math.max(0, xOf(s.startNs, o.domain, o.width));
        const x2 = Math.min(o.width, xOf(s.endNs, o.domain, o.width));
        ctx.fillStyle = moduleColour(s.colourIndex);
        ctx.fillRect(x, 1, Math.max(1, x2 - x), Math.max(1, o.height - 2));
        if (highlight && highlight(s)) {
            ctx.fillStyle = kSelectedColour;
            ctx.fillRect(x, 0, Math.max(1, x2 - x), 1);
        }
    }
}

/**
 * One deferred-step channel row. Outline = kick to apply window (four 1 px edges), fill = first start to ready (clipped at
 * apply), red = stall (apply to ready) when late, green tick at the ready point when on time. Domain is in ns.
 */
export function drawStepRow(ctx: MinimalCtx, bars: StepBar[], o: WorkerDrawOptions, selectedKey: string | null = null): void {
    if (!clearBackground(ctx, o)) return;
    const h = o.height;
    for (const b of bars) {
        const xk = xOf(b.kick, o.domain, o.width);
        const xa = xOf(b.apply, o.domain, o.width);
        const xr = xOf(b.ready, o.domain, o.width);
        const xf = xOf(b.firstStart, o.domain, o.width);
        const sel = b.key === selectedKey;
        ctx.fillStyle = sel ? kSelectedColour : kStepOutlineAlpha;
        const w = Math.max(1, xa - xk);
        ctx.fillRect(xk, 2, w, 1);
        ctx.fillRect(xk, h - 3, w, 1);
        ctx.fillRect(xk, 2, 1, h - 4);
        ctx.fillRect(xa, 2, 1, h - 4);
        ctx.fillStyle = kStepRunColour;
        ctx.fillRect(xf, 5, Math.max(1, Math.min(xr, xa) - xf), Math.max(1, h - 10));
        if (b.late) {
            ctx.fillStyle = kStallColour;
            ctx.fillRect(xa, 3, Math.max(2, xr - xa), Math.max(1, h - 6));
        } else {
            ctx.fillStyle = kReadyColour;
            ctx.fillRect(xr, 2, 1, h - 4);
        }
    }
}

/** Bar under pixel x (kick to the later of apply / ready), or null. Same x mapping as drawStepRow. */
export function hitStep(bars: StepBar[], x: number, width: number, domain: TimeDomain): StepBar | null {
    if (!(width > 0) || !(domain.t1 > domain.t0)) return null;
    const t = domain.t0 + (x / width) * (domain.t1 - domain.t0);
    for (const b of bars) if (t >= b.kick && t <= Math.max(b.apply, b.ready)) return b;
    return null;
}
