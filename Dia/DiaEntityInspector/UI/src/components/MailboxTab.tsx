import { CSSProperties, useState } from 'react';
import { theme, inputStyle, buttonStyle } from '@dia/editor-ui';
import type { MailboxEntry } from '../types';

interface MailboxTabProps {
    mailbox: MailboxEntry[];
    selectedEntityName: string | null;
    onClear: () => void;
}

// Message type badge colours — event-semantic, keep hardcoded
const MSG_TYPE_STYLES: Record<string, CSSProperties> = {
    mtdmg:   { background: '#280f00', border: '1px solid #6a4010', color: '#e8a850' },
    mtspwn:  { background: '#0a2000', border: '1px solid #1a6020', color: '#70c870' },
    mtdest:  { background: '#280000', border: '1px solid #6a1010', color: '#e87070' },
    mtstate: { background: '#10103a', border: '1px solid #30308a', color: '#b0b0f8' },
    mtanim:  { background: '#081828', border: '1px solid #18486a', color: '#70a8e8' },
    mtcoll:  { background: '#082828', border: '1px solid #186868', color: '#70c8c8' },
};

function badgeStyle(tc: string | undefined): CSSProperties {
    const base = tc ? (MSG_TYPE_STYLES[tc] ?? null) : null;
    if (!base) {
        return {
            background: theme.bgPanel,
            border: `1px solid ${theme.border}`,
            color: theme.textMuted,
            display: 'inline-block',
            padding: '1px 6px',
            borderRadius: 2,
            fontSize: 10,
        };
    }
    return {
        ...base,
        display: 'inline-block',
        padding: '1px 6px',
        borderRadius: 2,
        fontSize: 10,
    };
}

export function MailboxTab({ mailbox, selectedEntityName, onClear }: MailboxTabProps) {
    const [search, setSearch] = useState('');
    const [selfOnly, setSelfOnly] = useState(true);

    const srch = search.toLowerCase();

    const filtered = mailbox.filter((m) => {
        if (selfOnly && selectedEntityName) {
            if (m.inv !== selectedEntityName && m.s !== selectedEntityName && m.a !== selectedEntityName) {
                return false;
            }
        }
        if (srch) {
            const haystack = [m.s, m.a, m.type, m.p ?? ''].join(' ').toLowerCase();
            if (!haystack.includes(srch)) return false;
        }
        return true;
    });

    const toolbarStyle: CSSProperties = {
        background: theme.bgPanel,
        borderBottom: `1px solid ${theme.border}`,
        padding: '4px 8px',
        display: 'flex',
        alignItems: 'center',
        gap: 5,
        flexShrink: 0,
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
        fontFamily: 'monospace',
        fontSize: 11,
        height: 28,
        verticalAlign: 'middle',
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', flex: 1, overflow: 'hidden' }} data-testid="mailbox-tab">
            <div style={toolbarStyle}>
                <input
                    style={inputStyle({ width: 130, fontSize: 11 })}
                    type="text"
                    placeholder="filter..."
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                    data-testid="mailbox-search"
                />
                <button
                    data-testid="self-only-toggle"
                    onClick={() => setSelfOnly((v) => !v)}
                    style={buttonStyle(selfOnly ? 'primary' : 'default', { fontSize: 10, padding: '2px 7px' })}
                >
                    Self only
                </button>
                <div style={{ flex: 1 }} />
                <span style={{ color: theme.textMuted, fontSize: 11 }}>{filtered.length} messages</span>
                <button
                    data-testid="mailbox-clear"
                    onClick={onClear}
                    style={buttonStyle('default', { fontSize: 11, padding: '3px 8px' })}
                >
                    Clear
                </button>
            </div>
            <div style={{ flex: 1, overflowY: 'auto' }}>
                <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                    <thead>
                        <tr>
                            <th style={{ ...thStyle, width: 48 }}>Frame</th>
                            <th style={{ ...thStyle, width: 100 }}>Sender</th>
                            <th style={{ ...thStyle, width: 120 }}>Address</th>
                            <th style={{ ...thStyle, width: 130 }}>Type</th>
                            <th style={thStyle}>Payload</th>
                        </tr>
                    </thead>
                    <tbody>
                        {filtered.map((m, i) => {
                            const isSender = selectedEntityName && m.s === selectedEntityName;
                            const isAddr = selectedEntityName && m.a === selectedEntityName;
                            return (
                                <tr key={i} data-testid={`mailbox-row-${i}`}>
                                    <td style={{ ...tdStyle, color: theme.textMuted }}>{m.f}</td>
                                    <td style={{ ...tdStyle, color: isSender ? '#fff' : theme.textMuted, fontWeight: isSender ? 600 : undefined }}>
                                        {m.s}
                                    </td>
                                    <td style={tdStyle}>
                                        <span style={{ color: '#252545', fontSize: 9 }}>[{m.ak ?? ''}]</span>{' '}
                                        <span style={{ color: isAddr ? '#fff' : '#9cdcfe', fontWeight: isAddr ? 600 : undefined }}>{m.a}</span>
                                    </td>
                                    <td style={tdStyle}>
                                        <span style={badgeStyle(m.tc)}>{m.type}</span>
                                    </td>
                                    <td style={{ ...tdStyle, color: theme.textMuted, fontSize: 10 }}>{m.p ?? ''}</td>
                                </tr>
                            );
                        })}
                    </tbody>
                </table>
            </div>
        </div>
    );
}
