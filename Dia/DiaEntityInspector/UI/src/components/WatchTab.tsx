import { CSSProperties, useState } from 'react';
import { theme, buttonStyle } from '@dia/editor-ui';
import type { EntityEntry, WatchItem } from '../types';

interface WatchTabProps {
    watchItems: WatchItem[];
    entities: EntityEntry[];
    onWatchAdd: (e: string, c: string, f: string) => void;
    onWatchRemove: (index: number) => void;
}

function deltaSymbol(d: string | undefined): string {
    if (d === 'up') return '▲';
    if (d === 'dn') return '▼';
    return '—';
}

function deltaColor(d: string | undefined): string {
    if (d === 'up') return '#89d185';
    if (d === 'dn') return '#f44747';
    return '#2d2d2d';
}

export function WatchTab({ watchItems, entities, onWatchAdd, onWatchRemove }: WatchTabProps) {
    const [selEntity, setSelEntity] = useState('');
    const [selComp, setSelComp] = useState('');
    const [selField, setSelField] = useState('');

    const selectedEntityObj = entities.find((e) => e.n === selEntity);
    const comps = selectedEntityObj?.components ?? [];
    const selectedCompObj = comps.find((c) => c.name === selComp);
    const fields = selectedCompObj?.fields ?? [];

    const canAdd = selEntity !== '' && selComp !== '' && selField !== '';

    const handleAdd = () => {
        if (!canAdd) return;
        onWatchAdd(selEntity, selComp, selField);
        setSelEntity('');
        setSelComp('');
        setSelField('');
    };

    const selectStyle: CSSProperties = {
        background: theme.bgInput,
        border: `1px solid ${theme.border}`,
        borderRadius: 3,
        color: theme.text,
        padding: '3px 6px',
        fontSize: 11,
        outline: 'none',
        flex: 1,
        cursor: 'pointer',
    };

    const thStyle: CSSProperties = {
        background: theme.bgPanel,
        color: theme.textMuted,
        fontSize: 10,
        textTransform: 'uppercase',
        letterSpacing: 0.4,
        padding: '4px 8px',
        textAlign: 'left',
        borderBottom: `1px solid ${theme.border}`,
        position: 'sticky',
        top: 0,
        zIndex: 1,
    };

    const tdStyle: CSSProperties = {
        padding: '3px 8px',
        borderBottom: `1px solid ${theme.borderMuted}`,
        fontSize: 11,
        height: 28,
        verticalAlign: 'middle',
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden' }} data-testid="watch-tab">
            <div style={{ flex: 1, overflowY: 'auto' }}>
                <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                    <thead>
                        <tr>
                            <th style={{ ...thStyle, width: 100 }}>Entity</th>
                            <th style={{ ...thStyle, width: 110 }}>Component</th>
                            <th style={{ ...thStyle, width: 100 }}>Field</th>
                            <th style={thStyle}>Value</th>
                            <th style={{ ...thStyle, width: 50 }}>Delta</th>
                            <th style={{ ...thStyle, width: 24 }}></th>
                        </tr>
                    </thead>
                    <tbody>
                        {watchItems.map((w, i) => (
                            <tr key={i} data-testid={`watch-row-${i}`}>
                                <td style={{ ...tdStyle, color: theme.text }}>{w.e}</td>
                                <td style={{ ...tdStyle, color: theme.textMuted }}>{w.c}</td>
                                <td style={{ ...tdStyle, color: theme.textMuted, fontFamily: 'monospace' }}>{w.f}</td>
                                <td style={{ ...tdStyle, fontFamily: 'monospace', color: '#89d185' }}>{w.v ?? ''}</td>
                                <td style={{ ...tdStyle, color: deltaColor(w.d), fontFamily: 'monospace', fontSize: 10 }}>
                                    {deltaSymbol(w.d)}
                                </td>
                                <td style={{ ...tdStyle, textAlign: 'center', cursor: 'pointer', color: theme.borderMuted }}>
                                    <span
                                        data-testid={`watch-remove-${i}`}
                                        onClick={() => onWatchRemove(i)}
                                        title="Remove watch"
                                        style={{ cursor: 'pointer' }}
                                    >
                                        ✕
                                    </span>
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>

            {/* Add watch row */}
            <div style={{ display: 'flex', gap: 5, padding: '5px 8px', background: theme.bg, borderTop: `1px solid ${theme.border}`, flexShrink: 0 }} data-testid="watch-add-row">
                <select
                    style={selectStyle}
                    value={selEntity}
                    onChange={(e) => { setSelEntity(e.target.value); setSelComp(''); setSelField(''); }}
                    data-testid="watch-entity-select"
                >
                    <option value="">Entity...</option>
                    {entities.map((e) => (
                        <option key={e.i} value={e.n}>{e.n}</option>
                    ))}
                </select>
                <select
                    style={selectStyle}
                    value={selComp}
                    onChange={(e) => { setSelComp(e.target.value); setSelField(''); }}
                    data-testid="watch-comp-select"
                >
                    <option value="">Component...</option>
                    {comps.map((c) => (
                        <option key={c.name} value={c.name}>{c.name}</option>
                    ))}
                </select>
                <select
                    style={selectStyle}
                    value={selField}
                    onChange={(e) => setSelField(e.target.value)}
                    data-testid="watch-field-select"
                >
                    <option value="">Field...</option>
                    {fields.map((f) => (
                        <option key={f.n} value={f.n}>{f.n}</option>
                    ))}
                </select>
                <button
                    data-testid="watch-add-btn"
                    disabled={!canAdd}
                    onClick={handleAdd}
                    style={buttonStyle('primary', { fontSize: 11, padding: '3px 10px', opacity: canAdd ? 1 : 0.5, cursor: canAdd ? 'pointer' : 'not-allowed' })}
                >
                    + Watch
                </button>
            </div>
        </div>
    );
}
