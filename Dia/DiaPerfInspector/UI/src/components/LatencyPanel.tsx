import { buildLatencyRows } from '../model/latency';
import type { LatencyAction } from '../types';

interface Props {
    actions: LatencyAction[] | null;
    /** Why there is no data (from the perf.latency reply); null when unknown. The perf.timing `unavailable` list is deliberately ignored. */
    reason: string | null;
}

const colour = { ok: '#7fd18b', over: '#f48771', 'no-samples': '#808080' } as const;

export function LatencyPanel({ actions, reason }: Props) {
    if (actions === null) {
        return (
            <div data-testid="latency-panel" style={{ color: '#808080' }}>
                latency unavailable: {reason ?? 'no latency data from the game yet'}
            </div>
        );
    }
    const rows = buildLatencyRows(actions);
    return (
        <div data-testid="latency-panel">
            {rows.length === 0 && <div style={{ color: '#808080' }}>no latency samples yet: drag a slider or run a panel command in the game</div>}
            {rows.map((r) => (
                <div key={r.action} data-testid="latency-row" data-status={r.status}
                    style={{ display: 'grid', gridTemplateColumns: '1fr auto auto auto', gap: 8, padding: '2px 0', borderBottom: '1px solid #2a2a2a' }}>
                    <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{r.action}</span>
                    <span title="p50">{r.p50}</span>
                    <span title="p95" style={{ color: colour[r.status] }}>{r.p95}</span>
                    <span style={{ color: '#808080' }}>{r.status === 'no-samples' ? 'no samples' : `target ${r.target}`}</span>
                </div>
            ))}
        </div>
    );
}
