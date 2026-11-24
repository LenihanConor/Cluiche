import type { ChainData, ChainEdgeData } from '../types';

export type ChainEdgeRow = ChainEdgeData;

export interface ChainStageRow {
    id: string;
    unit: string;
    name: string;
    ms: number;
    /** ms / chain total; 0 when the total is 0. */
    share: number;
    /** True when this stage runs on a different unit than the previous stage. */
    unitChange: boolean;
    /** Edges whose source is this stage and whose target is also present. */
    edgesOut: ChainEdgeRow[];
}

export interface ChainUnitTotal { unit: string; ms: number }

export interface ChainView {
    name: string;
    stages: ChainStageRow[];
    units: ChainUnitTotal[];
    totalMs: number;
    budgetMs: number;
    /** total / budget; null when the budget is not positive. */
    shareOfBudget: number | null;
    /** Edges naming a stage that is not in the chain; they are not drawn. */
    danglingEdges: ChainEdgeRow[];
}

export function buildChain(data: ChainData, budgetMs: number): ChainView {
    const order = data.stages
        .map((s, i) => ({ s, i }))
        .sort((a, b) => a.s.startMs - b.s.startMs || a.i - b.i)
        .map((x) => x.s);
    const ids = new Set(order.map((s) => s.id));
    const edgesBySource = new Map<string, ChainEdgeRow[]>();
    const danglingEdges: ChainEdgeRow[] = [];
    for (const e of data.edges) {
        if (!ids.has(e.fromId) || !ids.has(e.toId)) { danglingEdges.push({ ...e }); continue; }
        const list = edgesBySource.get(e.fromId) ?? [];
        list.push({ ...e });
        edgesBySource.set(e.fromId, list);
    }
    const mss = order.map((s) => Math.max(0, s.endMs - s.startMs));
    const totalMs = mss.reduce((a, b) => a + b, 0);
    const units: ChainUnitTotal[] = [];
    const stages = order.map((s, i): ChainStageRow => {
        let u = units.find((x) => x.unit === s.unit);
        if (!u) { u = { unit: s.unit, ms: 0 }; units.push(u); }
        u.ms += mss[i];
        return {
            id: s.id, unit: s.unit, name: s.name, ms: mss[i],
            share: totalMs > 0 ? mss[i] / totalMs : 0,
            unitChange: i > 0 && order[i - 1].unit !== s.unit,
            edgesOut: edgesBySource.get(s.id) ?? [],
        };
    });
    return {
        name: data.name, stages, units, totalMs, budgetMs,
        shareOfBudget: budgetMs > 0 ? totalMs / budgetMs : null,
        danglingEdges,
    };
}
