// Pure lane model for the perf timeline. No React or DOM imports.
//
// Rules (also covered by tests):
//  - Time axis is seconds: t = absoluteIndex / hz. Never frame-aligned across units.
//  - Entries are positional: entry i of a reply has absolute index firstEntry + i. They are never
//    reordered or merged; duplicate units in one reply give separate lanes; each reply builds a fresh model.
//  - Values are 0.01 ms fixed point. 65535 or flag 0x01 = clamped (value is a lower bound, >= 655.35 ms).
//    Flag 0x02 = gap filler: no data, all values forced to 0, excluded from max/mean in buckets.
//  - unattributedMs = max(0, tick - sum(modules)); overAttributedMs = max(0, sum - tick).
//    Overlap rule: when modules overlap (nested or parallel scopes) the sum exceeds the tick; module bars
//    are NOT scaled, unattributed is 0 and the excess is exposed as overAttributedMs.
//    Always: tick = sum + unattributed - overAttributed.
//  - Windows are (anchor - seconds, anchor]. Anchor = newest time across ALL units unless paused.

import { decodeU16, decodeU8 } from './decode';

// ---------- wire types ----------

export interface PerfWindowModule { crc: number; name: string }

export interface PerfWindowUnit {
    unit: string;
    unitCrc: number;
    hz: number;
    firstEntry: number;
    count: number;
    modules: PerfWindowModule[];
    unitMs: string;
    moduleMs: string;
    /** Omitted by the packer when no flag is set. */
    flags?: string;
}

export interface PerfWindowReply {
    v: number;
    seconds: number;
    units: PerfWindowUnit[];
    markers: unknown[];
    trips: unknown[];
    truncated: boolean;
}

/**
 * Trip input. The wire `trips` record is not final, so callers pass ranges of absolute entry indexes
 * of one unit (both ends inclusive). Spans cover [firstEntry / hz, (lastEntry + 1) / hz].
 */
export interface TripRange { unit: string; firstEntry: number; lastEntry: number }

export interface TripSpan { t0: number; t1: number; firstEntry: number; lastEntry: number }

// ---------- model types ----------

/** Number of distinct module colours; UI maps colourIndex into its palette. */
export const kModuleColourCount = 12;

export interface LaneModule { crc: number; name: string; colourIndex: number }

/** Struct-of-arrays lane. `moduleMs` is module-major: module k, entry i at k * count + i. */
export interface Lane {
    unit: string;
    unitCrc: number;
    hz: number;
    count: number;
    modules: LaneModule[];
    budgetMs: number;
    /** Largest tick in the lane (non-gap entries), 0 when empty. */
    maxMs: number;
    /** Absolute entry index per entry. */
    index: Float64Array;
    /** Seconds per entry (index / hz). */
    t: Float64Array;
    tickMs: Float32Array;
    moduleMs: Float32Array;
    unattributedMs: Float32Array;
    overAttributedMs: Float32Array;
    clamped: Uint8Array;
    gap: Uint8Array;
}

export interface BuildOptions {
    /** Per-unit budget override by unit name. Default is 1000 / hz. */
    budgets?: Record<string, number>;
}

export interface ViewState { windowSeconds: 10 | 30 | 60; pausedAt: number | null }

export interface TimeDomain { t0: number; t1: number }

export interface ViewResult {
    lanes: Lane[];
    anchorT: number;
    domain: TimeDomain;
    paused: boolean;
    /** Paused at a time older than all retained data. */
    outOfRange: boolean;
}

/** Struct-of-arrays pixel buckets. `moduleMean` is module-major: module k, column c at k * columns + c. */
export interface BucketedLane {
    columns: number;
    tickMax: Float32Array;
    moduleMean: Float32Array;
    overBudget: Uint8Array;
    /** Column has entries and all of them are gap fillers. */
    hasGap: Uint8Array;
    hasTrip: Uint8Array;
    hasClamped: Uint8Array;
    /** Absolute entry indexes spanned by the column; -1 when the column has no entries. */
    firstIndex: Float64Array;
    lastIndex: Float64Array;
}

// ---------- helpers ----------

const kEps = 1e-9;
const kClampedRaw = 65535;
const kFlagClamped = 0x01;
const kFlagGap = 0x02;

function colourOf(crc: number): number {
    const h = Math.imul(crc >>> 0, 0x9e3779b1) >>> 0;
    return (h >>> 16) % kModuleColourCount;
}

function emptyLane(unit: string, unitCrc: number, hz: number, modules: LaneModule[], budgetMs: number): Lane {
    return {
        unit, unitCrc, hz, count: 0, modules, budgetMs, maxMs: 0,
        index: new Float64Array(0), t: new Float64Array(0), tickMs: new Float32Array(0),
        moduleMs: new Float32Array(0), unattributedMs: new Float32Array(0), overAttributedMs: new Float32Array(0),
        clamped: new Uint8Array(0), gap: new Uint8Array(0),
    };
}

function maxTick(tick: Float32Array, gap: Uint8Array): number {
    let m = 0;
    for (let i = 0; i < tick.length; i++) if (!gap[i] && tick[i] > m) m = tick[i];
    return m;
}

function buildLane(u: PerfWindowUnit, options: BuildOptions): Lane {
    const hz = Number(u.hz);
    const mods = Array.isArray(u.modules) ? u.modules : [];
    const modules: LaneModule[] = mods.map((m) => ({ crc: m.crc, name: m.name, colourIndex: colourOf(m.crc) }));
    const validHz = Number.isFinite(hz) && hz > 0;
    const override = options.budgets?.[u.unit];
    const budgetMs = override !== undefined ? override : validHz ? 1000 / hz : 0;
    const unitRaw = decodeU16(u.unitMs);
    const wireCount = Number.isFinite(u.count) && u.count > 0 ? Math.floor(u.count) : 0;
    const n = validHz ? Math.min(wireCount, unitRaw.length) : 0;
    if (n === 0) return emptyLane(u.unit, u.unitCrc, hz, modules, budgetMs);

    const m = modules.length;
    const modRaw = decodeU16(u.moduleMs);
    const flagRaw = decodeU8(u.flags);
    // Module blocks are wireCount long. If the reply is inconsistent, fall back to the decoded unit length.
    const stride = modRaw.length === m * wireCount ? wireCount : unitRaw.length;
    const first = Number.isFinite(u.firstEntry) && u.firstEntry > 0 ? Math.floor(u.firstEntry) : 0;

    const lane = emptyLane(u.unit, u.unitCrc, hz, modules, budgetMs);
    lane.count = n;
    lane.index = new Float64Array(n);
    lane.t = new Float64Array(n);
    lane.tickMs = new Float32Array(n);
    lane.moduleMs = new Float32Array(n * m);
    lane.unattributedMs = new Float32Array(n);
    lane.overAttributedMs = new Float32Array(n);
    lane.clamped = new Uint8Array(n);
    lane.gap = new Uint8Array(n);

    for (let i = 0; i < n; i++) {
        const flags = i < flagRaw.length ? flagRaw[i] : 0;
        const isGap = (flags & kFlagGap) !== 0;
        let isClamped = (flags & kFlagClamped) !== 0 || unitRaw[i] === kClampedRaw;
        lane.index[i] = first + i;
        lane.t[i] = (first + i) / hz;
        lane.gap[i] = isGap ? 1 : 0;
        if (isGap) continue; // values stay 0
        const tick = unitRaw[i] / 100;
        let sum = 0;
        for (let k = 0; k < m; k++) {
            const j = k * stride + i;
            const raw = j < modRaw.length ? modRaw[j] : 0;
            if (raw === kClampedRaw) isClamped = true;
            const v = raw / 100;
            lane.moduleMs[k * n + i] = v;
            sum += v;
        }
        lane.tickMs[i] = tick;
        lane.unattributedMs[i] = Math.max(0, tick - sum);
        lane.overAttributedMs[i] = Math.max(0, sum - tick);
        lane.clamped[i] = isClamped ? 1 : 0;
    }
    lane.maxMs = maxTick(lane.tickMs, lane.gap);
    return lane;
}

/** Takes the inner `perf.window` object out of the bridge envelope; null when not a usable v1 reply. */
export function unwrapWindowReply(reply: unknown): PerfWindowReply | null {
    if (!reply || typeof reply !== 'object') return null;
    const r = reply as { success?: unknown; result?: unknown };
    if (r.success !== true || !r.result || typeof r.result !== 'object') return null;
    const w = r.result as Partial<PerfWindowReply>;
    return w.v === 1 && Array.isArray(w.units) ? (w as PerfWindowReply) : null;
}

/** One lane per unit, in reply order. */
export function buildLanes(window: PerfWindowReply, options: BuildOptions = {}): Lane[] {
    if (!window || !Array.isArray(window.units)) return [];
    return window.units.map((u) => buildLane(u, options));
}

// ---------- windowing ----------

/** First position with t > lo (t ascending). */
function firstAfter(t: Float64Array, lo: number): number {
    let a = 0;
    let b = t.length;
    while (a < b) {
        const mid = (a + b) >> 1;
        if (t[mid] > lo) b = mid;
        else a = mid + 1;
    }
    return a;
}

function sliceLane(lane: Lane, from: number, to: number): Lane {
    const n = Math.max(0, to - from);
    if (n === lane.count) return lane;
    const m = lane.modules.length;
    const out: Lane = { ...lane, count: n };
    out.index = lane.index.slice(from, from + n);
    out.t = lane.t.slice(from, from + n);
    out.tickMs = lane.tickMs.slice(from, from + n);
    out.unattributedMs = lane.unattributedMs.slice(from, from + n);
    out.overAttributedMs = lane.overAttributedMs.slice(from, from + n);
    out.clamped = lane.clamped.slice(from, from + n);
    out.gap = lane.gap.slice(from, from + n);
    out.moduleMs = new Float32Array(n * m);
    for (let k = 0; k < m; k++) {
        out.moduleMs.set(lane.moduleMs.subarray(k * lane.count + from, k * lane.count + from + n), k * n);
    }
    out.maxMs = maxTick(out.tickMs, out.gap);
    return out;
}

/** Newest entry time across all lanes, or null when there is no data. */
export function newestTime(lanes: Lane[]): number | null {
    let best: number | null = null;
    for (const l of lanes) if (l.count > 0 && (best === null || l.t[l.count - 1] > best)) best = l.t[l.count - 1];
    return best;
}

function oldestTime(lanes: Lane[]): number | null {
    let best: number | null = null;
    for (const l of lanes) if (l.count > 0 && (best === null || l.t[0] < best)) best = l.t[0];
    return best;
}

/** Keeps entries with t in (anchorT - seconds, anchorT]. Default anchor: newest time across all lanes. */
export function sliceWindow(lanes: Lane[], seconds: number, anchorT?: number): Lane[] {
    const anchor = anchorT ?? newestTime(lanes);
    if (anchor === null) return lanes.map((l) => sliceLane(l, 0, 0));
    const lo = anchor - seconds + kEps;
    const hi = anchor + kEps;
    return lanes.map((l) => sliceLane(l, firstAfter(l.t, lo), firstAfter(l.t, hi)));
}

/** The visible slice for a view state. While paused, anchor = pausedAt and newer data cannot move it. */
export function applyViewState(lanes: Lane[], view: ViewState): ViewResult {
    const paused = view.pausedAt !== null;
    const newest = newestTime(lanes);
    const anchorT = view.pausedAt ?? newest ?? 0;
    const oldest = oldestTime(lanes);
    const outOfRange = paused && oldest !== null && anchorT < oldest;
    return {
        lanes: sliceWindow(lanes, view.windowSeconds, anchorT),
        anchorT,
        domain: { t0: anchorT - view.windowSeconds, t1: anchorT },
        paused,
        outOfRange,
    };
}

// ---------- bucketing, trips, lookup ----------

function inTrip(ranges: TripRange[], index: number): boolean {
    for (const r of ranges) if (index >= r.firstEntry && index <= r.lastEntry) return true;
    return false;
}

/**
 * Reduces a lane to `columns` pixel columns over (domain.t0, domain.t1]. Tick uses MAX (spikes stay
 * visible); modules use the mean of the non-gap entries in the column; a column with only gap
 * entries is a gap; a column with no entries has firstIndex -1.
 */
export function bucketize(lane: Lane, columns: number, domain: TimeDomain, trips: TripRange[] = []): BucketedLane {
    const cols = Number.isFinite(columns) && columns > 0 ? Math.floor(columns) : 0;
    const m = lane.modules.length;
    const out: BucketedLane = {
        columns: cols,
        tickMax: new Float32Array(cols),
        moduleMean: new Float32Array(cols * m),
        overBudget: new Uint8Array(cols),
        hasGap: new Uint8Array(cols),
        hasTrip: new Uint8Array(cols),
        hasClamped: new Uint8Array(cols),
        firstIndex: new Float64Array(cols).fill(-1),
        lastIndex: new Float64Array(cols).fill(-1),
    };
    const span = domain.t1 - domain.t0;
    if (cols === 0 || !(span > 0)) return out;
    const width = span / cols;
    const ranges = trips.filter((r) => r.unit === lane.unit);
    const real = new Uint32Array(cols);
    const total = new Uint32Array(cols);
    const sums = new Float64Array(cols * m);

    for (let i = 0; i < lane.count; i++) {
        const t = lane.t[i];
        if (t <= domain.t0 + kEps || t > domain.t1 + kEps) continue;
        const c = Math.min(cols - 1, Math.max(0, Math.ceil((t - domain.t0) / width - kEps) - 1));
        if (total[c] === 0) out.firstIndex[c] = lane.index[i];
        out.lastIndex[c] = lane.index[i];
        total[c]++;
        if (ranges.length > 0 && inTrip(ranges, lane.index[i])) out.hasTrip[c] = 1;
        if (lane.gap[i]) continue;
        real[c]++;
        if (lane.tickMs[i] > out.tickMax[c]) out.tickMax[c] = lane.tickMs[i];
        if (lane.clamped[i]) out.hasClamped[c] = 1;
        for (let k = 0; k < m; k++) sums[k * cols + c] += lane.moduleMs[k * lane.count + i];
    }
    for (let c = 0; c < cols; c++) {
        out.hasGap[c] = total[c] > 0 && real[c] === 0 ? 1 : 0;
        out.overBudget[c] = out.tickMax[c] > lane.budgetMs ? 1 : 0;
        if (real[c] > 0) for (let k = 0; k < m; k++) out.moduleMean[k * cols + c] = sums[k * cols + c] / real[c];
    }
    return out;
}

/**
 * Trip shading spans in seconds for this lane's unit, covering [firstEntry / hz, (lastEntry + 1) / hz]
 * and clipped to `domain` (default: the lane's own extent). Spans left empty by clipping are dropped.
 */
export function tripSpans(lane: Lane, trips: TripRange[], domain?: TimeDomain): TripSpan[] {
    if (lane.count === 0 || !(lane.hz > 0)) return [];
    const lo = domain ? domain.t0 : lane.t[0];
    const hi = domain ? domain.t1 : lane.t[lane.count - 1] + 1 / lane.hz;
    const out: TripSpan[] = [];
    for (const r of trips) {
        if (r.unit !== lane.unit || r.lastEntry < r.firstEntry) continue;
        const t0 = Math.max(lo, r.firstEntry / lane.hz);
        const t1 = Math.min(hi, (r.lastEntry + 1) / lane.hz);
        if (t1 > t0) out.push({ t0, t1, firstEntry: r.firstEntry, lastEntry: r.lastEntry });
    }
    return out;
}

/**
 * Absolute entry index nearest to t (gap entries included), or null when the lane is empty, t is not
 * finite, or t lies more than one entry period outside the lane's ends.
 */
export function findFrameAt(lane: Lane, t: number): number | null {
    if (lane.count === 0 || !Number.isFinite(t)) return null;
    const period = 1 / lane.hz;
    if (t < lane.t[0] - period - kEps || t > lane.t[lane.count - 1] + period + kEps) return null;
    const pos = Math.round(t * lane.hz) - lane.index[0];
    return lane.index[Math.min(lane.count - 1, Math.max(0, pos))];
}
