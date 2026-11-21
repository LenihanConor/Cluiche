// Pure frame-selection helpers. No React or DOM imports.

import type { Lane, TimeDomain } from './laneModel';

export interface SelectedFrame { unit: string; index: number }

const kMinOutlinePx = 3;

/** Pointer x (px from the left of the lane canvas) to seconds, using the same domain drawLane uses. */
export function timeAtX(x: number, width: number, domain: TimeDomain): number {
    if (!(width > 0)) return NaN;
    return domain.t0 + (x / width) * (domain.t1 - domain.t0);
}

/**
 * Outline rect (px) of an entry: the entry's period, ending at its time (entries are drawn at the right edge
 * of their column), at least 3 px wide. Null when the entry lies outside the domain or width is 0.
 */
export function selectionRect(
    lane: Lane, index: number, domain: TimeDomain, width: number,
): { left: number; width: number } | null {
    const span = domain.t1 - domain.t0;
    if (!(width > 0) || !(span > 0) || !(lane.hz > 0)) return null;
    const t = index / lane.hz;
    if (t <= domain.t0 || t > domain.t1 + 1e-9) return null;
    const right = ((t - domain.t0) / span) * width;
    const w = Math.max(kMinOutlinePx, width / (span * lane.hz));
    const left = Math.max(0, right - w);
    return { left, width: Math.min(w, width - left) };
}

/**
 * Null when the selection is still valid; otherwise the reason it is stale. A momentarily empty lane is
 * not stale (the next refresh may bring data back).
 */
export function reconcileSelection(selected: SelectedFrame | null, lanes: Lane[]): string | null {
    if (!selected) return null;
    const lane = lanes.find((l) => l.unit === selected.unit);
    if (!lane) return `unit ${selected.unit} is no longer reported`;
    if (lane.count > 0 && selected.index < lane.index[0]) return 'frame is no longer retained';
    return null;
}
