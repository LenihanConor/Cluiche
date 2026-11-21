// NOTE: __fixtures__/jobs.fixture.json is SYNTHETIC (see makeJobsFixture.ts). Its step summaries and raw records are
// fixture-only: a live game sends `steps: []` when no deferred step ran in the window.
import fixture from './__fixtures__/jobs.fixture.json';
import { encodeU16LE, encodeU64LE, encodeU32LE } from './__fixtures__/makeJobsFixture';
import { decodeU32, decodeU64 } from './decode';
import { colourOf } from './laneModel';
import {
    buildHeatStrip, buildOwnerShare, buildStepBars, buildStepShare, domainOf, lateResults, parseJobsWindow,
    parseRawJobs, poolHealth, rangesPerWorker, rowsByWorker, stepCost, stepKey, waitSplit,
} from './jobs';

const win = parseJobsWindow(fixture.window)!;
const sim = win.units[0];
const names = new Map(win.units.flatMap((u) => u.owners.map((o) => [o.crc, o.name] as [number, string])));
const raw = parseRawJobs(fixture.raw, names)!;

describe('decode u32 / u64', () => {
    it('round-trips little-endian values, u64 exact below 2^53', () => {
        expect(Array.from(decodeU32(encodeU32LE([0, 1, 4294967295])))).toEqual([0, 1, 4294967295]);
        const big = 5_004_002_000_123;
        expect(Array.from(decodeU64(encodeU64LE([0, big, 2 ** 40 + 7])))).toEqual([0, big, 2 ** 40 + 7]);
    });
    it('drops trailing partial words and tolerates empty or bad input', () => {
        expect(decodeU32('AQIDBAU=').length).toBe(1);
        expect(decodeU64('').length).toBe(0);
        expect(decodeU64('!!notbase64').length).toBe(0);
    });
});

describe('parseJobsWindow', () => {
    it('reads owner-major arrays: busy is 0.01 ms fixed point', () => {
        expect(sim.bucketCount).toBe(50);
        expect(sim.owners.map((o) => o.name)).toEqual(['weather', 'ecology', 'PathfindingModule', '(other)']);
        // weather owner (index 0), bucket 0 is "on": 150 ms
        expect(sim.busyMs[0]).toBeCloseTo(150, 5);
        // ecology owner (index 1), bucket 25 is on: 380 ms
        expect(sim.busyMs[1 * 50 + 25]).toBeCloseTo(380, 5);
        expect(sim.busyMs[1 * 50 + 5]).toBe(0);
        // saturated cell is 655.35 ms
        expect(sim.busyMs[2 * 50 + 49]).toBeCloseTo(655.35, 4);
        expect(sim.count[0]).toBe(4);
    });
    it('uses the same stable colour function as modules', () => {
        expect(sim.owners[1].colourIndex).toBe(colourOf(sim.owners[1].crc));
    });
    it('parses steps and drops malformed ones', () => {
        expect(win.steps.map((s) => s.channel)).toEqual(['weather', 'ecology', 'weather']);
        const w = parseJobsWindow({ bucketMs: 200, perJobRecording: true, units: [], steps: [{ channel: 'x' }, 5, null] })!;
        expect(w.steps).toEqual([]);
    });
    it('is empty and recording-off for a unit with no arrays; null for junk', () => {
        const main = win.units[1];
        expect(main.perJobRecording).toBe(false);
        expect(main.bucketCount).toBe(0);
        expect(buildHeatStrip(main, { t0: 0, t1: 10 }).buckets).toEqual([]);
        expect(parseJobsWindow(undefined)).toBeNull();
        expect(parseJobsWindow({})).toBeNull();
    });
    it('treats the live shape (steps: [], no jobs units) as valid and empty', () => {
        const w = parseJobsWindow({ bucketMs: 200, perJobRecording: true, units: [], steps: [] })!;
        expect(w.units).toEqual([]);
        expect(w.steps).toEqual([]);
    });
    it('tolerates arrays shorter than owners * buckets (reads 0)', () => {
        const w = parseJobsWindow({
            bucketMs: 200, perJobRecording: true, steps: [],
            units: [{ unit: 'U', unitCrc: 1, perJobRecording: true, hz: 30, firstFrame: 0, framesPerBucket: 6, bucketCount: 4,
                owners: [{ crc: 9, name: 'a' }, { crc: 10, name: 'b' }], frameAgg: { count: '', busyMs: encodeU16LE([100, 100]), worstWaitMs: '' } }],
        })!;
        expect(w.units[0].busyMs[7] ?? 0).toBe(0);
        expect(buildHeatStrip(w.units[0], { t0: 0, t1: 5 }).buckets).toHaveLength(4);
    });
});

describe('heat strip', () => {
    it('buckets are 200 ms wide and time-aligned with the lane axis (t = frame / hz)', () => {
        const strip = buildHeatStrip(sim, domainOf(win.units, 10));
        const b0 = strip.buckets[0];
        expect(b0.t0).toBeCloseTo(1500 / 30, 9);
        expect(b0.t1 - b0.t0).toBeCloseTo(0.2, 9);
        expect(strip.buckets).toHaveLength(50);
        expect(strip.buckets[49].t1).toBeCloseTo((1500 + 300) / 30, 9);
    });
    it('keeps only buckets that overlap the domain', () => {
        const d = { t0: 50 + 2.0, t1: 50 + 2.4 }; // 1500/30 = 50 s
        const strip = buildHeatStrip(sim, d);
        expect(strip.buckets.map((b) => Math.round((b.t0 - 50) * 10) / 10)).toEqual([2, 2.2]);
    });
    it('stacks owner segments in owner order, skipping zero cells, totals add up', () => {
        const strip = buildHeatStrip(sim, domainOf(win.units, 10));
        const b = strip.buckets[0]; // weather on, pathfinding 3 ms, other 0.25 ms
        expect(b.segments.map((s) => s.owner)).toEqual([0, 2, 3]);
        expect(b.totalMs).toBeCloseTo(150 + 3 + 0.25, 5);
        expect(strip.maxTotalMs).toBeGreaterThanOrEqual(655.35);
    });
});

describe('owner share (AC10)', () => {
    it('sums to 100 percent of worker busy time for the visible window', () => {
        const s = buildOwnerShare(win.units, domainOf(win.units, 10));
        expect(s.rows.reduce((a, r) => a + r.share, 0)).toBeCloseTo(1, 9);
        expect(s.rows.reduce((a, r) => a + r.pct, 0)).toBe(100);
        expect(s.rows[0].busyMs).toBeGreaterThanOrEqual(s.rows[1].busyMs);
    });
    it('window total equals the sum of all bucket busy ms inside the domain', () => {
        const s = buildOwnerShare(win.units, domainOf(win.units, 10));
        const all = Array.from(sim.busyMs).reduce((a, b) => a + b, 0);
        expect(s.totalBusyMs).toBeCloseTo(all, 3);
    });
    it('narrower windows only count the overlapping part of a bucket', () => {
        const half = buildOwnerShare(win.units, { t0: 50, t1: 50.1 }); // half of bucket 0
        expect(half.totalBusyMs).toBeCloseTo((150 + 3 + 0.25) / 2, 4);
        expect(half.rows.reduce((a, r) => a + r.pct, 0)).toBe(100);
    });
    it('gives no rows when there is no busy time', () => {
        const s = buildOwnerShare(win.units, { t0: 0, t1: 1 });
        expect(s.rows).toEqual([]);
        expect(s.totalBusyMs).toBe(0);
    });
    it('pct uses largest remainder so thirds still sum to 100', () => {
        const w = parseJobsWindow({
            bucketMs: 200, perJobRecording: true, steps: [],
            units: [{ unit: 'U', unitCrc: 1, perJobRecording: true, hz: 30, firstFrame: 0, framesPerBucket: 6, bucketCount: 1,
                owners: [{ crc: 1, name: 'a' }, { crc: 2, name: 'b' }, { crc: 3, name: 'c' }],
                frameAgg: { count: '', busyMs: encodeU16LE([100, 100, 100]), worstWaitMs: '' } }],
        })!;
        const s = buildOwnerShare(w.units, { t0: 0, t1: 1 });
        expect(s.rows.map((r) => r.pct).sort()).toEqual([33, 33, 34]);
    });
    it('merges the same owner across units', () => {
        const u = { ...sim, unit: 'Copy' };
        const s = buildOwnerShare([sim, u], domainOf(win.units, 10));
        expect(s.rows).toHaveLength(4);
        const one = buildOwnerShare([sim], domainOf(win.units, 10));
        expect(s.totalBusyMs).toBeCloseTo(one.totalBusyMs * 2, 3);
    });
});

describe('raw jobs (tier 3)', () => {
    it('decodes every packed field', () => {
        expect(raw.slices.length).toBe(17);
        expect(raw.truncated).toBe(false);
        expect(raw.perJobRecording).toBe(true);
        const s = raw.slices[0];
        expect(s.endNs).toBe(s.startNs + s.durNs);
        expect(s.ownerName).toBe('weather');
        expect(s.colourIndex).toBe(colourOf(s.owner));
    });
    it('reports a truncated reply and perJobRecording false', () => {
        const t = parseRawJobs({ ...fixture.raw, truncated: true }, names)!;
        expect(t.truncated).toBe(true);
        const off = parseRawJobs({ v: 1, unit: 'Sim', perJobRecording: false, truncated: false, count: 0 }, names)!;
        expect(off.perJobRecording).toBe(false);
        expect(off.slices).toEqual([]);
    });
    it('returns null for an error reply or junk, and clamps count to the shortest array', () => {
        expect(parseRawJobs({ error: 'unknown unit' }, names)).toBeNull();
        expect(parseRawJobs(null, names)).toBeNull();
        const short = parseRawJobs({ ...fixture.raw, worker: encodeU16LE([1, 2]) }, names)!;
        expect(short.slices).toHaveLength(2);
    });
    it('names unknown owners by hex crc', () => {
        const r = parseRawJobs(fixture.raw, new Map())!;
        expect(r.slices[0].ownerName).toBe('0x5ea71001');
    });
    it('groups into worker rows sorted by worker index', () => {
        const rows = rowsByWorker(raw);
        expect(rows.map((r) => r.worker)).toEqual([0, 1, 2, 3, 4, 5, 6]);
        for (const r of rows) for (let i = 1; i < r.slices.length; i++) expect(r.slices[i].startNs).toBeGreaterThanOrEqual(r.slices[i - 1].startNs);
    });
});

describe('wait split (AC11)', () => {
    it('reports cap wait and worker wait separately, exactly as on the summary', () => {
        const ecology = win.steps[1];
        const w = waitSplit(ecology);
        expect(w.capMs).toBe(ecology.capWaitMs);
        expect(w.workerMs).toBe(ecology.workerWaitMs);
        expect(w.totalMs).toBe(ecology.capWaitMs + ecology.workerWaitMs);
        expect(w.capShare).toBeCloseTo(6240 / (6240 + 8), 6);
    });
    it('raw record waits add up to the summary total', () => {
        const ecology = win.steps[1];
        const sum = raw.slices.filter((s) => s.ownerName === 'ecology').reduce((a, s) => a + s.waitMs, 0);
        expect(sum).toBeCloseTo(ecology.capWaitMs + ecology.workerWaitMs, 0);
    });
    it('has zero share when there was no wait', () => {
        const w = waitSplit({ ...win.steps[0], capWaitMs: 0, workerWaitMs: 0 });
        expect(w).toMatchObject({ capMs: 0, workerMs: 0, totalMs: 0, capShare: 0 });
    });
});

describe('step share (AC10, selected step)', () => {
    it('splits worker time during the step run by owner and sums to 100 percent', () => {
        const ecology = win.steps[1];
        const s = buildStepShare(raw, ecology);
        expect(s.rows.reduce((a, r) => a + r.pct, 0)).toBe(100);
        expect(s.rows.reduce((a, r) => a + r.share, 0)).toBeCloseTo(1, 9);
        expect(s.rows[0].name).toBe('ecology');
        expect(s.rows.map((r) => r.name)).toContain('PathfindingModule');
    });
    it('is empty when no raw jobs overlap the step', () => {
        const s = buildStepShare({ ...raw, slices: [] }, win.steps[1]);
        expect(s.rows).toEqual([]);
    });
});

describe('pool health', () => {
    const t1 = fixture.timing.jobs.units[0];
    const unavailable = fixture.timing.unavailable;
    it('marks fields named in unavailable as unavailable, not 0', () => {
        const h = poolHealth(t1, unavailable);
        expect(h.capUse).toEqual({ available: false, value: null });
        expect(h.timeAtCapMs.available).toBe(false);
        expect(h.peakQueueDepth.available).toBe(false);
        expect(h.inlineFallbacks).toEqual({ available: true, value: 3 });
        expect(h.inboxDrops).toEqual({ available: true, value: 1 });
    });
    it('shows real values once the game stops listing them as unavailable', () => {
        const h = poolHealth({ ...t1, pool: { ...t1.pool, capUse: 2, peakQueueDepth: 9, timeAtCapMs: 1200 } }, []);
        expect(h.capUse).toEqual({ available: true, value: 2 });
        expect(h.peakQueueDepth.value).toBe(9);
        expect(h.timeAtCapMs.value).toBe(1200);
    });
    it('lists queue wait p95 per owner, worst first', () => {
        const h = poolHealth(t1, unavailable);
        expect(h.perOwner.map((o) => o.name)).toEqual(['ecology', 'weather', 'PathfindingModule']);
        expect(h.perOwner[0].waitP95Ms).toBe(1500);
    });
    it('everything is unavailable with no tier 1 data', () => {
        const h = poolHealth(null, []);
        expect(h.perOwner).toEqual([]);
        expect(h.inlineFallbacks.available).toBe(false);
    });
});

describe('step rows, late results, cost', () => {
    it('lays out kick -> apply windows, run span and stall per channel on one axis', () => {
        const rows = buildStepBars(win.steps);
        expect(rows.channels.map((c) => c.channel)).toEqual(['weather', 'ecology']);
        expect(rows.channels[0].bars).toHaveLength(2);
        expect(rows.domain.t0).toBe(win.steps[0].kick);
        expect(rows.domain.t1).toBeGreaterThanOrEqual(win.steps[1].ready);
        const e = rows.channels[1].bars[0];
        expect(e.late).toBe(true);
        expect(e.kick).toBe(win.steps[1].kick);
        expect(e.ready).toBe(win.steps[1].ready);
        expect(e.apply).toBe(win.steps[1].apply);
        expect(rows.channels[0].bars[0].late).toBe(false);
    });
    it('is empty without steps', () => {
        expect(buildStepBars([]).channels).toEqual([]);
    });
    it('lists late steps newest first', () => {
        const late = lateResults(win.steps);
        expect(late).toHaveLength(1);
        expect(late[0].channel).toBe('ecology');
        expect(late[0].stallMs).toBe(85);
        expect(lateResults([])).toEqual([]);
    });
    it('step key is unique per channel and kick key', () => {
        expect(stepKey(win.steps[0])).not.toBe(stepKey(win.steps[2]));
    });
    it('computes critical path, parallelism and window used', () => {
        const c = stepCost(win.steps[1], raw);
        expect(c.criticalPathMs).toBeCloseTo(1.9 + 1.4 + 85, 6);
        expect(c.runSpanMs).toBeCloseTo((win.steps[1].ready - win.steps[1].firstStart) / 1e6, 6);
        expect(c.parallelism).toBeCloseTo(win.steps[1].cpuMs / c.runSpanMs, 6);
        expect(c.windowUsedPct).toBeGreaterThan(100);
        expect(c.imbalance).toBeGreaterThan(1);
    });
    it('critical path is unavailable without capture/apply and imbalance without raw records', () => {
        const { captureMs: _c, applyMs: _a, ...bare } = win.steps[0];
        const c = stepCost(bare, null);
        expect(c.criticalPathMs).toBeNull();
        expect(c.imbalance).toBeNull();
        expect(c.windowUsedPct).toBeLessThan(100);
    });
    it('groups the step ranges by worker from raw records', () => {
        const r = rangesPerWorker(raw, win.steps[1]);
        expect(r.map((x) => x.worker)).toEqual([4, 5]);
        expect(r.reduce((a, x) => a + x.ranges, 0)).toBe(8);
        expect(rangesPerWorker(null, win.steps[1])).toEqual([]);
    });
});

describe('domainOf', () => {
    it('ends at the newest bucket end and spans the requested seconds', () => {
        const d = domainOf(win.units, 10);
        expect(d.t1).toBeCloseTo(60, 9);
        expect(d.t0).toBeCloseTo(50, 9);
        expect(domainOf([], 10)).toEqual({ t0: 0, t1: 10 });
    });
});
