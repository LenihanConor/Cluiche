import { buildSelfCost, kSelfCostAvgMs } from './selfCost';

describe('buildSelfCost', () => {
    it('is ok below the attached average threshold', () => {
        const v = buildSelfCost(0.04, ['latency']);
        expect(v.status).toBe('ok');
        expect(v.valueText).toBe('0.040 ms');
    });
    it('is over at or above the threshold (Sim 0.05)', () => {
        expect(buildSelfCost(0.05, []).status).toBe('over');
        expect(buildSelfCost(0.2, []).status).toBe('over');
        expect(kSelfCostAvgMs).toEqual({ sim: 0.05, main: 0.1 });
    });
    it('is unavailable when selfMs is missing', () => {
        const v = buildSelfCost(undefined, ['self_ms']);
        expect(v.status).toBe('unavailable');
        expect(v.valueText).toBe('unavailable');
    });
    it('states that the gauge is shared, last writer', () => {
        expect(buildSelfCost(0.01, []).note).toMatch(/last writer/);
    });
    it('suppressed log lines is unavailable while listed, and before the first push', () => {
        expect(buildSelfCost(0.01, ['suppressed_logs']).suppressedLogs).toBe('unavailable');
        expect(buildSelfCost(0.01, undefined).suppressedLogs).toBe('unavailable');
        expect(buildSelfCost(0.01, []).suppressedLogs).toBe('not reported');
    });
    it('reports the other unavailable entries it knows', () => {
        expect(buildSelfCost(0.01, ['trips', 'suppressed_logs', 'self_ms']).unavailableRows).toEqual(['suppressed_logs', 'self_ms']);
    });
});
