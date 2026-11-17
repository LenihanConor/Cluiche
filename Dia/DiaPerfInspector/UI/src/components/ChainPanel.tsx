import { useMemo } from 'react';
import { buildChain } from '../model/chain';
import type { ChainEdgeRow } from '../model/chain';
import { useChainStore } from '../useChainStore';

const hot = '#f48771';
const edgeColour = '#d7ba7d';

function formatBytes(b: number): string {
    return b >= 1024 ? `${(b / 1024).toFixed(1)} KB` : `${Math.round(b)} B`;
}

function edgeText(e: ChainEdgeRow): string {
    const parts = [`stream ${e.stream}`];
    if (e.bytes !== undefined) parts.push(formatBytes(e.bytes));
    if (e.ratePerSec !== undefined) parts.push(`${Math.round(e.ratePerSec * 10) / 10}/s`);
    if (e.writeCostMs !== undefined) parts.push(`write ${e.writeCostMs.toFixed(1)} ms`);
    if (e.dataAgeMs !== undefined) parts.push(`data age ${e.dataAgeMs.toFixed(0)} ms`);
    return `↓ ${parts.join(' · ')} ↓`;
}

export function ChainPanel({ budgetMs }: { budgetMs: number }) {
    const chain = useChainStore((s) => s.chain);
    const reason = useChainStore((s) => s.reason);
    const view = useMemo(() => (chain ? buildChain(chain, budgetMs) : null), [chain, budgetMs]);

    if (!view) {
        return <div data-testid="chain-panel" style={{ color: '#808080' }}>{`chain unavailable: ${reason ?? 'no chain data from the game yet'}`}</div>;
    }
    if (view.stages.length === 0) {
        return <div data-testid="chain-panel" style={{ color: '#808080' }}>chain has no stages</div>;
    }
    const share = view.shareOfBudget === null
        ? 'budget unknown'
        : `${(view.shareOfBudget * 100).toFixed(0)}% of one budget`;

    return (
        <div data-testid="chain-panel">
            {view.stages.map((s) => (
                <div key={s.id}>
                    <div data-testid="chain-stage" style={{ display: 'grid', gridTemplateColumns: '42px 1fr 52px', gap: 6, padding: '2px 0' }}>
                        <span style={{ color: '#808080' }}>{s.unit}</span>
                        <span>
                            {s.name}
                            <span style={{ display: 'block', height: 3, background: '#333' }}>
                                <span style={{ display: 'block', height: 3, width: `${s.share * 100}%`, background: '#4a90c8' }} />
                            </span>
                        </span>
                        <span style={{ textAlign: 'right', color: hot }}>{s.ms.toFixed(1)}</span>
                    </div>
                    {s.edgesOut.map((e) => (
                        <div key={`${e.stream}:${e.toId}`} data-testid="chain-edge" style={{ color: edgeColour, padding: '2px 0 2px 48px', fontSize: 11 }}>
                            {edgeText(e)}
                        </div>
                    ))}
                </div>
            ))}
            <div data-testid="chain-total" style={{ display: 'flex', borderTop: '1px solid #3c3c3c', marginTop: 5, paddingTop: 5 }}>
                <span>{`chain: "${view.name}"`}</span>
                <b style={{ marginLeft: 'auto' }}>{`${view.totalMs.toFixed(1)} ms · ${share}`}</b>
            </div>
            {view.danglingEdges.length > 0 && (
                <div style={{ color: '#808080', fontSize: 11 }}>{`${view.danglingEdges.length} stream edge(s) skipped: stage missing`}</div>
            )}
        </div>
    );
}
