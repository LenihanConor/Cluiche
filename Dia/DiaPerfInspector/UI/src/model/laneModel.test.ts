// NOTE: the fixture (__fixtures__/perfWindow.fixture.json) is SYNTHETIC. It is produced by
// __fixtures__/makeFixture.ts to match the perf.window wire format (base64 LE uint16, module-major,
// flags omitted when none). Replace it with a recording from a real `--perf` session when one is
// available. The decoder is separately pinned to a hand-computed known answer in decode.test.ts.

import fixture from './__fixtures__/perfWindow.fixture.json';
import { encodeU16, encodeU8, makeWindow } from './__fixtures__/makeFixture';
import {
    applyViewState, bucketize, buildLanes, findFrameAt, kModuleColourCount, sliceWindow, tripSpans, unwrapWindowReply,
} from './laneModel';
import type { Lane, PerfWindowReply, TripRange } from './laneModel';

const win = fixture as unknown as PerfWindowReply;
const lanesOf = (w: unknown = win): Lane[] => buildLanes(w as PerfWindowReply);
const byUnit = (lanes: Lane[], name: string): Lane => lanes.find((l) => l.unit === name)!;

/** Hand-made single-unit window: hz 10, firstEntry 11 => t = 1.1 .. 11.0, domain (1, 11] is 10 columns of 10. */
function tinyWindow(opts: { modules?: number } = {}): PerfWindowReply {
    const n = 100;
    const m = opts.modules ?? 1;
    const unitMs = new Array<number>(n).fill(500);
    unitMs[37] = 5000;
    const mod = new Array<number>(n * m).fill(200);
    const flags = new Array<number>(n).fill(0);
    for (const [from, to] of [[50, 60], [20, 25]]) {
        for (let i = from; i < to; i++) {
            unitMs[i] = 0;
            flags[i] = 2;
            for (let k = 0; k < m; k++) mod[k * n + i] = 0;
        }
    }
    return {
        v: 1, seconds: 10, markers: [], trips: [], truncated: false,
        units: [{
            unit: 'T', unitCrc: 7, hz: 10, firstEntry: 11, count: n,
            modules: Array.from({ length: m }, (_, k) => ({ crc: 100 + k, name: `M${k}` })),
            unitMs: encodeU16(unitMs), moduleMs: encodeU16(mod), flags: encodeU8(flags),
        }],
    };
}

function emptied(w: PerfWindowReply): PerfWindowReply {
    return { ...w, units: [{ ...w.units[0], count: 0, unitMs: '', moduleMs: '', flags: undefined }] };
}

describe('buildLanes on the fixture', () => {
    const lanes = lanesOf();

    it('makes one lane per unit', () => {
        expect(lanes.map((l) => l.unit)).toEqual(['Main', 'Sim', 'Render']);
        expect(byUnit(lanes, 'Main').modules).toHaveLength(5);
        expect(byUnit(lanes, 'Render').modules).toHaveLength(3);
    });

    it('times are index / hz and aligned across hz', () => {
        const main = byUnit(lanes, 'Main');
        const render = byUnit(lanes, 'Render');
        expect(main.index[0]).toBe(9000);
        expect(main.t[0]).toBeCloseTo(300, 9);
        expect(render.t[0]).toBeCloseTo(300, 9);
        expect(main.t[1] - main.t[0]).toBeCloseTo(1 / 30, 9);
        expect(render.t[1] - render.t[0]).toBeCloseTo(1 / 60, 9);
    });

    it('Render has twice the entries per second of Main/Sim', () => {
        expect(byUnit(lanes, 'Main').count).toBe(360);
        expect(byUnit(lanes, 'Sim').count).toBe(360);
        expect(byUnit(lanes, 'Render').count).toBe(720);
    });

    it('budget defaults to 1000/hz and can be overridden by unit name', () => {
        expect(byUnit(lanes, 'Main').budgetMs).toBeCloseTo(33.333, 2);
        expect(byUnit(lanes, 'Render').budgetMs).toBeCloseTo(16.667, 2);
        const o = buildLanes(win, { budgets: { Sim: 20 } });
        expect(byUnit(o, 'Sim').budgetMs).toBe(20);
        expect(byUnit(o, 'Main').budgetMs).toBeCloseTo(33.333, 2);
    });

    it('decodes fixed point 0.01 ms, flags clamped and gap, maxMs tracks the largest tick', () => {
        const main = byUnit(lanes, 'Main');
        expect(main.clamped[200]).toBe(1);
        expect(main.tickMs[200]).toBeCloseTo(655.35, 2);
        expect(main.maxMs).toBeCloseTo(655.35, 2);
        const sim = byUnit(lanes, 'Sim');
        for (let i = 100; i < 110; i++) {
            expect(sim.gap[i]).toBe(1);
            expect(sim.tickMs[i]).toBe(0);
        }
        expect(sim.gap[99]).toBe(0);
        expect(sim.gap[110]).toBe(0);
        expect(sim.maxMs).toBeGreaterThan(sim.budgetMs); // fixture has over-budget spikes
    });

    it('uses typed arrays', () => {
        const l = byUnit(lanes, 'Sim');
        expect(l.tickMs).toBeInstanceOf(Float32Array);
        expect(l.moduleMs).toHaveLength(l.count * l.modules.length);
        expect(l.t).toBeInstanceOf(Float64Array);
    });

    it('module colour is stable when module order changes', () => {
        const w = makeWindow() as unknown as PerfWindowReply;
        const reordered: PerfWindowReply = JSON.parse(JSON.stringify(w));
        const u = reordered.units.find((x) => x.unit === 'Sim')!;
        // reverse module order (data is module-major, so reverse the blocks too)
        const n = u.count;
        const mm = u.modules.length;
        const src = Uint8Array.from(atob(u.moduleMs), (c) => c.charCodeAt(0));
        const raw = new Uint16Array(n * mm);
        for (let i = 0; i < raw.length; i++) raw[i] = src[i * 2] | (src[i * 2 + 1] << 8);
        const rev = new Uint16Array(raw.length);
        for (let k = 0; k < mm; k++) rev.set(raw.subarray(k * n, (k + 1) * n), (mm - 1 - k) * n);
        u.modules = [...u.modules].reverse();
        u.moduleMs = encodeU16(rev);

        const a = byUnit(buildLanes(w), 'Sim');
        const b = byUnit(buildLanes(reordered), 'Sim');
        for (const m of a.modules) {
            const other = b.modules.find((x) => x.crc === m.crc)!;
            expect(other.colourIndex).toBe(m.colourIndex);
            expect(m.colourIndex).toBeGreaterThanOrEqual(0);
            expect(m.colourIndex).toBeLessThan(kModuleColourCount);
        }
        // and the values follow the module, not the position
        const ai = a.modules.findIndex((m) => m.name === 'Physics');
        const bi = b.modules.findIndex((m) => m.name === 'Physics');
        expect(ai).not.toBe(bi);
        for (let i = 0; i < a.count; i++) {
            expect(b.moduleMs[bi * b.count + i]).toBeCloseTo(a.moduleMs[ai * a.count + i], 5);
        }
    });
});

describe('AC2: bars sum to tick', () => {
    it('tick = sum + unattributed - overAttributed for every entry; remainder within 1 ms when not over-attributed', () => {
        let sawOver = false;
        let sawUnattributed = false;
        for (const lane of lanesOf()) {
            const m = lane.modules.length;
            for (let i = 0; i < lane.count; i++) {
                let sum = 0;
                for (let k = 0; k < m; k++) sum += lane.moduleMs[k * lane.count + i];
                const tick = lane.tickMs[i];
                expect(lane.unattributedMs[i]).toBeGreaterThanOrEqual(0);
                expect(lane.overAttributedMs[i]).toBeGreaterThanOrEqual(0);
                expect(Math.abs(tick - (sum + lane.unattributedMs[i] - lane.overAttributedMs[i]))).toBeLessThan(0.001);
                if (lane.overAttributedMs[i] > 0) {
                    sawOver = true;
                    expect(lane.unattributedMs[i]).toBe(0);
                } else {
                    expect(Math.abs(sum + lane.unattributedMs[i] - tick)).toBeLessThan(1);
                }
                if (lane.unattributedMs[i] > 0) sawUnattributed = true;
            }
        }
        expect(sawOver).toBe(true);
        expect(sawUnattributed).toBe(true);
    });
});

describe('sliceWindow', () => {
    const lanes = lanesOf();

    it('10 s keeps (anchor-10, anchor] per unit, time aligned', () => {
        const s = sliceWindow(lanes, 10);
        expect(byUnit(s, 'Main').count).toBe(300);
        expect(byUnit(s, 'Sim').count).toBe(300);
        expect(byUnit(s, 'Render').count).toBe(600);
        const newest = byUnit(lanes, 'Render').t[719];
        for (const l of s) {
            expect(l.t[l.count - 1]).toBeLessThanOrEqual(newest + 1e-6);
            expect(l.t[0]).toBeGreaterThan(newest - 10);
            expect(l.moduleMs).toHaveLength(l.count * l.modules.length);
        }
    });

    it('30 and 60 s return everything when data is shorter than the window', () => {
        for (const sec of [30, 60]) {
            const s = sliceWindow(lanes, sec);
            expect(s.map((l) => l.count)).toEqual([360, 360, 720]);
        }
    });

    it('explicit anchor and recomputed maxMs', () => {
        const sim = byUnit(lanes, 'Sim');
        const s = byUnit(sliceWindow(lanes, 10, sim.t[50]), 'Sim');
        expect(s.count).toBe(51);
        expect(s.maxMs).toBeLessThanOrEqual(sim.maxMs);
    });

    it('empty input gives empty output', () => {
        expect(sliceWindow([], 10)).toEqual([]);
    });
});

describe('applyViewState / pause', () => {
    it('live view anchors at the newest time across all units', () => {
        const v = applyViewState(lanesOf(), { windowSeconds: 10, pausedAt: null });
        expect(v.paused).toBe(false);
        expect(v.outOfRange).toBe(false);
        expect(v.anchorT).toBeCloseTo(300 + 719 / 60, 6);
        expect(v.domain.t1).toBe(v.anchorT);
        expect(v.domain.t0).toBeCloseTo(v.anchorT - 10, 9);
    });

    it('a paused view is unchanged by a newer reply', () => {
        const older = lanesOf(makeWindow(8));
        const newer = lanesOf(makeWindow(12));
        const view = { windowSeconds: 10 as const, pausedAt: 306 };
        const a = applyViewState(older, view);
        const b = applyViewState(newer, view);
        expect(b.anchorT).toBe(306);
        expect(a.outOfRange).toBe(false);
        expect(b.outOfRange).toBe(false);
        for (let u = 0; u < a.lanes.length; u++) {
            expect(a.lanes[u].count).toBeGreaterThan(0);
            expect(Array.from(b.lanes[u].tickMs)).toEqual(Array.from(a.lanes[u].tickMs));
            expect(Array.from(b.lanes[u].index)).toEqual(Array.from(a.lanes[u].index));
        }
        // a live view does move
        const live = applyViewState(newer, { windowSeconds: 10, pausedAt: null });
        expect(live.anchorT).toBeGreaterThan(306);
    });

    it('pausedAt older than the retained data is outOfRange with an empty visible slice', () => {
        const v = applyViewState(lanesOf(), { windowSeconds: 10, pausedAt: 100 });
        expect(v.paused).toBe(true);
        expect(v.outOfRange).toBe(true);
        expect(v.lanes.every((l) => l.count === 0)).toBe(true);
        expect(v.anchorT).toBe(100);
    });

    it('no data yields no lanes and no crash', () => {
        const v = applyViewState([], { windowSeconds: 30, pausedAt: null });
        expect(v.lanes).toEqual([]);
        expect(v.outOfRange).toBe(false);
    });
});

describe('bucketize', () => {
    const domain = { t0: 1, t1: 11 };

    it('one column per request with max-tick spikes preserved', () => {
        const lane = buildLanes(tinyWindow())[0];
        const b = bucketize(lane, 10, domain);
        expect(b.columns).toBe(10);
        expect(b.tickMax).toHaveLength(10);
        expect(b.moduleMean).toHaveLength(10);
        expect(b.tickMax[3]).toBeCloseTo(50, 3); // spike survives
        expect(b.tickMax[4]).toBeCloseTo(5, 3);
        expect(b.overBudget[3]).toBe(0); // budget at 10 Hz is 100 ms
    });

    it('over budget is tick > budget', () => {
        const lane = buildLanes(tinyWindow(), { budgets: { T: 20 } })[0];
        const b = bucketize(lane, 10, domain);
        expect(b.overBudget[3]).toBe(1);
        expect(b.overBudget[4]).toBe(0);
    });

    it('module mean covers non-gap entries only; gap-only column is a gap; partial gap is not', () => {
        const lane = buildLanes(tinyWindow())[0];
        const b = bucketize(lane, 10, domain);
        expect(b.hasGap[5]).toBe(1);
        expect(b.tickMax[5]).toBe(0);
        expect(b.hasGap[2]).toBe(0); // entries 20..24 gap, 25..29 real
        expect(b.moduleMean[2]).toBeCloseTo(2, 4);
        expect(b.hasGap[0]).toBe(0);
        expect(b.firstIndex[0]).toBe(11);
        expect(b.lastIndex[0]).toBe(20);
        expect(b.firstIndex[9]).toBe(101);
        expect(b.lastIndex[9]).toBe(110);
    });

    it('marks columns containing trip entries', () => {
        const lane = buildLanes(tinyWindow())[0];
        const trips: TripRange[] = [{ unit: 'T', firstEntry: 48, lastEntry: 50 }, { unit: 'Other', firstEntry: 11, lastEntry: 110 }];
        const b = bucketize(lane, 10, domain, trips);
        expect(b.hasTrip[3]).toBe(1);
        expect(b.hasTrip[4]).toBe(0);
        expect(b.hasTrip[0]).toBe(0);
    });

    it('3600 entries draw in 800 columns and keep the worst spike', () => {
        const n = 3600;
        const unitMs = new Array<number>(n).fill(300);
        unitMs[1234] = 9000;
        const w: PerfWindowReply = {
            v: 1, seconds: 60, markers: [], trips: [], truncated: false,
            units: [{ unit: 'Big', unitCrc: 1, hz: 60, firstEntry: 0, count: n, modules: [{ crc: 5, name: 'A' }], unitMs: encodeU16(unitMs), moduleMs: encodeU16(unitMs) }],
        };
        const lane = buildLanes(w)[0];
        const b = bucketize(lane, 800, { t0: lane.t[0] - 1 / 60, t1: lane.t[n - 1] });
        expect(b.tickMax).toHaveLength(800);
        let max = 0;
        for (let c = 0; c < 800; c++) max = Math.max(max, b.tickMax[c]);
        expect(max).toBeCloseTo(90, 3);
    });

    it('entries outside the domain are ignored and empty columns have firstIndex -1', () => {
        const lane = buildLanes(tinyWindow())[0];
        const b = bucketize(lane, 10, { t0: -4, t1: 6 }); // data only covers the last 5 columns' worth up to t=6
        expect(b.firstIndex[0]).toBe(-1);
        expect(b.tickMax[0]).toBe(0);
        expect(b.hasGap[0]).toBe(0);
        expect(b.lastIndex[9]).toBe(60);
    });

    it('degenerate input: zero columns, empty lane', () => {
        const lane = buildLanes(tinyWindow())[0];
        expect(bucketize(lane, 0, domain).columns).toBe(0);
        const empty = buildLanes(emptied(tinyWindow()))[0];
        expect(empty.count).toBe(0);
        expect(bucketize(empty, 5, domain).tickMax).toHaveLength(5);
    });
});

describe('tripSpans', () => {
    const lane = buildLanes(tinyWindow())[0]; // t = index / 10

    it('converts entry ranges to seconds [first/hz, (last+1)/hz] for this unit only', () => {
        const spans = tripSpans(lane, [{ unit: 'T', firstEntry: 48, lastEntry: 50 }, { unit: 'X', firstEntry: 12, lastEntry: 20 }]);
        expect(spans).toHaveLength(1);
        expect(spans[0].t0).toBeCloseTo(4.8, 9);
        expect(spans[0].t1).toBeCloseTo(5.1, 9);
    });

    it('clips to the visible domain and drops spans outside', () => {
        const trips: TripRange[] = [{ unit: 'T', firstEntry: 48, lastEntry: 50 }, { unit: 'T', firstEntry: 100, lastEntry: 105 }];
        const spans = tripSpans(lane, trips, { t0: 4.9, t1: 8 });
        expect(spans).toHaveLength(1);
        expect(spans[0].t0).toBeCloseTo(4.9, 9);
        expect(spans[0].t1).toBeCloseTo(5.1, 9);
    });

    it('no trips gives no spans', () => {
        expect(tripSpans(lane, [])).toEqual([]);
    });
});

describe('edge cases', () => {
    it('null or malformed windows give no lanes', () => {
        expect(buildLanes(null as unknown as PerfWindowReply)).toEqual([]);
        expect(buildLanes({} as PerfWindowReply)).toEqual([]);
    });

    it('unit with no modules: everything is unattributed', () => {
        const lane = buildLanes(tinyWindow({ modules: 0 }))[0];
        expect(lane.modules).toHaveLength(0);
        expect(lane.unattributedMs[0]).toBeCloseTo(lane.tickMs[0], 5);
        expect(lane.overAttributedMs[0]).toBe(0);
    });

    it('count 0 yields an empty lane', () => {
        const l = buildLanes(emptied(tinyWindow()))[0];
        expect(l.count).toBe(0);
        expect(l.maxMs).toBe(0);
    });

    it('count larger than the data is truncated to the decoded length', () => {
        const w = tinyWindow();
        w.units[0] = { ...w.units[0], count: 500 };
        expect(buildLanes(w)[0].count).toBe(100);
    });

    it('entries are by index, never reordered: duplicate units stay separate, firstEntry is the offset origin', () => {
        const w = tinyWindow();
        const second = { ...w.units[0], firstEntry: 5 }; // overlapping / non-monotonic vs the first
        const lanes = buildLanes({ ...w, units: [w.units[0], second] });
        expect(lanes).toHaveLength(2);
        expect(lanes[0].index[0]).toBe(11);
        expect(lanes[1].index[0]).toBe(5);
        expect(lanes[1].index[99]).toBe(104);
        // a later reply with an older firstEntry (game restart) is simply a fresh model
        expect(buildLanes({ ...w, units: [{ ...w.units[0], firstEntry: 0 }] })[0].t[0]).toBe(0);
    });

    it('invalid firstEntry is treated as 0 and invalid hz gives an empty lane', () => {
        const w = tinyWindow();
        expect(buildLanes({ ...w, units: [{ ...w.units[0], firstEntry: NaN }] })[0].index[0]).toBe(0);
        expect(buildLanes({ ...w, units: [{ ...w.units[0], hz: 0 }] })[0].count).toBe(0);
    });

    it('unwrapWindowReply takes the inner object of the bridge envelope', () => {
        const w = tinyWindow();
        expect(unwrapWindowReply({ success: true, result: w })).toBe(w);
        expect(unwrapWindowReply({ success: false, result: w })).toBeNull();
        expect(unwrapWindowReply({ success: true, result: { v: 2 } })).toBeNull();
        expect(unwrapWindowReply(null)).toBeNull();
    });
});

describe('findFrameAt', () => {
    const lane = buildLanes(tinyWindow())[0]; // t 1.1 .. 11.0, absolute index 11..110

    it('returns the absolute index nearest to t', () => {
        expect(findFrameAt(lane, 5.04)).toBe(50);
        expect(findFrameAt(lane, 5.06)).toBe(51);
        expect(findFrameAt(lane, 1.1)).toBe(11);
        expect(findFrameAt(lane, 11.0)).toBe(110);
    });

    it('edges: within one period outside the ends clamps, further is null', () => {
        expect(findFrameAt(lane, 1.05)).toBe(11);
        expect(findFrameAt(lane, 11.05)).toBe(110);
        expect(findFrameAt(lane, 0.5)).toBeNull();
        expect(findFrameAt(lane, 11.5)).toBeNull();
    });

    it('empty lane and NaN give null', () => {
        expect(findFrameAt(buildLanes(emptied(tinyWindow()))[0], 1)).toBeNull();
        expect(findFrameAt(lane, NaN)).toBeNull();
    });
});
