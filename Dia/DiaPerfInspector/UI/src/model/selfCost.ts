/** SD-PERF-7 attached average budgets (ms per frame) for the telemetry's own cost. */
export const kSelfCostAvgMs = { sim: 0.05, main: 0.1 } as const;

export type SelfCostStatus = 'ok' | 'over' | 'unavailable';

export interface SelfCostView {
    status: SelfCostStatus;
    valueText: string;
    /** Threshold used for the status, ms. */
    thresholdMs: number;
    note: string;
    /** "unavailable" while `suppressed_logs` is listed (or before the first push). */
    suppressedLogs: string;
    /** Entries of the game's `unavailable` list that this panel reports as rows. */
    unavailableRows: string[];
}

const kRowNames = ['suppressed_logs', 'self_ms'];

export function buildSelfCost(selfMs: number | undefined, unavailable: string[] | undefined): SelfCostView {
    const thresholdMs = kSelfCostAvgMs.sim; // the stricter attached average; Main's is looser
    const have = selfMs !== undefined && Number.isFinite(selfMs);
    const list = unavailable ?? [];
    return {
        status: !have ? 'unavailable' : selfMs! >= thresholdMs ? 'over' : 'ok',
        valueText: have ? `${selfMs!.toFixed(3)} ms` : 'unavailable',
        thresholdMs,
        note: `perf.self_ms is one shared gauge, the last writer wins, not a per-unit cost. Compared with the attached average budget (Sim ${kSelfCostAvgMs.sim} ms, Main ${kSelfCostAvgMs.main} ms).`,
        suppressedLogs: unavailable === undefined || list.includes('suppressed_logs') ? 'unavailable' : 'not reported',
        unavailableRows: list.filter((n) => kRowNames.includes(n)),
    };
}
