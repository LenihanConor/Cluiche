// Perf Inspector task 10: edge tests for the pure lane / decoder models. Garbage input must never throw.

import { encodeU16, encodeU8 } from './__fixtures__/makeFixture';
import {
    applyViewState, bucketize, buildLanes, colourOf, findFrameAt, newestTime, sliceWindow, tripSpans, unwrapWindowReply,
} from './laneModel';
import type { Lane, PerfWindowReply, PerfWindowUnit, TripRange } from './laneModel';
import { reconcileSelection, selectionRect } from './selection';
import { parseFrameReply } from './frameDetail';
import { interpretPauseReply } from './pauseReply';
import { readReply } from './reply';
import { decodeU16, decodeU32, decodeU64, decodeU8 } from './decode';
import { parseJobsWindow, parseRawJobs } from './jobs';
import { buildModuleTable } from './moduleTable';
import { latencyActionsFromReply } from './latency';
import { tripRangesFromRecords } from './trips';
import type { PerfTimingPush, TripRecord } from '../types';

interface UnitSpec {
    unit?: string; crc?: number; hz?: number; first?: number; modules?: { crc: number; name: string }[];
    tick: number[]; mods?: number[][]; flags?: number[];
}

/** Hand-made unit. mods[k][i] are module-major raw values (0.01 ms). */
function unitOf(s: UnitSpec): PerfWindowUnit {
    const n = s.tick.length;
    const modules = s.modules ?? [];
    const mods = s.mods ?? modules.map(() => new Array<number>(n).fill(100));
    return {
        unit: s.unit ?? 'U', unitCrc: s.crc ?? 1, hz: s.hz ?? 10, firstEntry: s.first ?? 0, count: n, modules,
        unitMs: encodeU16(s.tick), moduleMs: encodeU16(mods.flat()), ...(s.flags ? { flags: encodeU8(s.flags) } : {}),
    };
}
const winOf = (...units: PerfWindowUnit[]): PerfWindowReply => ({ v: 1, seconds: 60, units, markers: [], trips: [], truncated: false });

const garbage: unknown[] = [null, undefined, 0, 7, 'x', true, [], {}, [null], [[]], { success: true }, { success: true, result: null },
    { success: true, result: 5 }, { success: true, result: [] }, { success: true, result: { v: 1 } }, { result: { v: 1, units: 'x' } }];

describe('garbage replies never throw', () => {
    it('unwrapWindowReply, readReply, parseFrameReply, interpretPauseReply', () => {
        for (const g of garbage) {
            expect(() => unwrapWindowReply(g)).not.toThrow();
            expect(() => readReply(g)).not.toThrow();
            expect(parseFrameReply(g).kind).toBe('error');
            expect(['ok', 'error', 'unsupported']).toContain(interpretPauseReply(g).kind);
        }
    });

    it('decoders treat wrong types and malformed base64 as no data', () => {
        for (const g of [null, undefined, '', '!!!', 'AAA', 'A', '====', 'a b c', 12345 as unknown as string, {} as unknown as string, [] as unknown as string]) {
            expect(() => decodeU16(g)).not.toThrow();
            expect(() => decodeU32(g)).not.toThrow();
            expect(() => decodeU64(g)).not.toThrow();
            expect(() => decodeU8(g)).not.toThrow();
        }
        expect(decodeU16('!!!')).toHaveLength(0);
        expect(decodeU16('AAAA')).toHaveLength(1);   // 3 bytes -> 1 whole u16, trailing odd byte dropped
    });

    it('parseJobsWindow and parseRawJobs', () => {
        for (const g of garbage) {
            expect(() => parseJobsWindow(g)).not.toThrow();
            expect(() => parseRawJobs(g, new Map())).not.toThrow();
        }
        expect(parseJobsWindow({ units: [null, 3, 'x', { unit: 7 }, { unit: 'u', owners: [null, 5, { crc: 'x' }] }], steps: [null, {}, 4] })?.units).toHaveLength(1);
        const raw = parseRawJobs({ count: 99, startNs: 'AAAA', durNs: '', owner: '!!', tag: null, waitUs: 5, worker: [], range: {}, rangeCount: '', flags: '' }, new Map());
        expect(raw?.slices).toHaveLength(0);
        expect(parseRawJobs({ count: -3, unit: 'u' }, new Map())?.slices).toHaveLength(0);
        expect(parseRawJobs({ error: 'unknown unit' }, new Map())).toBeNull();
    });

    it('latencyActionsFromReply', () => {
        for (const g of garbage) expect(() => latencyActionsFromReply(g)).not.toThrow();
        expect(latencyActionsFromReply({ actions: { a: null, b: { count: 2, p95Ms: 3 } } })).toHaveLength(2);
    });

    it('buildLanes tolerates null units, null modules and non-array fields', () => {
        const bad = { v: 1, seconds: 60, markers: [], trips: [], truncated: false,
            units: [null, 5, 'x', { unit: 'a', unitCrc: 1, hz: 10, firstEntry: 0, count: 3, modules: [null, 7, { crc: 1, name: 'ok' }], unitMs: encodeU16([1, 2, 3]), moduleMs: '' }] };
        let lanes: Lane[] = [];
        expect(() => { lanes = buildLanes(bad as unknown as PerfWindowReply); }).not.toThrow();
        expect(lanes.length).toBeGreaterThan(0);
        expect(lanes.find((l) => l.unit === 'a')?.modules.map((m) => m.name)).toEqual(['ok']);
        for (const g of garbage) expect(() => buildLanes(g as PerfWindowReply)).not.toThrow();
    });

    it('buildModuleTable tolerates null units and non-string module names', () => {
        const push = { scale: 0.01, windowSec: 10, unavailable: [], units: [null, 3, { unit: 'M', hz: 60, samples: 1, tick: [1, 2], modules: ['A', null, 5], ewma: [1], p95: [2] }] };
        expect(() => buildModuleTable(push as unknown as PerfTimingPush, {})).not.toThrow();
        const groups = buildModuleTable(push as unknown as PerfTimingPush, {});
        expect(groups.map((g) => g.unit)).toEqual(['M']);
        expect(groups[0].rows.map((r) => r.module)).toEqual(['A']);
        for (const g of garbage) expect(() => buildModuleTable(g as PerfTimingPush, {})).not.toThrow();
    });
});

describe('empty and degenerate windows', () => {
    it('count 0 units yield empty lanes and a safe view', () => {
        const lanes = buildLanes(winOf({ ...unitOf({ tick: [100] }), count: 0, unitMs: '', moduleMs: '' }));
        expect(lanes).toHaveLength(1);
        expect(lanes[0].count).toBe(0);
        expect(newestTime(lanes)).toBeNull();
        const v = applyViewState(lanes, { windowSeconds: 10, pausedAt: null });
        expect(v.lanes[0].count).toBe(0);
        expect(v.outOfRange).toBe(false);
        expect(bucketize(lanes[0], 100, v.domain).columns).toBe(100);
        expect(tripSpans(lanes[0], [{ unit: 'U', firstEntry: 0, lastEntry: 5 }])).toEqual([]);
        expect(findFrameAt(lanes[0], 1)).toBeNull();
    });

    it('a window with no units at all', () => {
        const lanes = buildLanes(winOf());
        expect(lanes).toEqual([]);
        expect(applyViewState(lanes, { windowSeconds: 10, pausedAt: null }).anchorT).toBe(0);
        expect(applyViewState(lanes, { windowSeconds: 10, pausedAt: 5 }).outOfRange).toBe(false);
    });

    it('unit hz 0, NaN, Infinity, negative or a string gives an empty lane with no NaN times', () => {
        for (const hz of [0, NaN, Infinity, -10, '60' as unknown as number, null as unknown as number]) {
            const lanes = buildLanes(winOf({ ...unitOf({ tick: [100, 100] }), hz }));
            const l = lanes[0];
            if (hz === Infinity) { expect(l.t.every((t) => Number.isFinite(t))).toBe(true); continue; }
            if (hz === ('60' as unknown)) { expect(l.count).toBe(2); continue; }  // Number('60') is a valid hz
            expect(l.count).toBe(0);
            expect(l.budgetMs).toBe(0);
            expect(() => bucketize(l, 10, { t0: 0, t1: 1 })).not.toThrow();
        }
    });

    it('names with quotes, backslashes, control characters, unicode and markup pass through verbatim', () => {
        const name = 'a"b\\c\n\t\u0001é€ </script><b>x</b>';
        const lanes = buildLanes(winOf(unitOf({ unit: name, modules: [{ crc: 5, name }], tick: [100] })));
        expect(lanes[0].unit).toBe(name);
        expect(lanes[0].modules[0].name).toBe(name);
    });
});

describe('module set changes between replies', () => {
    const A = { crc: 1001, name: 'A' };
    const B = { crc: 1002, name: 'B' };
    const C = { crc: 1003, name: 'C' };

    it('a module appearing mid-window keeps earlier modules aligned, colours stable, and zeros before it exists', () => {
        const n = 8;
        const before = buildLanes(winOf(unitOf({ modules: [A, B], tick: new Array(n).fill(500), mods: [new Array(n).fill(100), new Array(n).fill(200)] })))[0];
        // C is in the reply from the start of the window but only recorded in the last 3 entries
        const c = [0, 0, 0, 0, 0, 300, 300, 300];
        const after = buildLanes(winOf(unitOf({ modules: [A, B, C], tick: new Array(n).fill(500), mods: [new Array(n).fill(100), new Array(n).fill(200), c] })))[0];
        expect(after.modules.map((m) => m.crc)).toEqual([1001, 1002, 1003]);
        expect(after.modules[0].colourIndex).toBe(before.modules[0].colourIndex);
        expect(after.modules[1].colourIndex).toBe(before.modules[1].colourIndex);
        expect(after.modules[2].colourIndex).toBe(colourOf(1003));
        for (let i = 0; i < n; i++) {
            expect(after.moduleMs[0 * n + i]).toBeCloseTo(1);
            expect(after.moduleMs[1 * n + i]).toBeCloseTo(2);
            expect(after.moduleMs[2 * n + i]).toBeCloseTo(c[i] / 100);
            expect(after.tickMs[i] - (1 + 2 + c[i] / 100)).toBeCloseTo(after.unattributedMs[i] - after.overAttributedMs[i]);
        }
        const bucket = bucketize(after, n, { t0: -0.1, t1: 0.7 });
        expect(bucket.moduleMean[2 * n + 2]).toBeCloseTo(0);
        expect(bucket.moduleMean[2 * n + 6]).toBeCloseTo(3);
    });

    it('a module block that does not match count falls back to the unit length, never reads out of range', () => {
        const u = unitOf({ modules: [A, B], tick: [100, 100, 100, 100], mods: [[1, 2, 3, 4], [5, 6, 7, 8]] });
        const lane = buildLanes(winOf({ ...u, moduleMs: encodeU16([1, 2, 3]) }))[0];   // truncated block
        expect(lane.count).toBe(4);
        expect(lane.moduleMs.every((v) => Number.isFinite(v))).toBe(true);
    });

    it('module removed in the next reply: lane has only the remaining modules, selection stays valid', () => {
        const r1 = buildLanes(winOf(unitOf({ modules: [A, B], tick: [100, 100, 100], first: 10 })));
        const r2 = buildLanes(winOf(unitOf({ modules: [A], tick: [100, 100, 100], first: 11 })));
        expect(r2[0].modules).toHaveLength(1);
        expect(reconcileSelection({ unit: 'U', index: 11 }, r1)).toBeNull();
        expect(reconcileSelection({ unit: 'U', index: 11 }, r2)).toBeNull();
    });

    it('unit renamed, added and removed between polls is reported through reconcileSelection', () => {
        const poll1 = buildLanes(winOf(unitOf({ unit: 'Sim', tick: [1, 1, 1], first: 0 }), unitOf({ unit: 'Main', tick: [1, 1, 1], first: 0 })));
        const poll2 = buildLanes(winOf(unitOf({ unit: 'Sim2', tick: [1, 1, 1], first: 0 }), unitOf({ unit: 'Main', tick: [1, 1, 1], first: 0 }), unitOf({ unit: 'New', tick: [1], first: 0 })));
        expect(reconcileSelection({ unit: 'Sim', index: 1 }, poll1)).toBeNull();
        expect(reconcileSelection({ unit: 'Sim', index: 1 }, poll2)).toMatch(/no longer reported/);
        expect(reconcileSelection({ unit: 'Main', index: 1 }, poll2)).toBeNull();
        expect(poll2.map((l) => l.unit)).toEqual(['Sim2', 'Main', 'New']);
    });
});

describe('non-monotonic and duplicate data', () => {
    it('firstEntry going backwards between replies (game restart) builds a fresh model', () => {
        const old = buildLanes(winOf(unitOf({ tick: new Array(50).fill(100), first: 5000 })));
        const fresh = buildLanes(winOf(unitOf({ tick: new Array(20).fill(100), first: 0 })));
        expect(newestTime(old)).toBeGreaterThan(newestTime(fresh)!);
        // a view paused on the old session now points past the new data: nothing visible, not out of range, no throw
        const v = applyViewState(fresh, { windowSeconds: 10, pausedAt: newestTime(old) });
        expect(v.lanes[0].count).toBe(0);
        expect(v.outOfRange).toBe(false);
        expect(() => bucketize(v.lanes[0], 200, v.domain)).not.toThrow();
        // a selection on the old session's index is not stale-by-eviction for the fresh lane (index beyond end), unit still present
        expect(reconcileSelection({ unit: 'U', index: 5010 }, fresh)).toBeNull();
        expect(reconcileSelection({ unit: 'U', index: 5010 }, old)).toBeNull();
    });

    it('bucketize with unsorted timestamps does not throw, counts every in-domain entry and keeps the worst tick', () => {
        const lane = buildLanes(winOf(unitOf({ tick: [100, 900, 200, 300, 400, 500, 600, 700, 800, 150], first: 1 })))[0];
        lane.t.reverse();                                  // descending times: violates the ascending invariant
        const b = bucketize(lane, 5, { t0: 0, t1: 1.1 });
        expect(Math.max(...b.tickMax)).toBeCloseTo(9);
        let seen = 0;
        for (let c = 0; c < b.columns; c++) if (b.firstIndex[c] >= 0) seen++;
        expect(seen).toBeGreaterThan(0);
        expect(() => sliceWindow([lane], 1, 1.1)).not.toThrow();
        expect(() => findFrameAt(lane, 0.5)).not.toThrow();
    });

    it('duplicate entries (same index twice) are kept positionally, never merged', () => {
        const l = buildLanes(winOf(unitOf({ tick: [100, 100, 100], first: 4 }), unitOf({ tick: [100, 100, 100], first: 4 })));
        expect(l).toHaveLength(2);
        expect(Array.from(l[0].index)).toEqual([4, 5, 6]);
    });

    it('a clamped value (65535) in the middle of a bucket flags the column but not its neighbours and keeps the spike', () => {
        const tick = new Array(30).fill(100);
        tick[14] = 65535;                                  // t = 1.4 s at 10 Hz -> column 1 of 3 over (0, 3]
        const lane = buildLanes(winOf(unitOf({ tick, first: 1 })))[0];
        expect(lane.clamped[14]).toBe(1);
        expect(lane.clamped[13]).toBe(0);
        const b = bucketize(lane, 3, { t0: 0.05, t1: 3.05 });
        expect(Array.from(b.hasClamped)).toEqual([0, 1, 0]);
        expect(b.tickMax[1]).toBeCloseTo(655.35, 1);
        expect(b.tickMax[0]).toBeCloseTo(1);
    });
});

describe('paused view with trips', () => {
    const lane = (): Lane => buildLanes(winOf(unitOf({ tick: new Array(100).fill(100), first: 0, hz: 10 })))[0];

    it('trip shading for a frozen view clips to the frozen domain, drops trips after it', () => {
        const l = lane();
        const view = applyViewState([l], { windowSeconds: 10, pausedAt: 5 });   // domain (-5, 5]
        const ranges: TripRange[] = [
            { unit: 'U', firstEntry: 10, lastEntry: 20 },     // 1.0 .. 2.1 inside
            { unit: 'U', firstEntry: 45, lastEntry: 60 },     // straddles the freeze at 5.0
            { unit: 'U', firstEntry: 70, lastEntry: 80 },     // entirely after the freeze (trip happened while paused)
        ];
        const spans = tripSpans(l, ranges, view.domain);
        expect(spans).toHaveLength(2);
        expect(spans[1].t1).toBeCloseTo(5);
        expect(newestTime([l])).toBeGreaterThan(5);           // data newer than the freeze exists but is not drawn
    });

    it('trip records that arrive while paused update ranges without touching the frozen anchor', () => {
        const rec = (first: number, last: number): TripRecord => ({
            unit: 'U', buildConfig: 'Debug', firstFrame: first, lastFrame: last, budgetMs: 1, peakMs: 2, suppressedSinceLastTrip: 0,
            topModules: [], sections: [], worstFrame: { index: first, unitMs: 2, modules: [] }, recentUnitMs: [],
        });
        const ranges = tripRangesFromRecords([rec(70, 80), rec(90, 85)]);
        expect(ranges).toEqual([{ unit: 'U', firstEntry: 70, lastEntry: 80 }]);
        const view = applyViewState([lane()], { windowSeconds: 10, pausedAt: 5 });
        expect(view.anchorT).toBe(5);
    });

    it('selecting a trip frame outside the visible (paused) window is valid while still retained', () => {
        const l = lane();
        const view = applyViewState([l], { windowSeconds: 10, pausedAt: 3 });   // domain (-7, 3]
        const index = 80;                                                       // t = 8, after the frozen view
        expect(selectionRect(l, index, view.domain, 800)).toBeNull();           // no outline outside the domain, no throw
        expect(reconcileSelection({ unit: 'U', index }, [l])).toBeNull();       // reconcile uses full lanes, not the slice
        expect(reconcileSelection({ unit: 'U', index }, view.lanes)).toBeNull();// even against the sliced lanes: not stale by eviction
    });

    it('a trip frame older than the retained ring is stale; one in the future is not', () => {
        const l = buildLanes(winOf(unitOf({ tick: new Array(100).fill(100), first: 1000 })))[0];
        expect(reconcileSelection({ unit: 'U', index: 10 }, [l])).toMatch(/no longer retained/);
        expect(reconcileSelection({ unit: 'U', index: 5000 }, [l])).toBeNull();
    });

    it('pausedAt older than all data is outOfRange and still safe to bucketize', () => {
        const l = buildLanes(winOf(unitOf({ tick: new Array(10).fill(100), first: 600 })))[0];
        const v = applyViewState([l], { windowSeconds: 10, pausedAt: 1 });
        expect(v.outOfRange).toBe(true);
        expect(() => bucketize(v.lanes[0], 100, v.domain)).not.toThrow();
    });
});

describe('frame detail while profiling toggles', () => {
    const base = { v: 1, unit: 'Main', frame: 7, unitMs: 3, modules: [{ crc: 1, name: 'A', ms: 1 }] };
    it('sections available true -> false -> true', () => {
        const on = parseFrameReply({ success: true, result: { ...base, sections: { available: true, sections: [{ name: 's', ms: 1 }] } } });
        const off = parseFrameReply({ success: true, result: { ...base, sections: { available: false, reason: 'profiling off' } } });
        if (on.kind !== 'ok' || off.kind !== 'ok') throw new Error('expected ok');
        expect(on.data.sections).toHaveLength(1);
        expect(on.data.sectionsUnavailableReason).toBeNull();
        expect(off.data.sections).toEqual([]);
        expect(off.data.sectionsUnavailableReason).toBe('profiling off');
        const back = parseFrameReply({ success: true, result: { ...base, sections: { available: true, sections: [{ name: 't', ms: 2 }] } } });
        if (back.kind !== 'ok') throw new Error('expected ok');
        expect(back.data.sections[0].name).toBe('t');
    });

    it('available true with sections of the wrong type or malformed entries degrades quietly', () => {
        const r = parseFrameReply({ success: true, result: { ...base, modules: [null, 4, { crc: 'x' }], sections: { available: true, sections: 'oops' } } });
        if (r.kind !== 'ok') throw new Error('expected ok');
        expect(r.data.sections).toEqual([]);
        expect(r.data.modules).toHaveLength(3);
        const r2 = parseFrameReply({ success: true, result: { ...base, sections: { available: true, sections: [{ name: 5, ms: 'x' }] } } });
        if (r2.kind !== 'ok') throw new Error('expected ok');
        expect(r2.data.sections).toEqual([{ name: '?', ms: 0 }]);
        const r3 = parseFrameReply({ success: true, result: { ...base, sections: { available: true, sections: [null] } } });
        if (r3.kind !== 'ok') throw new Error('expected ok');
        expect(r3.data.sections).toEqual([{ name: '?', ms: 0 }]);
    });
});
