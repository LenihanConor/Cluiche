import { buildSelfCost } from '../model/selfCost';

interface Props {
    selfMs: number | undefined;
    /** The `unavailable` list of the latest perf.timing push; undefined before the first push. */
    unavailable: string[] | undefined;
}

const colour = { ok: '#7fd18b', over: '#f48771', unavailable: '#808080' } as const;

export function SelfCostPanel({ selfMs, unavailable }: Props) {
    const v = buildSelfCost(selfMs, unavailable);
    return (
        <div data-testid="selfcost-panel">
            <div style={{ display: 'flex', gap: 8 }}>
                <span>perf.self_ms</span>
                <b data-testid="selfcost-status" data-status={v.status} style={{ marginLeft: 'auto', fontWeight: 400, color: colour[v.status] }}>
                    {v.valueText}{v.status !== 'unavailable' ? (v.status === 'over' ? ' (over budget)' : ' (within budget)') : ''}
                </b>
            </div>
            <div data-testid="selfcost-suppressed" style={{ display: 'flex', gap: 8 }}>
                <span>suppressed log lines</span>
                <span style={{ marginLeft: 'auto', color: '#808080' }}>{v.suppressedLogs}</span>
            </div>
            <div style={{ color: '#808080', fontSize: 11, marginTop: 4 }}>{v.note}</div>
        </div>
    );
}
