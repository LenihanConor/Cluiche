// Pure module-table model: merges perf.timing units into rows. No React or DOM imports.
//
// The wire carries only EWMA and p95 (no p50), in fixed point; `scale` converts to ms (0.01 today).

import type { PerfTimingPush } from '../types';

export interface ModuleRow {
    unit: string;
    module: string;
    ewmaMs: number;
    p95Ms: number;
    /** ewmaMs / unit tick EWMA, 0 when the tick is 0. Can exceed 1 when modules overlap. */
    share: number;
    /** The module's p95 alone exceeds the unit's frame budget. */
    overBudget: boolean;
}

export interface UnitGroup {
    unit: string;
    hz: number;
    samples: number;
    tickEwmaMs: number;
    tickP95Ms: number;
    budgetMs: number;
    /** Unit tick p95 exceeds the budget. */
    overBudget: boolean;
    rows: ModuleRow[];
}

export type SortKey = 'schedule' | 'p95' | 'name';

export const rowKey = (unit: string, module: string): string => `${unit}/${module}`;

const num = (v: unknown): number => (typeof v === 'number' && Number.isFinite(v) ? v : 0);

/**
 * One group per unit, in wire order; rows in wire (schedule) order. `budgets` maps unit name to the
 * lane budget in ms; units without an entry fall back to 1000 / hz.
 */
export function buildModuleTable(timing: PerfTimingPush | null, budgets: Record<string, number>): UnitGroup[] {
    if (!timing || !Array.isArray(timing.units)) return [];
    const scale = Number.isFinite(timing.scale) && timing.scale > 0 ? timing.scale : 0.01;
    return timing.units.filter((u) => !!u && typeof u === 'object').map((u) => {
        const tickEwmaMs = num(u.tick?.[0]) * scale;
        const tickP95Ms = num(u.tick?.[1]) * scale;
        const override = budgets[u.unit];
        const budgetMs = override !== undefined ? override : u.hz > 0 ? 1000 / u.hz : 0;
        const names = Array.isArray(u.modules) ? u.modules.filter((m) => typeof m === 'string') : [];
        const rows: ModuleRow[] = names.map((module, i) => {
            const ewmaMs = num(u.ewma?.[i]) * scale;
            const p95Ms = num(u.p95?.[i]) * scale;
            return {
                unit: u.unit, module, ewmaMs, p95Ms,
                share: tickEwmaMs > 0 ? ewmaMs / tickEwmaMs : 0,
                overBudget: budgetMs > 0 && p95Ms > budgetMs,
            };
        });
        return {
            unit: u.unit, hz: u.hz, samples: u.samples, tickEwmaMs, tickP95Ms, budgetMs,
            overBudget: budgetMs > 0 && tickP95Ms > budgetMs, rows,
        };
    });
}

/** New groups with rows sorted (stable: ties keep schedule order). Does not mutate the input. */
export function sortGroups(groups: UnitGroup[], key: SortKey): UnitGroup[] {
    return groups.map((g) => {
        const rows = g.rows.map((r, i) => ({ r, i }));
        if (key === 'p95') rows.sort((a, b) => b.r.p95Ms - a.r.p95Ms || a.i - b.i);
        else if (key === 'name') rows.sort((a, b) => (a.r.module < b.r.module ? -1 : a.r.module > b.r.module ? 1 : a.i - b.i));
        return { ...g, rows: rows.map((x) => x.r) };
    });
}

/** The selected row, or null when no key is set or the module disappeared. */
export function resolveSelection(groups: UnitGroup[], key: string | null): ModuleRow | null {
    if (key === null) return null;
    for (const g of groups) for (const r of g.rows) if (rowKey(r.unit, r.module) === key) return r;
    return null;
}
