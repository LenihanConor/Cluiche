import { reconcileSelection, selectionRect, timeAtX } from './selection';
import { buildLanes } from './laneModel';
import { encodeU16 } from './__fixtures__/makeFixture';

function lane(unit: string, firstEntry: number, count = 10, hz = 10) {
    return buildLanes({
        v: 1, seconds: 60, markers: [], trips: [], truncated: false,
        units: [{
            unit, unitCrc: 1, hz, firstEntry, count, modules: [{ crc: 5, name: 'A' }],
            unitMs: encodeU16(new Array(count).fill(500)), moduleMs: encodeU16(new Array(count).fill(200)),
        }],
    })[0];
}

describe('timeAtX', () => {
    it('maps pixels linearly over the domain', () => {
        expect(timeAtX(0, 100, { t0: 10, t1: 20 })).toBe(10);
        expect(timeAtX(50, 100, { t0: 10, t1: 20 })).toBe(15);
        expect(timeAtX(100, 100, { t0: 10, t1: 20 })).toBe(20);
    });
    it('returns NaN for a zero width', () => {
        expect(Number.isNaN(timeAtX(5, 0, { t0: 0, t1: 1 }))).toBe(true);
    });
});

describe('selectionRect', () => {
    const l = lane('Main', 100); // entries 100..109, t = 10.0..10.9
    it('covers the entry period ending at the entry time', () => {
        const r = selectionRect(l, 105, { t0: 10, t1: 11 }, 100);
        expect(r).not.toBeNull();
        expect(r!.left).toBeCloseTo(40);
        expect(r!.width).toBeCloseTo(10);
    });
    it('is at least 3 px wide', () => {
        const r = selectionRect(l, 105, { t0: 10.4, t1: 110 }, 100);
        expect(r!.width).toBeGreaterThanOrEqual(3);
    });
    it('is null when the entry is outside the domain or width is 0', () => {
        expect(selectionRect(l, 105, { t0: 0, t1: 5 }, 100)).toBeNull();
        expect(selectionRect(l, 150, { t0: 10, t1: 11 }, 100)).toBeNull();
        expect(selectionRect(l, 105, { t0: 10, t1: 11 }, 0)).toBeNull();
    });
});

describe('reconcileSelection', () => {
    it('is ok while the entry is retained', () => {
        expect(reconcileSelection({ unit: 'Main', index: 105 }, [lane('Main', 100)])).toBeNull();
    });
    it('is stale when the unit disappears', () => {
        expect(reconcileSelection({ unit: 'Sim', index: 105 }, [lane('Main', 100)])).toMatch(/Sim/);
    });
    it('is stale when the entry fell out of the retained window', () => {
        expect(reconcileSelection({ unit: 'Main', index: 50 }, [lane('Main', 100)])).toMatch(/retained/);
    });
    it('is not stale when the lane is momentarily empty', () => {
        expect(reconcileSelection({ unit: 'Main', index: 50 }, [lane('Main', 100, 0)])).toBeNull();
    });
    it('null selection is never stale', () => {
        expect(reconcileSelection(null, [])).toBeNull();
    });
});
