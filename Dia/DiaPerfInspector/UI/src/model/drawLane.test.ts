import { encodeU16 } from './__fixtures__/makeFixture';
import { bucketize, buildLanes, tripSpans } from './laneModel';
import type { Lane, PerfWindowReply } from './laneModel';
import { drawLane, kModulePalette, kNeutralColour, moduleColour } from './drawLane';
import type { MinimalCtx } from './drawLane';

interface Call { op: string; args: number[]; fill?: string; stroke?: string; dash?: number[] }

function recorder() {
    const calls: Call[] = [];
    const ctx: MinimalCtx = {
        fillStyle: '', strokeStyle: '',
        fillRect(x, y, w, h) { calls.push({ op: 'fillRect', args: [x, y, w, h], fill: String(this.fillStyle) }); },
        beginPath() { calls.push({ op: 'beginPath', args: [] }); },
        moveTo(x, y) { calls.push({ op: 'moveTo', args: [x, y], stroke: String(this.strokeStyle) }); },
        lineTo(x, y) { calls.push({ op: 'lineTo', args: [x, y] }); },
        stroke() { calls.push({ op: 'stroke', args: [], stroke: String(this.strokeStyle) }); },
        setLineDash(d) { calls.push({ op: 'setLineDash', args: [], dash: d }); },
    };
    return { ctx, calls, rects: () => calls.filter((c) => c.op === 'fillRect') };
}

/** One unit, `n` entries from firstEntry. tick/mods in 0.01 ms units (per entry). */
function windowOf(unit: string, hz: number, firstEntry: number, tick: number[], mods: number[][], flags?: number[]): PerfWindowReply {
    const n = tick.length;
    return {
        v: 1, seconds: 10, markers: [], trips: [], truncated: false,
        units: [{
            unit, unitCrc: 1, hz, firstEntry, count: n,
            modules: mods.map((_, k) => ({ crc: 10 + k, name: `M${k}` })),
            unitMs: encodeU16(tick), moduleMs: encodeU16(mods.flat()),
            flags: flags ? btoa(String.fromCharCode(...flags)) : undefined,
        }],
    };
}

const W = 100;
const H = 50;
const domain = { t0: 0, t1: 10 };

function draw(lane: Lane, trips: Parameters<typeof tripSpans>[1] = []) {
    const r = recorder();
    const b = bucketize(lane, W, domain, trips);
    drawLane(r.ctx, lane, b, { width: W, height: H, domain, spans: tripSpans(lane, trips, domain) });
    return r;
}

// hz 10, entries at t = 5.0 (index 50) .. : one entry at 5.0
describe('drawLane', () => {
    it('draws background + one rect per non-empty stack segment', () => {
        // tick 10 ms, modules 4 + 3 ms, unattributed 3 ms. budget 100 ms => yMax 125
        const lane = buildLanes(windowOf('A', 10, 50, [1000], [[400], [300]]))[0];
        const r = draw(lane);
        const rects = r.rects();
        // background + 2 modules + 1 unattributed
        expect(rects).toHaveLength(4);
        expect(rects[0].args).toEqual([0, 0, W, H]);
        expect(rects[3].fill).toBe(kNeutralColour);
        expect(rects[1].fill).toBe(moduleColour(lane.modules[0].colourIndex));
    });

    it('draws the budget line at the right y, dashed', () => {
        // hz 10 => budget 100 ms; yMax = max(125, 10) = 125 => y = H - 100/125*H = 10
        const lane = buildLanes(windowOf('A', 10, 50, [1000], [[400]]))[0];
        const r = draw(lane);
        const mv = r.calls.find((c) => c.op === 'moveTo')!;
        expect(mv.args[1]).toBeCloseTo(10, 6);
        expect(mv.args[0]).toBe(0);
        const ln = r.calls.find((c) => c.op === 'lineTo')!;
        expect(ln.args).toEqual([W, mv.args[1]]);
        expect(r.calls.some((c) => c.op === 'setLineDash' && c.dash!.length === 2)).toBe(true);
    });

    it('scales y to maxMs when a tick exceeds budget x 1.25', () => {
        // tick 200 ms, budget 100 => yMax 200, budget line at y = 50 - 100/200*50 = 25
        const lane = buildLanes(windowOf('A', 10, 50, [20000], [[400]]))[0];
        const r = draw(lane);
        expect(r.calls.find((c) => c.op === 'moveTo')!.args[1]).toBeCloseTo(25, 6);
    });

    it('marks over-budget columns with a red top tick', () => {
        const lane = buildLanes(windowOf('A', 10, 50, [20000], [[400]]))[0];
        const normal = draw(buildLanes(windowOf('A', 10, 50, [1000], [[400]]))[0]);
        const over = draw(lane);
        expect(over.rects().some((c) => c.fill === '#f48771' && c.args[1] === 0)).toBe(true);
        expect(normal.rects().some((c) => c.fill === '#f48771')).toBe(false);
    });

    it('shades trip spans over the right x-range', () => {
        // entries 50..59 (t 5.0..5.9); trip over entries 52..54 => t 5.2..5.5 => x 52..55
        const lane = buildLanes(windowOf('A', 10, 50, new Array(10).fill(1000), [new Array(10).fill(400)]))[0];
        const r = draw(lane, [{ unit: 'A', firstEntry: 52, lastEntry: 54 }]);
        const shade = r.rects().find((c) => c.args[3] === H && c.args[2] !== W)!;
        expect(shade.args[0]).toBeCloseTo(52, 6);
        expect(shade.args[2]).toBeCloseTo(3, 6);
        // and no shade without trips
        const none = draw(lane);
        expect(none.rects().some((c) => c.args[3] === H && c.args[2] !== W)).toBe(false);
    });

    it('draws a gap column as a dim full-height block, no bars', () => {
        const lane = buildLanes(windowOf('A', 10, 50, [0], [[0]], [2]))[0];
        const r = draw(lane);
        const rects = r.rects();
        expect(rects).toHaveLength(2);
        expect(rects[1].args[1]).toBe(0);
        expect(rects[1].args[3]).toBe(H);
        expect(rects[1].fill).not.toBe(kNeutralColour);
    });

    it('draws a clamped marker', () => {
        const lane = buildLanes(windowOf('A', 10, 50, [65535], [[400]]))[0];
        const r = draw(lane);
        expect(r.rects().some((c) => c.fill === '#ffffff' && c.args[1] === 0)).toBe(true);
    });

    it('lanes of different Hz put the same time at the same x', () => {
        const slow = buildLanes(windowOf('S', 30, 150, [500], [[300]]))[0]; // t = 5.0
        const fast = buildLanes(windowOf('F', 60, 300, [500], [[300]]))[0]; // t = 5.0
        const xs = (l: Lane) => draw(l).rects()[1].args[0];
        expect(xs(slow)).toBe(xs(fast));
        expect(xs(slow)).toBe(49); // (5.0 - 0) / 10 * 100 = column 49 (ceil(50)-1)
    });

    it('an empty lane draws only the background and budget line and does not throw', () => {
        const lane = buildLanes(windowOf('A', 10, 0, [], [[]]))[0];
        expect(lane.count).toBe(0);
        let r!: ReturnType<typeof recorder>;
        expect(() => { r = draw(lane); }).not.toThrow();
        expect(r.rects()).toHaveLength(1);
    });

    it('zero width or height draws nothing', () => {
        const lane = buildLanes(windowOf('A', 10, 50, [1000], [[400]]))[0];
        const r = recorder();
        drawLane(r.ctx, lane, bucketize(lane, 0, domain), { width: 0, height: H, domain });
        expect(r.calls).toHaveLength(0);
    });

    it('palette maps every model colour index', () => {
        expect(kModulePalette).toHaveLength(12);
        expect(moduleColour(13)).toBe(kModulePalette[1]);
    });
});
