// Pure canvas drawing for one perf lane. No React or DOM imports: draws through a tiny structural
// 2D-context interface so tests can pass a recording fake.
//
// Layout (CSS px, caller applies devicePixelRatio): x = time over `domain` (identical for all lanes),
// y = ms over [0, max(budget * 1.25, lane max)], newest at the right.

import type { BucketedLane, Lane, TimeDomain, TripSpan } from './laneModel';

export interface MinimalCtx {
    fillStyle: string;
    strokeStyle: string;
    fillRect(x: number, y: number, w: number, h: number): void;
    beginPath(): void;
    moveTo(x: number, y: number): void;
    lineTo(x: number, y: number): void;
    stroke(): void;
    setLineDash(segments: number[]): void;
}

export interface DrawOptions {
    /** Lane size in CSS px. */
    width: number;
    height: number;
    domain: TimeDomain;
    /** Trip shading spans in seconds (see tripSpans), already clipped to the domain. */
    spans?: TripSpan[];
}

/** Module colours: the 12 module hues of the approved mockup, indexed by the model's colourIndex. */
export const kModulePalette: readonly string[] = [
    '#569cd6', '#c586c0', '#f48771', '#c8c8c8', '#d7ba7d', '#6a9955',
    '#4ec9b0', '#ce9178', '#dcdcaa', '#9cdcfe', '#b5cea8', '#4fc1ff',
];

export const kBackgroundColour = '#1a1a1a';
/** Unattributed remainder of a tick (no module owns it). */
export const kNeutralColour = '#5a5a5a';
export const kBudgetColour = '#d7ba7d';
export const kTripColour = '#f48771';
export const kTripFill = 'rgba(244, 135, 113, 0.10)';
export const kGapFill = 'rgba(128, 128, 128, 0.18)';
export const kOverBudgetColour = '#f48771';
export const kClampedColour = '#ffffff';

const kTickHeight = 2;
const kMarkerSize = 3;

export function moduleColour(colourIndex: number): string {
    const n = kModulePalette.length;
    return kModulePalette[((colourIndex % n) + n) % n];
}

/** Top of the y scale for a lane: room above the budget line, or the largest tick if higher. */
export function laneYMax(lane: Lane): number {
    return Math.max(lane.budgetMs * 1.25, lane.maxMs);
}

export function drawLane(ctx: MinimalCtx, lane: Lane, buckets: BucketedLane, opts: DrawOptions): void {
    const { width, height, domain } = opts;
    if (!(width > 0) || !(height > 0)) return;

    ctx.fillStyle = kBackgroundColour;
    ctx.fillRect(0, 0, width, height);

    const yMax = laneYMax(lane);
    if (!(yMax > 0)) return;
    const y = (ms: number) => height - (Math.min(ms, yMax) / yMax) * height;

    const span = domain.t1 - domain.t0;
    const spans = opts.spans ?? [];
    const x = (t: number) => ((t - domain.t0) / span) * width;

    if (span > 0 && spans.length > 0) {
        ctx.fillStyle = kTripFill;
        for (const s of spans) ctx.fillRect(x(s.t0), 0, x(s.t1) - x(s.t0), height);
    }

    const cols = buckets.columns;
    const m = lane.modules.length;
    if (cols > 0) {
        const colW = width / cols;
        for (let c = 0; c < cols; c++) {
            if (buckets.firstIndex[c] < 0) continue; // no entries in this column
            const cx = c * colW;
            if (buckets.hasGap[c]) {
                ctx.fillStyle = kGapFill;
                ctx.fillRect(cx, 0, colW, height);
                continue;
            }
            let acc = 0;
            for (let k = 0; k < m; k++) {
                const v = buckets.moduleMean[k * cols + c];
                if (!(v > 0)) continue;
                const top = y(acc + v);
                const bottom = y(acc);
                acc += v;
                if (bottom > top) {
                    ctx.fillStyle = moduleColour(lane.modules[k].colourIndex);
                    ctx.fillRect(cx, top, colW, bottom - top);
                }
            }
            const remainder = buckets.tickMax[c] - acc;
            if (remainder > 0) {
                const top = y(acc + remainder);
                const bottom = y(acc);
                if (bottom > top) {
                    ctx.fillStyle = kNeutralColour;
                    ctx.fillRect(cx, top, colW, bottom - top);
                }
                acc += remainder;
            }
            const stackTop = y(acc);
            if (buckets.overBudget[c]) {
                ctx.fillStyle = kOverBudgetColour;
                ctx.fillRect(cx, stackTop, colW, kTickHeight);
            }
            if (buckets.hasClamped[c]) {
                ctx.fillStyle = kClampedColour;
                ctx.fillRect(cx, stackTop, Math.max(colW, kMarkerSize), kMarkerSize);
            }
        }
    }

    // Budget line
    const by = y(lane.budgetMs);
    ctx.strokeStyle = kBudgetColour;
    ctx.setLineDash([4, 3]);
    ctx.beginPath();
    ctx.moveTo(0, by);
    ctx.lineTo(width, by);
    ctx.stroke();
    ctx.setLineDash([]);

    // Trip start edges
    if (span > 0 && spans.length > 0) {
        ctx.strokeStyle = kTripColour;
        ctx.beginPath();
        for (const s of spans) {
            ctx.moveTo(x(s.t0), 0);
            ctx.lineTo(x(s.t0), height);
        }
        ctx.stroke();
    }
}
