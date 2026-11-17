// NOTE: __fixtures__/chain.fixture.json is SYNTHETIC. It carries the fields the engine emits per
// trace-linked-streams.md (span id/time/unit, flow link, stream.<id>.edge.* metrics) with realistic numbers.
// It was NOT recorded from a live game (no chain wire exists yet).
import fixture from './__fixtures__/chain.fixture.json';
import { buildChain } from './chain';
import type { ChainData } from '../types';

const data = fixture as ChainData;

describe('buildChain (S-1 card fixture)', () => {
    const v = buildChain(data, 33.3);
    it('orders stages and computes ms, total and share of budget', () => {
        expect(v.stages[0].name).toBe('IDebugDomain.GetJSONState');
        expect(v.stages.map((s) => +s.ms.toFixed(1))).toEqual([4.2, 2.5, 2.6, 2.2]);
        expect(v.totalMs).toBeCloseTo(11.5, 6);
        expect(v.shareOfBudget).toBeCloseTo(11.5 / 33.3, 6);
    });
    it('attaches the stream edge after its source stage', () => {
        expect(v.stages[1].edgesOut).toHaveLength(1);
        expect(v.stages[1].edgesOut[0]).toMatchObject({ stream: 'DomainStateSnapshot1', bytes: 6656, ratePerSec: 30, dataAgeMs: 12, writeCostMs: 0.2 });
        expect(v.stages[0].edgesOut).toHaveLength(0);
    });
    it('groups by unit and marks unit changes', () => {
        expect(v.units.map((u) => u.unit)).toEqual(['Sim', 'Main']);
        expect(v.units[0].ms).toBeCloseTo(6.7, 6);
        expect(v.stages.map((s) => s.unitChange)).toEqual([false, false, true, false]);
    });
    it('stage shares sum to 1', () => {
        expect(v.stages.reduce((a, s) => a + s.share, 0)).toBeCloseTo(1, 9);
    });
});

describe('buildChain edge cases', () => {
    it('sorts out-of-order input by start', () => {
        const v = buildChain({ ...data, stages: [...data.stages].reverse() }, 33.3);
        expect(v.stages.map((s) => s.id)).toEqual(['1001', '1002', '2001', '2002']);
    });
    it('keeps edges with a missing stage out of the rows and reports them', () => {
        const v = buildChain({ ...data, stages: data.stages.filter((s) => s.id !== '2001') }, 33.3);
        expect(v.stages.flatMap((s) => s.edgesOut)).toHaveLength(0);
        expect(v.danglingEdges).toHaveLength(1);
        expect(v.stages).toHaveLength(3);
    });
    it('zero budget gives null share, not Infinity', () => {
        expect(buildChain(data, 0).shareOfBudget).toBeNull();
        expect(buildChain(data, -1).shareOfBudget).toBeNull();
    });
    it('empty chain is zero without NaN', () => {
        const v = buildChain({ name: 'x', stages: [], edges: [] }, 33.3);
        expect(v.stages).toEqual([]);
        expect(v.totalMs).toBe(0);
        expect(v.shareOfBudget).toBe(0);
    });
    it('zero-length stages give 0 shares and clamp negative durations', () => {
        const v = buildChain({ name: 'x', edges: [], stages: [{ id: 'a', unit: 'Sim', name: 'a', startMs: 5, endMs: 5 }, { id: 'b', unit: 'Sim', name: 'b', startMs: 6, endMs: 5 }] }, 10);
        expect(v.totalMs).toBe(0);
        expect(v.stages.map((s) => s.share)).toEqual([0, 0]);
        expect(v.stages[1].ms).toBe(0);
    });
});
