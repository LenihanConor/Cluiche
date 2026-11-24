import { parseChainReply } from './chainReply';
import fixture from './__fixtures__/chain.fixture.json';

const stage = { id: '10', unit: 'Sim', name: 'DomainStateSnapshotPublisher.Update', startMs: 1000, endMs: 1007 };

describe('parseChainReply', () => {
    it('reads an available chain, whether wrapped in data or not', () => {
        const body = { available: true, ...fixture };
        for (const result of [{ success: true, data: body }, body]) {
            const r = parseChainReply({ success: true, result });
            expect(r.reason).toBeNull();
            expect(r.chain?.stages).toHaveLength(4);
            expect(r.chain?.edges).toHaveLength(1);
            expect(r.chain?.edges[0].bytes).toBe(6656);
        }
    });

    it('keeps the games reason for an unavailable reply and leaves the chain null', () => {
        const r = parseChainReply({ success: true, result: { success: true, data: { available: false, reason: 'no spans recorded (is tracing on?)' } } });
        expect(r.chain).toBeNull();
        expect(r.reason).toBe('no spans recorded (is tracing on?)');
    });

    it('uses a fallback reason when unavailable has none', () => {
        expect(parseChainReply({ success: true, result: { available: false } }).reason).toBeTruthy();
    });

    it('treats a failed query as unavailable with the error text', () => {
        const r = parseChainReply({ success: false, result: { error: 'query timeout: perf_inspector.query_chain' } });
        expect(r.chain).toBeNull();
        expect(r.reason).toBe('query timeout: perf_inspector.query_chain');
    });

    it('does not throw on malformed replies', () => {
        const junk: unknown[] = [
            null, undefined, 5, 'x', [], {}, { success: true }, { success: true, result: null }, { success: true, result: 'x' },
            { success: true, result: { available: true } },
            { success: true, result: { available: true, stages: 'nope' } },
            { success: true, result: { available: true, stages: [null, 3, {}], edges: 'bad' } },
        ];
        for (const j of junk) {
            const r = parseChainReply(j);
            expect(r.chain === null || Array.isArray(r.chain.stages)).toBe(true);
            if (r.chain === null) expect(r.reason).toBeTruthy();
        }
    });

    it('drops malformed stages and edges but keeps the good ones', () => {
        const r = parseChainReply({
            success: true,
            result: { available: true, name: 'n', stages: [stage, { id: 5 }, { ...stage, id: '11', startMs: NaN }], edges: [{ stream: 's' }, { stream: 's', fromId: '10', toId: '11', bytes: 'x', dataAgeMs: 3 }] },
        });
        expect(r.chain?.stages.map((s) => s.id)).toEqual(['10']);
        expect(r.chain?.edges).toEqual([{ stream: 's', fromId: '10', toId: '11', dataAgeMs: 3 }]);
    });
});
