import type { LatencyAction } from '../types';

/** The only target perf-live-control.md names: slider drag to visible change, 100 ms. The game carries no targets. */
export const kDefaultLatencyTargetMs = 100;

export type LatencyStatus = 'ok' | 'over' | 'no-samples';

export interface LatencyRow {
    action: string;
    status: LatencyStatus;
    p50: string;
    p95: string;
    target: string;
    samples: number;
    p95Ms: number;
}

export function latencyStatus(a: LatencyAction): LatencyStatus {
    if (a.samples <= 0) return 'no-samples';
    return a.p95Ms > a.targetMs ? 'over' : 'ok';
}

/** One decimal under 100 ms, whole ms from 100 up. */
export function formatMs(ms: number): string {
    return `${ms >= 100 ? ms.toFixed(0) : ms.toFixed(1)} ms`;
}

/** False before the first perf.timing push and while the game lists `latency` as unavailable. */
export function latencyAvailable(unavailable: string[] | undefined): boolean {
    return unavailable !== undefined && !unavailable.includes('latency');
}

const kRank: Record<LatencyStatus, number> = { over: 0, ok: 1, 'no-samples': 2 };

/** Over-target first, then within target, then no samples; p95 descending, then action name. Does not mutate input. */
export function buildLatencyRows(actions: LatencyAction[]): LatencyRow[] {
    return actions
        .map((a): LatencyRow => {
            const status = latencyStatus(a);
            const has = status !== 'no-samples';
            return {
                action: a.action, status, samples: a.samples, p95Ms: has ? a.p95Ms : 0,
                p50: has ? formatMs(a.p50Ms) : '-', p95: has ? formatMs(a.p95Ms) : '-', target: formatMs(a.targetMs),
            };
        })
        .sort((x, y) => kRank[x.status] - kRank[y.status] || y.p95Ms - x.p95Ms || x.action.localeCompare(y.action));
}

/**
 * Maps a `perf.latency` reply (actions[name] = {count, p50Ms, p95Ms, ...}) to typed input. Targets are not on the
 * wire, so every action gets targetMs (default 100). Returns null when the reply has no `actions` object.
 */
export function latencyActionsFromReply(reply: unknown, targetMs: number = kDefaultLatencyTargetMs): LatencyAction[] | null {
    const actions = (reply as { actions?: unknown } | null)?.actions;
    if (!actions || typeof actions !== 'object') return null;
    return Object.entries(actions as Record<string, { count?: number; p50Ms?: number; p95Ms?: number }>).map(([action, s]) => ({
        action, targetMs, p50Ms: s?.p50Ms ?? 0, p95Ms: s?.p95Ms ?? 0, samples: s?.count ?? 0,
    }));
}
