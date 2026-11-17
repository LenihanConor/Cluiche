// latency.fixture.json is SYNTHETIC (not recorded from a game). Action names follow the game's
// "<domain>.<command>" form; the 100 ms target is the only target perf-live-control.md names.
import fixture from './__fixtures__/latency.fixture.json';
import { buildLatencyRows, formatMs, latencyStatus, latencyAvailable, latencyActionsFromReply, kDefaultLatencyTargetMs } from './latency';
import type { LatencyAction } from '../types';

const a = (over: Partial<LatencyAction> = {}): LatencyAction =>
    ({ action: 'X.y', targetMs: 100, p50Ms: 10, p95Ms: 50, samples: 40, ...over });

describe('latencyStatus', () => {
    it('is ok when p95 is at or under target', () => {
        expect(latencyStatus(a({ p95Ms: 100 }))).toBe('ok');
        expect(latencyStatus(a({ p95Ms: 99.9 }))).toBe('ok');
    });
    it('is over when p95 exceeds target', () => expect(latencyStatus(a({ p95Ms: 100.1 }))).toBe('over'));
    it('is no-samples when samples is 0, whatever the numbers', () =>
        expect(latencyStatus(a({ samples: 0, p95Ms: 500 }))).toBe('no-samples'));
});

describe('buildLatencyRows', () => {
    it('classifies the fixture and sorts over first, then ok by p95 desc, then no-samples', () => {
        const rows = buildLatencyRows(fixture as LatencyAction[]);
        expect(rows.map((r) => [r.action, r.status])).toEqual([
            ['TerrainDomain.loadPreset', 'over'],
            ['TerrainDomain.setSlider', 'ok'],
            ['WeatherDomain.setSeason', 'ok'],
            ['TerrainDomain.regenerate', 'no-samples'],
        ]);
    });
    it('formats p50, p95 and target; no-samples shows dashes', () => {
        const rows = buildLatencyRows(fixture as LatencyAction[]);
        expect(rows[0]).toMatchObject({ p50: '90.0 ms', p95: '163 ms', target: '100 ms' });
        expect(rows[3]).toMatchObject({ p50: '-', p95: '-' });
    });
    it('does not mutate its input', () => {
        const input = [a({ action: 'b', p95Ms: 1 }), a({ action: 'a', p95Ms: 200 })];
        buildLatencyRows(input);
        expect(input[0].action).toBe('b');
    });
    it('breaks ties by action name', () => {
        const rows = buildLatencyRows([a({ action: 'b' }), a({ action: 'a' })]);
        expect(rows.map((r) => r.action)).toEqual(['a', 'b']);
    });
});

describe('formatMs / latencyAvailable', () => {
    it('formats with one decimal under 100 and none above', () => {
        expect(formatMs(4.26)).toBe('4.3 ms');
        expect(formatMs(100)).toBe('100 ms');
        expect(formatMs(163.4)).toBe('163 ms');
    });
    it('is unavailable before the first push or while listed', () => {
        expect(latencyAvailable(undefined)).toBe(false);
        expect(latencyAvailable(['latency'])).toBe(false);
        expect(latencyAvailable(['trips'])).toBe(true);
    });
});

describe('latencyActionsFromReply', () => {
    it('maps the perf.latency reply shape, applying the default target', () => {
        const out = latencyActionsFromReply({ actions: { 'D.c': { count: 3, p50Ms: 1, p95Ms: 2, minMs: 0, maxMs: 3 } } });
        expect(out).toEqual([{ action: 'D.c', targetMs: kDefaultLatencyTargetMs, p50Ms: 1, p95Ms: 2, samples: 3 }]);
    });
    it('returns null for a malformed reply', () => {
        expect(latencyActionsFromReply(null)).toBeNull();
        expect(latencyActionsFromReply({})).toBeNull();
    });
});
