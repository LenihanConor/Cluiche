// Pure jobs / workers model for the Workers tab. No React or DOM imports.
//
// Three data tiers (spec "Jobs and workers"):
//  - tier 1 (perf.timing `jobs`): per-owner aggregates and pool counters -> poolHealth
//  - tier 2 (perf.window `jobs`): per-owner 200 ms buckets (owner-major u16 arrays) and deferred-step summaries
//    -> heat strip, owner share, step rows, late results
//  - tier 3 (perf.jobs): raw per-job records -> worker slices, ranges per worker, step share
//
// Time bases. The heat strip uses the lane axis: t = frame / hz seconds (bucket b spans frames
// firstFrame + b*framesPerBucket ...), so it lines up with the Frames lanes. Step summaries and raw records are on the
// steady clock in ns; the wire carries no mapping between the two, so they are NOT aligned with each other.

import { decodeU16, decodeU32, decodeU64 } from './decode';
import { colourOf } from './laneModel';
import type { TimeDomain } from './laneModel';
import type { JobsTier1Unit, StepSummary } from '../types';

// ---------- tier 2: window ----------

export interface JobOwner { crc: number; name: string; colourIndex: number }

export interface JobsUnit {
    unit: string;
    unitCrc: number;
    perJobRecording: boolean;
    hz: number;
    firstFrame: number;
    framesPerBucket: number;
    bucketCount: number;
    owners: JobOwner[];
    /** Owner-major (owner * bucketCount + bucket). */
    count: Uint16Array;
    /** ms (the wire is 0.01 ms fixed point, saturating at 655.35). */
    busyMs: Float32Array;
    worstWaitMs: Float32Array;
}

export interface JobsWindow {
    bucketMs: number;
    perJobRecording: boolean;
    units: JobsUnit[];
    steps: StepSummary[];
}

const isObj = (v: unknown): v is Record<string, unknown> => !!v && typeof v === 'object' && !Array.isArray(v);
const num = (v: unknown, d = 0): number => (typeof v === 'number' && Number.isFinite(v) ? v : d);

function toFixedMs(a: Uint16Array): Float32Array {
    const out = new Float32Array(a.length);
    for (let i = 0; i < a.length; i++) out[i] = a[i] / 100;
    return out;
}

function parseUnit(u: Record<string, unknown>): JobsUnit | null {
    if (typeof u.unit !== 'string') return null;
    const owners = (Array.isArray(u.owners) ? u.owners : [])
        .filter((o): o is Record<string, unknown> => isObj(o))
        .map((o) => ({ crc: num(o.crc), name: typeof o.name === 'string' ? o.name : `0x${num(o.crc).toString(16).padStart(8, '0')}`, colourIndex: colourOf(num(o.crc)) }));
    const agg = isObj(u.frameAgg) ? u.frameAgg : {};
    return {
        unit: u.unit, unitCrc: num(u.unitCrc), perJobRecording: u.perJobRecording === true,
        hz: num(u.hz, 60) > 0 ? num(u.hz, 60) : 60, firstFrame: num(u.firstFrame),
        framesPerBucket: Math.max(1, num(u.framesPerBucket, 1)), bucketCount: Math.max(0, num(u.bucketCount)),
        owners,
        count: decodeU16(agg.count as string | undefined),
        busyMs: toFixedMs(decodeU16(agg.busyMs as string | undefined)),
        worstWaitMs: toFixedMs(decodeU16(agg.worstWaitMs as string | undefined)),
    };
}

function parseStep(s: unknown): StepSummary | null {
    if (!isObj(s) || typeof s.channel !== 'string') return null;
    const need = ['kickKey', 'applyKey', 'kick', 'firstStart', 'ready', 'apply', 'stallMs', 'cpuMs', 'capWaitMs', 'workerWaitMs', 'ranges'] as const;
    for (const k of need) if (typeof s[k] !== 'number' || !Number.isFinite(s[k] as number)) return null;
    const out: StepSummary = {
        channel: s.channel,
        kickKey: s.kickKey as number, applyKey: s.applyKey as number, kick: s.kick as number, firstStart: s.firstStart as number,
        ready: s.ready as number, apply: s.apply as number, stallMs: s.stallMs as number, cpuMs: s.cpuMs as number,
        capWaitMs: s.capWaitMs as number, workerWaitMs: s.workerWaitMs as number, ranges: s.ranges as number,
    };
    if (typeof s.unit === 'string') out.unit = s.unit;
    if (typeof s.captureMs === 'number') out.captureMs = s.captureMs;
    if (typeof s.applyMs === 'number') out.applyMs = s.applyMs;
    return out;
}

/** Reads the `jobs` key of a perf.window reply; null when absent or not an object with a units array. */
export function parseJobsWindow(raw: unknown): JobsWindow | null {
    if (!isObj(raw) || !Array.isArray(raw.units)) return null;
    const units = raw.units.filter(isObj).map(parseUnit).filter((u): u is JobsUnit => u !== null);
    const steps = (Array.isArray(raw.steps) ? raw.steps : []).map(parseStep).filter((s): s is StepSummary => s !== null);
    return { bucketMs: num(raw.bucketMs, 200), perJobRecording: raw.perJobRecording !== false, units, steps };
}

/** Lane-axis domain for the heat strip when there are no lanes: ends at the newest bucket end. */
export function domainOf(units: JobsUnit[], seconds: number): TimeDomain {
    let t1 = -Infinity;
    for (const u of units) {
        if (u.bucketCount > 0) t1 = Math.max(t1, (u.firstFrame + u.bucketCount * u.framesPerBucket) / u.hz);
    }
    return Number.isFinite(t1) ? { t0: t1 - seconds, t1 } : { t0: 0, t1: seconds };
}

// ---------- heat strip ----------

const kEps = 1e-9;

export interface HeatSegment { owner: number; crc: number; colourIndex: number; busyMs: number }
export interface HeatBucket { t0: number; t1: number; totalMs: number; segments: HeatSegment[] }
export interface HeatStrip { unit: string; buckets: HeatBucket[]; maxTotalMs: number; owners: JobOwner[] }

/** One stacked bar per 200 ms bucket (owner order, zero cells skipped), clipped to buckets overlapping the domain. */
export function buildHeatStrip(u: JobsUnit, domain: TimeDomain): HeatStrip {
    const buckets: HeatBucket[] = [];
    let maxTotalMs = 0;
    if (u.perJobRecording) {
        for (let b = 0; b < u.bucketCount; b++) {
            const t0 = (u.firstFrame + b * u.framesPerBucket) / u.hz;
            const t1 = (u.firstFrame + (b + 1) * u.framesPerBucket) / u.hz;
            if (!(t1 > domain.t0 + kEps && t0 < domain.t1 - kEps)) continue;
            const segments: HeatSegment[] = [];
            let total = 0;
            for (let o = 0; o < u.owners.length; o++) {
                const ms = u.busyMs[o * u.bucketCount + b] ?? 0;
                if (ms > 0) {
                    segments.push({ owner: o, crc: u.owners[o].crc, colourIndex: u.owners[o].colourIndex, busyMs: ms });
                    total += ms;
                }
            }
            buckets.push({ t0, t1, totalMs: total, segments });
            if (total > maxTotalMs) maxTotalMs = total;
        }
    }
    return { unit: u.unit, buckets, maxTotalMs, owners: u.owners };
}

// ---------- owner share ----------

export interface OwnerShareRow { crc: number; name: string; colourIndex: number; busyMs: number; share: number; pct: number }
export interface OwnerShare { totalBusyMs: number; rows: OwnerShareRow[] }

/** Largest-remainder rounding so the integer percentages sum to exactly 100. */
function finishShare(acc: Map<number, { name: string; colourIndex: number; busyMs: number }>): OwnerShare {
    const entries = [...acc.entries()].filter(([, v]) => v.busyMs > 0).sort((a, b) => b[1].busyMs - a[1].busyMs);
    const total = entries.reduce((s, [, v]) => s + v.busyMs, 0);
    if (!(total > 0)) return { totalBusyMs: 0, rows: [] };
    const exact = entries.map(([, v]) => (v.busyMs / total) * 100);
    const pct = exact.map(Math.floor);
    let left = 100 - pct.reduce((a, b) => a + b, 0);
    const order = exact.map((e, i) => ({ i, frac: e - Math.floor(e) })).sort((a, b) => b.frac - a.frac || a.i - b.i);
    for (let k = 0; left > 0 && k < order.length; k++, left--) pct[order[k].i]++;
    const rows = entries.map(([crc, v], i) => ({ crc, name: v.name, colourIndex: v.colourIndex, busyMs: v.busyMs, share: v.busyMs / total, pct: pct[i] }));
    return { totalBusyMs: total, rows };
}

/**
 * Worker busy time per owner over `window` (lane-axis seconds), merged across units by owner crc. A bucket only
 * partly inside the window counts proportionally. Shares sum to 1 and integer percentages to 100.
 */
export function buildOwnerShare(units: JobsUnit[], window: TimeDomain): OwnerShare {
    const acc = new Map<number, { name: string; colourIndex: number; busyMs: number }>();
    for (const u of units) {
        if (!u.perJobRecording) continue;
        for (let b = 0; b < u.bucketCount; b++) {
            const t0 = (u.firstFrame + b * u.framesPerBucket) / u.hz;
            const t1 = (u.firstFrame + (b + 1) * u.framesPerBucket) / u.hz;
            const overlap = Math.min(t1, window.t1) - Math.max(t0, window.t0);
            if (!(overlap > kEps)) continue;
            const f = Math.min(1, overlap / (t1 - t0));
            for (let o = 0; o < u.owners.length; o++) {
                const ms = u.busyMs[o * u.bucketCount + b] ?? 0;
                if (!(ms > 0)) continue;
                const cur = acc.get(u.owners[o].crc);
                if (cur) cur.busyMs += ms * f;
                else acc.set(u.owners[o].crc, { name: u.owners[o].name, colourIndex: u.owners[o].colourIndex, busyMs: ms * f });
            }
        }
    }
    return finishShare(acc);
}

// ---------- tier 3: raw records ----------

export interface RawSlice {
    startNs: number;
    endNs: number;
    durNs: number;
    owner: number;
    ownerName: string;
    colourIndex: number;
    tag: number;
    waitMs: number;
    worker: number;
    range: number;
    rangeCount: number;
    flags: number;
}

export interface RawJobs {
    unit: string;
    perJobRecording: boolean;
    truncated: boolean;
    slices: RawSlice[];
}

/** Decodes a perf.jobs reply body; null for an error reply or anything that is not an object. */
export function parseRawJobs(result: unknown, ownerNames: ReadonlyMap<number, string>): RawJobs | null {
    if (!isObj(result) || result.error !== undefined) return null;
    const start = decodeU64(result.startNs as string | undefined);
    const dur = decodeU32(result.durNs as string | undefined);
    const owner = decodeU32(result.owner as string | undefined);
    const tag = decodeU32(result.tag as string | undefined);
    const waitUs = decodeU32(result.waitUs as string | undefined);
    const worker = decodeU16(result.worker as string | undefined);
    const range = decodeU16(result.range as string | undefined);
    const rangeCount = decodeU16(result.rangeCount as string | undefined);
    const flags = decodeU16(result.flags as string | undefined);
    let n = Math.min(start.length, dur.length, owner.length, tag.length, waitUs.length, worker.length, range.length, rangeCount.length, flags.length);
    if (typeof result.count === 'number') n = Math.min(n, result.count);
    const slices: RawSlice[] = [];
    for (let i = 0; i < n; i++) {
        slices.push({
            startNs: start[i], endNs: start[i] + dur[i], durNs: dur[i], owner: owner[i],
            ownerName: ownerNames.get(owner[i]) ?? `0x${owner[i].toString(16).padStart(8, '0')}`,
            colourIndex: colourOf(owner[i]), tag: tag[i], waitMs: waitUs[i] / 1000, worker: worker[i],
            range: range[i], rangeCount: rangeCount[i], flags: flags[i],
        });
    }
    return {
        unit: typeof result.unit === 'string' ? result.unit : '',
        perJobRecording: result.perJobRecording !== false,
        truncated: result.truncated === true,
        slices,
    };
}

export interface WorkerRow { worker: number; slices: RawSlice[] }

/** Worker rows sorted by worker index, slices by start time. */
export function rowsByWorker(raw: RawJobs): WorkerRow[] {
    const m = new Map<number, RawSlice[]>();
    for (const s of raw.slices) {
        const a = m.get(s.worker);
        if (a) a.push(s); else m.set(s.worker, [s]);
    }
    return [...m.entries()].sort((a, b) => a[0] - b[0]).map(([worker, slices]) => ({ worker, slices: slices.sort((a, b) => a.startNs - b.startNs) }));
}

// ---------- deferred steps ----------

export function stepKey(s: StepSummary): string { return `${s.channel}:${s.kickKey}`; }

/**
 * Raw records belonging to a step. PROVISIONAL: matched by owner name == channel and start inside the run span, because the
 * wire carries no step id on raw records yet.
 */
export function stepSlices(raw: RawJobs, step: StepSummary): RawSlice[] {
    return raw.slices.filter((s) => s.ownerName === step.channel && s.startNs >= step.firstStart && s.startNs <= step.ready);
}

export interface WaitSplit { capMs: number; workerMs: number; totalMs: number; capShare: number }

/** Queue wait split into "waiting for a cap slot" and "waiting for a free worker", straight from the summary (AC11). */
export function waitSplit(step: StepSummary): WaitSplit {
    const totalMs = step.capWaitMs + step.workerWaitMs;
    return { capMs: step.capWaitMs, workerMs: step.workerWaitMs, totalMs, capShare: totalMs > 0 ? step.capWaitMs / totalMs : 0 };
}

/** Worker time per owner while the step ran (first start -> ready), from raw records. */
export function buildStepShare(raw: RawJobs, step: StepSummary): OwnerShare {
    const acc = new Map<number, { name: string; colourIndex: number; busyMs: number }>();
    for (const s of raw.slices) {
        const d = Math.min(s.endNs, step.ready) - Math.max(s.startNs, step.firstStart);
        if (!(d > 0)) continue;
        const cur = acc.get(s.owner);
        if (cur) cur.busyMs += d / 1e6;
        else acc.set(s.owner, { name: s.ownerName, colourIndex: s.colourIndex, busyMs: d / 1e6 });
    }
    return finishShare(acc);
}

export interface WorkerRanges { worker: number; ranges: number; busyMs: number }

export function rangesPerWorker(raw: RawJobs | null, step: StepSummary): WorkerRanges[] {
    if (!raw) return [];
    const m = new Map<number, WorkerRanges>();
    for (const s of stepSlices(raw, step)) {
        const cur = m.get(s.worker) ?? { worker: s.worker, ranges: 0, busyMs: 0 };
        cur.ranges++;
        cur.busyMs += s.durNs / 1e6;
        m.set(s.worker, cur);
    }
    return [...m.values()].sort((a, b) => a.worker - b.worker);
}

export interface StepCost {
    /** Capture + Apply + stall; null when the summary has no captureMs / applyMs. */
    criticalPathMs: number | null;
    stallMs: number;
    runSpanMs: number;
    parallelism: number;
    /** (ready - kick) / (apply - kick) in percent; over 100 means late. */
    windowUsedPct: number;
    /** max range duration / mean range duration; null without raw records. */
    imbalance: number | null;
}

export function stepCost(step: StepSummary, raw: RawJobs | null): StepCost {
    const runSpanMs = Math.max(0, (step.ready - step.firstStart) / 1e6);
    const window = step.apply - step.kick;
    let imbalance: number | null = null;
    if (raw) {
        const durs = stepSlices(raw, step).map((s) => s.durNs);
        if (durs.length > 0) {
            const mean = durs.reduce((a, b) => a + b, 0) / durs.length;
            imbalance = mean > 0 ? Math.max(...durs) / mean : null;
        }
    }
    return {
        criticalPathMs: step.captureMs !== undefined && step.applyMs !== undefined ? step.captureMs + step.applyMs + step.stallMs : null,
        stallMs: step.stallMs,
        runSpanMs,
        parallelism: runSpanMs > 0 ? step.cpuMs / runSpanMs : 0,
        windowUsedPct: window > 0 ? ((step.ready - step.kick) / window) * 100 : 0,
        imbalance,
    };
}

export interface StepBar { key: string; kick: number; firstStart: number; ready: number; apply: number; late: boolean }
export interface StepChannel { channel: string; bars: StepBar[] }
export interface StepRows { domain: TimeDomain; channels: StepChannel[] }

/** One row per channel in first-seen order; ns domain from the earliest kick to the latest apply / ready. */
export function buildStepBars(steps: StepSummary[]): StepRows {
    const channels: StepChannel[] = [];
    let t0 = Infinity;
    let t1 = -Infinity;
    for (const s of steps) {
        let ch = channels.find((c) => c.channel === s.channel);
        if (!ch) { ch = { channel: s.channel, bars: [] }; channels.push(ch); }
        ch.bars.push({ key: stepKey(s), kick: s.kick, firstStart: s.firstStart, ready: s.ready, apply: s.apply, late: s.stallMs > 0 });
        t0 = Math.min(t0, s.kick);
        t1 = Math.max(t1, s.apply, s.ready);
    }
    for (const c of channels) c.bars.sort((a, b) => a.kick - b.kick);
    return { domain: channels.length > 0 ? { t0, t1 } : { t0: 0, t1: 1 }, channels };
}

/** Steps whose result was not ready at its apply key, newest apply first. */
export function lateResults(steps: StepSummary[]): StepSummary[] {
    return steps.filter((s) => s.stallMs > 0).sort((a, b) => b.apply - a.apply);
}

// ---------- pool health (tier 1) ----------

export interface PoolMetric { available: boolean; value: number | null }
export interface OwnerWait { name: string; crc: number; colourIndex: number; count: number; busyMs: number; waitP50Ms: number; waitP95Ms: number }
export interface PoolHealth {
    perOwner: OwnerWait[];
    capUse: PoolMetric;
    timeAtCapMs: PoolMetric;
    peakQueueDepth: PoolMetric;
    inlineFallbacks: PoolMetric;
    inboxDrops: PoolMetric;
}

const kUnavailableKey = { capUse: 'cap_use', timeAtCapMs: 'time_at_cap', peakQueueDepth: 'peak_queue_depth' } as const;

/** Fields the game lists in `unavailable` are reported as unavailable (value null), never as a fake 0. */
export function poolHealth(t1: JobsTier1Unit | null, unavailable: readonly string[]): PoolHealth {
    const metric = (v: number | undefined, key?: string): PoolMetric =>
        t1 === null || v === undefined || (key !== undefined && unavailable.includes(key)) ? { available: false, value: null } : { available: true, value: v };
    return {
        perOwner: (t1?.owners ?? [])
            .map((o) => ({ name: o.name, crc: o.crc, colourIndex: colourOf(o.crc), count: o.count, busyMs: o.busyMs, waitP50Ms: o.waitP50Ms, waitP95Ms: o.waitP95Ms }))
            .sort((a, b) => b.waitP95Ms - a.waitP95Ms),
        capUse: metric(t1?.pool.capUse, kUnavailableKey.capUse),
        timeAtCapMs: metric(t1?.pool.timeAtCapMs, kUnavailableKey.timeAtCapMs),
        peakQueueDepth: metric(t1?.pool.peakQueueDepth, kUnavailableKey.peakQueueDepth),
        inlineFallbacks: metric(t1?.pool.inlineFallbacks),
        inboxDrops: metric(t1?.pool.inboxDrops),
    };
}
