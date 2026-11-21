import { buildModuleTable, rowKey, resolveSelection, sortGroups } from './moduleTable';
import type { PerfTimingPush } from '../types';

function push(units: PerfTimingPush['units'], scale = 0.01): PerfTimingPush {
    return { scale, windowSec: 10, units, unavailable: [] };
}

const main = {
    unit: 'Main', hz: 30, samples: 300, tick: [2000, 3000] as [number, number],
    modules: ['A', 'B', 'C'], ewma: [500, 1000, 250], p95: [900, 1500, 400],
};
const sim = {
    unit: 'Sim', hz: 60, samples: 600, tick: [800, 2000] as [number, number],
    modules: ['X'], ewma: [400], p95: [1800],
};

describe('buildModuleTable', () => {
    it('converts fixed point with scale and computes share of the unit tick', () => {
        const g = buildModuleTable(push([main]), {});
        expect(g).toHaveLength(1);
        expect(g[0].tickEwmaMs).toBeCloseTo(20);
        expect(g[0].tickP95Ms).toBeCloseTo(30);
        const rows = g[0].rows;
        expect(rows.map((r) => r.module)).toEqual(['A', 'B', 'C']);
        expect(rows[0].ewmaMs).toBeCloseTo(5);
        expect(rows[0].p95Ms).toBeCloseTo(9);
        expect(rows[0].share).toBeCloseTo(0.25);
        expect(rows[1].share).toBeCloseTo(0.5);
    });

    it('honours a different scale', () => {
        const g = buildModuleTable(push([{ ...main, tick: [2, 3], ewma: [1, 1, 1], p95: [1, 1, 1] }], 1), {});
        expect(g[0].tickEwmaMs).toBe(2);
        expect(g[0].rows[0].ewmaMs).toBe(1);
    });

    it('handles units with different module sets, in unit order', () => {
        const g = buildModuleTable(push([main, sim]), {});
        expect(g.map((x) => x.unit)).toEqual(['Main', 'Sim']);
        expect(g[0].rows).toHaveLength(3);
        expect(g[1].rows.map((r) => r.module)).toEqual(['X']);
    });

    it('flags over budget against the lane budget, falling back to 1000/hz', () => {
        const g = buildModuleTable(push([main, sim]), { Main: 8 });
        expect(g[0].budgetMs).toBe(8);
        expect(g[0].rows.map((r) => r.overBudget)).toEqual([true, true, false]); // p95 9, 15, 4 vs 8
        expect(g[0].overBudget).toBe(true); // tick p95 30 > 8
        expect(g[1].budgetMs).toBeCloseTo(1000 / 60);
        expect(g[1].overBudget).toBe(true); // 20 > 16.7
    });

    it('returns no groups for null timing and tolerates empty/ragged units', () => {
        expect(buildModuleTable(null, {})).toEqual([]);
        const g = buildModuleTable(push([
            { unit: 'E', hz: 30, samples: 0, tick: [0, 0], modules: [], ewma: [], p95: [] },
            { unit: 'R', hz: 30, samples: 5, tick: [100, 200], modules: ['A', 'B'], ewma: [50], p95: [] },
        ]), {});
        expect(g[0].rows).toEqual([]);
        expect(g[1].rows).toHaveLength(2);
        expect(g[1].rows[1].ewmaMs).toBe(0);
        expect(g[1].rows[0].p95Ms).toBe(0);
    });

    it('share is 0 when the unit tick is 0', () => {
        const g = buildModuleTable(push([{ ...main, tick: [0, 0] }]), {});
        expect(g[0].rows[0].share).toBe(0);
    });
});

describe('sortGroups', () => {
    const groups = () => buildModuleTable(push([{
        ...main, modules: ['B', 'A', 'C', 'D'], ewma: [1, 2, 3, 4], p95: [500, 900, 500, 500],
    }]), {});

    it('sorts by p95 descending with stable ties (schedule order kept)', () => {
        const s = sortGroups(groups(), 'p95');
        expect(s[0].rows.map((r) => r.module)).toEqual(['A', 'B', 'C', 'D']);
    });

    it('sorts by name and does not mutate the input', () => {
        const g = groups();
        const s = sortGroups(g, 'name');
        expect(s[0].rows.map((r) => r.module)).toEqual(['A', 'B', 'C', 'D']);
        expect(g[0].rows.map((r) => r.module)).toEqual(['B', 'A', 'C', 'D']);
    });

    it('schedule sort keeps the order given', () => {
        expect(sortGroups(groups(), 'schedule')[0].rows.map((r) => r.module)).toEqual(['B', 'A', 'C', 'D']);
    });
});

describe('selection', () => {
    it('resolves a key to a row and returns null when it disappeared', () => {
        const g = buildModuleTable(push([main]), {});
        expect(resolveSelection(g, rowKey('Main', 'B'))?.module).toBe('B');
        expect(resolveSelection(g, rowKey('Main', 'Z'))).toBeNull();
        expect(resolveSelection(g, null)).toBeNull();
    });
});
