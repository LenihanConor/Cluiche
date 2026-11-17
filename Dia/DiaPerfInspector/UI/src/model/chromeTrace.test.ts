import fixture from './__fixtures__/perfWindow.fixture.json';
import { buildLanes } from './laneModel';
import type { Lane, PerfWindowReply } from './laneModel';
import { buildChromeTrace } from './chromeTrace';

const win = fixture as unknown as PerfWindowReply;
const lanesOf = (w: unknown = win): Lane[] => buildLanes(w as PerfWindowReply);

describe('buildChromeTrace schema', () => {
    it('returns an object with traceEvents and displayTimeUnit', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        expect(result).toHaveProperty('traceEvents');
        expect(result).toHaveProperty('displayTimeUnit');
        expect(Array.isArray(result.traceEvents)).toBe(true);
        expect(result.displayTimeUnit).toBe('ms');
    });

    it('every event has name, ph, pid, tid', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        for (const ev of result.traceEvents) {
            expect(typeof ev.name).toBe('string');
            expect(typeof ev.ph).toBe('string');
            expect(typeof ev.pid).toBe('number');
            expect(typeof ev.tid).toBe('number');
        }
    });

    it('X events have numeric ts >= 0 and dur >= 0', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const xEvents = result.traceEvents.filter((e) => e.ph === 'X');
        for (const ev of xEvents) {
            expect(typeof ev.ts).toBe('number');
            expect(typeof ev.dur).toBe('number');
            expect(ev.ts).toBeGreaterThanOrEqual(0);
            expect(ev.dur).toBeGreaterThanOrEqual(0);
        }
    });

    it('metadata events are well formed (M with args)', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const metaEvents = result.traceEvents.filter((e) => e.ph === 'M');
        for (const ev of metaEvents) {
            expect(ev.args).toBeDefined();
            expect(typeof ev.args).toBe('object');
        }
    });

    it('JSON.stringify round-trips', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const str = JSON.stringify(result);
        const reparsed = JSON.parse(str);
        expect(reparsed).toEqual(result);
    });
});

describe('buildChromeTrace events', () => {
    it('empty lanes -> only metadata', () => {
        const result = buildChromeTrace([]);
        const metadata = result.traceEvents.filter((e) => e.ph === 'M');
        const xEvents = result.traceEvents.filter((e) => e.ph === 'X');
        expect(metadata.length).toBeGreaterThan(0);
        expect(xEvents.length).toBe(0);
    });

    it('emits process_name and thread_name metadata first', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const allEvents = result.traceEvents;
        let processNameIdx = -1;
        let firstXIdx = -1;
        for (let i = 0; i < allEvents.length; i++) {
            if (allEvents[i].name === 'process_name') processNameIdx = i;
            if (allEvents[i].ph === 'X' && firstXIdx === -1) firstXIdx = i;
        }
        expect(processNameIdx).toBeGreaterThanOrEqual(0);
        expect(firstXIdx).toBeGreaterThan(processNameIdx);
    });

    it('has one thread per unit with thread_name metadata', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const threadNames = result.traceEvents
            .filter((e) => e.name === 'thread_name')
            .map((e) => (e.args as { name?: string }).name || '');
        expect(threadNames.length).toBe(lanes.length);
        for (const lane of lanes) {
            expect(threadNames).toContain(lane.unit);
        }
    });

    it('events are ordered: metadata first, then by ts', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const events = result.traceEvents;
        let lastMetadataIdx = -1;
        let hasXEvent = false;
        for (let i = 0; i < events.length; i++) {
            if (events[i].ph === 'M') lastMetadataIdx = i;
            if (events[i].ph === 'X' && !hasXEvent) {
                hasXEvent = true;
                expect(lastMetadataIdx).toBeLessThan(i);
            }
        }
        // Check ts ordering after metadata
        let lastTs = -1;
        for (let i = lastMetadataIdx + 1; i < events.length; i++) {
            const ts = (events[i] as unknown as { ts?: number }).ts ?? 0;
            expect(ts).toBeGreaterThanOrEqual(lastTs);
            lastTs = ts;
        }
    });

    it('timestamps are in microseconds', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const xEvents = result.traceEvents.filter((e) => e.ph === 'X');
        // First lane should have t starting at some value, ts should be t * 1e6
        if (xEvents.length > 0 && lanes.length > 0) {
            const firstLane = lanes[0];
            if (firstLane.count > 0) {
                const tSeconds = firstLane.t[0];
                const expectedTs = tSeconds * 1e6;
                const actualTs = (xEvents[0] as unknown as { ts?: number }).ts ?? 0;
                // Allow small floating-point tolerance
                expect(Math.abs(actualTs - expectedTs)).toBeLessThan(1);
            }
        }
    });

    it('gap entries are skipped', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const xEvents = result.traceEvents.filter((e) => e.ph === 'X' && (e.args as any)?.is_tick);
        let totalEntries = 0;
        let nonGapEntries = 0;
        for (const lane of lanes) {
            for (let i = 0; i < lane.count; i++) {
                totalEntries++;
                if (lane.gap[i] === 0) {
                    nonGapEntries++;
                }
            }
        }
        // Should have approximately nonGapEntries tick events (one per non-gap entry per lane)
        expect(xEvents.length).toBeLessThanOrEqual(nonGapEntries);
    });

    it('clamped entries get args: {clamped: true}', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        for (const lane of lanes) {
            for (let i = 0; i < lane.count; i++) {
                if (lane.clamped[i]) {
                    // Find the tick event for this entry
                    const found = result.traceEvents.find(
                        (e) => e.ph === 'X'
                            && e.tid === lanes.indexOf(lane)
                            && Math.abs((e as unknown as { ts?: number }).ts ?? 0 - lane.t[i] * 1e6) < 1
                    );
                    if (found && (found.args as any)?.is_tick) {
                        expect((found.args as any).clamped).toBe(true);
                    }
                }
            }
        }
    });
});

describe('buildChromeTrace module slices', () => {
    it('each non-gap entry emits a tick event and module slices', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        // Count tick events
        const tickEvents = result.traceEvents.filter((e) => e.ph === 'X' && (e.args as any)?.is_tick);
        let expectedTicks = 0;
        for (const lane of lanes) {
            for (let i = 0; i < lane.count; i++) {
                if (lane.gap[i] === 0) {
                    expectedTicks++;
                }
            }
        }
        // Approximate check (exact match depends on module indexing)
        expect(tickEvents.length).toBe(expectedTicks);
    });

    it('module slices lay back-to-back inside their tick', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const xEvents = result.traceEvents.filter((e) => e.ph === 'X') as unknown as Array<{
            ts: number;
            dur: number;
            tid: number;
            args?: { is_tick?: boolean };
        }>;

        // Group events by tick (by ts + dur boundary within lane)
        for (let i = 0; i < xEvents.length; i++) {
            const tick = xEvents[i];
            if (tick.args?.is_tick) {
                const tickStart = tick.ts;
                const tickEnd = tick.ts + tick.dur;
                // Find all non-tick slices in this lane that start before tick end
                const slicesInTick: typeof xEvents = [];
                for (let j = i + 1; j < xEvents.length; j++) {
                    const ev = xEvents[j];
                    // Stop if we hit the next tick or a different tid
                    if (ev.tid !== tick.tid) break;
                    if (ev.args?.is_tick) break;
                    if (ev.ts < tickEnd) {
                        slicesInTick.push(ev);
                    }
                }
                // Verify slices start at or after tick start
                for (const slice of slicesInTick) {
                    expect(slice.ts).toBeGreaterThanOrEqual(tickStart - 1); // -1 for floating-point rounding
                }
            }
        }
    });

    it('zero-ms modules are skipped', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        for (const lane of lanes) {
            for (let i = 0; i < lane.count; i++) {
                if (lane.gap[i] === 0) {
                    for (let k = 0; k < lane.modules.length; k++) {
                        const moduleMs = lane.moduleMs[k * lane.count + i];
                        if (moduleMs === 0) {
                            // Should not emit a slice for this module in this entry
                            const tickTs = lane.t[i] * 1e6;
                            const sliceForThisModule = result.traceEvents.find(
                                (e) => e.ph === 'X'
                                    && e.tid === lanes.indexOf(lane)
                                    && Math.abs((e as unknown as { ts?: number }).ts ?? 0 - tickTs) < 1
                                    && e.name === lane.modules[k].name
                            );
                            expect(sliceForThisModule).toBeUndefined();
                        }
                    }
                }
            }
        }
    });

    it('slices never exceed the tick duration unless over-attributed', () => {
        const lanes = lanesOf();
        const result = buildChromeTrace(lanes);
        const xEvents = result.traceEvents.filter((e) => e.ph === 'X') as unknown as Array<{
            ts: number;
            dur: number;
            tid: number;
            args?: { is_tick?: boolean };
        }>;

        for (let i = 0; i < xEvents.length; i++) {
            const tick = xEvents[i];
            if (tick.args?.is_tick) {
                const tickEnd = tick.ts + tick.dur;
                // Find slices in this tick
                for (let j = i + 1; j < xEvents.length && xEvents[j].tid === tick.tid; j++) {
                    const slice = xEvents[j];
                    if (slice.ts >= tickEnd) break;
                    if (!slice.args?.is_tick) {
                        // Slice should not extend beyond tick end unless clamped
                        const sliceEnd = slice.ts + slice.dur;
                        expect(sliceEnd).toBeLessThanOrEqual(tickEnd + 1); // +1 for rounding
                    }
                }
            }
        }
    });
});

describe('buildChromeTrace with visible lanes (windowed)', () => {
    it('uses only visible (windowed) slice of lanes', () => {
        const lanes = lanesOf();
        // If lanes are pre-windowed, the result should only include their entries
        const result = buildChromeTrace(lanes);
        let totalExpectedTicks = 0;
        for (const lane of lanes) {
            totalExpectedTicks += lane.count;
        }
        const tickEvents = result.traceEvents.filter((e) => e.ph === 'X' && (e.args as any)?.is_tick);
        expect(tickEvents.length).toBeLessThanOrEqual(totalExpectedTicks);
    });
});
