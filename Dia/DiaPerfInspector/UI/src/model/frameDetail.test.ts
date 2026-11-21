import { parseFrameReply } from './frameDetail';

const okReply = {
    success: true,
    result: {
        v: 1, unit: 'Main', frame: 42, unitMs: 12.5, flags: 1,
        modules: [{ crc: 3, name: 'Zeta', ms: 1.5 }, { crc: 1, name: 'Alpha', ms: 4.25 }],
        sections: { available: false, reason: 'sections need profiling on' },
    },
};

describe('parseFrameReply', () => {
    it('keeps modules in reply (schedule) order', () => {
        const r = parseFrameReply(okReply);
        expect(r.kind).toBe('ok');
        if (r.kind !== 'ok') return;
        expect(r.data.modules.map((m) => m.name)).toEqual(['Zeta', 'Alpha']);
        expect(r.data.unitMs).toBe(12.5);
        expect(r.data.frame).toBe(42);
        expect(r.data.sectionsUnavailableReason).toBe('sections need profiling on');
    });
    it('reads available sections', () => {
        const r = parseFrameReply({ success: true, result: { ...okReply.result, sections: { available: true, sections: [{ name: 's', ms: 2 }] } } });
        if (r.kind !== 'ok') throw new Error('expected ok');
        expect(r.data.sectionsUnavailableReason).toBeNull();
        expect(r.data.sections).toEqual([{ name: 's', ms: 2 }]);
    });
    it('turns an {error} reply into an error', () => {
        const r = parseFrameReply({ success: true, result: { error: 'frame outside retained range' } });
        expect(r).toEqual({ kind: 'error', message: 'frame outside retained range' });
    });
    it('turns a failed relay into an error', () => {
        const r = parseFrameReply({ success: false, result: { error: 'query timeout: x' } });
        expect(r.kind).toBe('error');
    });
    it('does not throw on garbage', () => {
        for (const g of [null, undefined, 7, {}, { success: true }, { success: true, result: { v: 1 } }]) {
            expect(parseFrameReply(g).kind).toBe('error');
        }
    });
    it('defaults the sections reason when missing', () => {
        const r = parseFrameReply({ success: true, result: { v: 1, unit: 'M', frame: 1, unitMs: 1, modules: [] } });
        if (r.kind !== 'ok') throw new Error('expected ok');
        expect(r.data.sectionsUnavailableReason).toBe('not reported by the game');
    });
});
