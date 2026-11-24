import { useMemo, useState } from 'react';
import { buildModuleTable, rowKey, sortGroups } from '../model/moduleTable';
import type { SortKey } from '../model/moduleTable';
import { usePerfTimingStore } from '../usePerfTimingStore';
import { usePauseStore } from '../usePauseStore';
import { useWindowStore } from '../useWindowStore';

const btn = (on: boolean): React.CSSProperties => ({
    background: on ? '#264f78' : '#3a3a3a', border: `1px solid ${on ? '#3a74a8' : '#4a4a4a'}`,
    color: '#d4d4d4', font: 'inherit', padding: '2px 9px', cursor: 'pointer',
});
const cols = '1fr 78px 78px 46px 56px';
const hot = '#f48771';

export function ModuleTable() {
    const timing = usePerfTimingStore((s) => s.timing);
    const lanes = useWindowStore((s) => s.lanes);
    const paused = usePauseStore((s) => s.paused);
    const errors = usePauseStore((s) => s.errors);
    const unsupported = usePauseStore((s) => s.unsupported);
    const toggle = usePauseStore((s) => s.toggle);
    const resumeAll = usePauseStore((s) => s.resumeAll);
    const [sortKey, setSortKey] = useState<SortKey>('p95');
    const [selectedKey, setSelectedKey] = useState<string | null>(null);

    const groups = useMemo(() => {
        const budgets: Record<string, number> = {};
        for (const l of lanes) budgets[l.unit] = l.budgetMs;
        return sortGroups(buildModuleTable(timing, budgets), sortKey);
    }, [timing, lanes, sortKey]);

    return (
        <div data-testid="module-table">
            <div style={{ display: 'flex', gap: 6, alignItems: 'center', marginBottom: 4 }}>
                <span style={{ color: '#808080' }}>Sort</span>
                <button type="button" aria-label="Sort by p95" aria-pressed={sortKey === 'p95'} style={btn(sortKey === 'p95')} onClick={() => setSortKey('p95')}>p95</button>
                <button type="button" aria-label="Sort by name" aria-pressed={sortKey === 'name'} style={btn(sortKey === 'name')} onClick={() => setSortKey('name')}>name</button>
                <span style={{ flex: 1 }} />
                <button type="button" data-testid="resume-all" disabled={unsupported !== null} style={btn(false)} onClick={() => { void resumeAll(); }}>
                    Resume all
                </button>
            </div>
            {unsupported && (
                <div data-testid="pause-unsupported" style={{ color: '#d7ba7d', marginBottom: 4 }}>{unsupported}</div>
            )}
            {groups.length === 0 && <div style={{ color: '#808080' }}>No timing data yet.</div>}
            {groups.map((g) => (
                <div key={g.unit} data-testid="module-unit" data-unit={g.unit} style={{ marginBottom: 8 }}>
                    <div data-testid="module-table-header" style={{ display: 'grid', gridTemplateColumns: cols, gap: 6, color: '#808080', fontSize: 11 }}>
                        <span style={{ color: g.overBudget ? hot : '#9cdcfe', fontWeight: 600 }}>
                            {g.unit} <small>{g.overBudget ? 'over budget' : 'ok'}</small>
                        </span>
                        <span style={{ textAlign: 'right' }}>avg (EWMA)</span>
                        <span style={{ textAlign: 'right' }}>p95</span>
                        <span style={{ textAlign: 'right' }}>share</span>
                        <span />
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: cols, gap: 6, color: '#a0a0a0' }}>
                        <span>unit tick (budget {g.budgetMs.toFixed(1)} ms)</span>
                        <span style={{ textAlign: 'right' }}>{g.tickEwmaMs.toFixed(2)}</span>
                        <span style={{ textAlign: 'right', color: g.overBudget ? hot : undefined }}>{g.tickP95Ms.toFixed(2)}</span>
                        <span /><span />
                    </div>
                    {g.rows.map((r) => {
                        const key = rowKey(r.unit, r.module);
                        const isPaused = paused[key] === true;
                        const err = errors[key];
                        return (
                            <div key={key}>
                                <div
                                    data-testid="module-row"
                                    data-module={r.module}
                                    onClick={() => setSelectedKey(key)}
                                    style={{
                                        display: 'grid', gridTemplateColumns: cols, gap: 6, alignItems: 'center',
                                        background: selectedKey === key ? '#264f78' : undefined, opacity: isPaused ? 0.55 : 1,
                                    }}
                                >
                                    <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }} title={r.module}>{r.module}</span>
                                    <span style={{ textAlign: 'right' }}>{r.ewmaMs.toFixed(2)}</span>
                                    <span style={{ textAlign: 'right', color: r.overBudget ? hot : undefined }}>{r.p95Ms.toFixed(2)}</span>
                                    <span style={{ textAlign: 'right' }}>{Math.round(r.share * 100)}%</span>
                                    <button
                                        type="button"
                                        data-testid={`pause-${r.unit}-${r.module}`}
                                        aria-pressed={isPaused}
                                        disabled={unsupported !== null}
                                        title={unsupported ?? 'Pause this module in the running game (A/B). Released on disconnect.'}
                                        style={btn(isPaused)}
                                        onClick={(e) => { e.stopPropagation(); void toggle(r.unit, r.module); }}
                                    >
                                        {isPaused ? 'paused' : 'on'}
                                    </button>
                                </div>
                                {err && (
                                    <div data-testid={`pause-error-${r.unit}-${r.module}`} style={{ color: hot, fontSize: 11 }}>{err}</div>
                                )}
                            </div>
                        );
                    })}
                </div>
            ))}
            {errors['*'] && <div style={{ color: hot }}>{errors['*']}</div>}
        </div>
    );
}
