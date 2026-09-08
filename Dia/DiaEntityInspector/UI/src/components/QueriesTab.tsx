import { CSSProperties, useState } from 'react';
import { theme } from '@dia/editor-ui';
import type { QueryEntry } from '../types';

interface QueriesTabProps {
    queries: QueryEntry[];
    selectedEntityName: string | null;
}

export function QueriesTab({ queries, selectedEntityName }: QueriesTabProps) {
    const [openCards, setOpenCards] = useState<Record<string, boolean>>({});

    if (selectedEntityName === null) {
        return (
            <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: 6, color: theme.borderMuted }}>
                <span style={{ fontSize: 28 }}>←</span>
                <span style={{ fontSize: 11 }}>Select an entity</span>
            </div>
        );
    }

    const toggleCard = (sig: string) => {
        setOpenCards((prev) => ({ ...prev, [sig]: !prev[sig] }));
    };

    const isOpen = (sig: string, isMember: boolean): boolean => {
        if (sig in openCards) return openCards[sig];
        return isMember; // member cards start open
    };

    const cardHeaderStyle: CSSProperties = {
        background: theme.bgPanel,
        padding: '5px 10px',
        display: 'flex',
        alignItems: 'center',
        cursor: 'pointer',
        gap: 8,
    };

    const chipStyle: CSSProperties = {
        background: theme.bgPanel,
        border: `1px solid ${theme.border}`,
        borderRadius: 2,
        padding: '2px 6px',
        fontSize: 10,
        color: theme.textMuted,
        fontFamily: 'monospace',
    };

    return (
        <div style={{ overflowY: 'auto', flex: 1, padding: 6 }} data-testid="queries-tab">
            {queries.map((q) => {
                const isMember = q.members?.includes(selectedEntityName) ?? false;
                const open = isOpen(q.sig, isMember);

                return (
                    <div
                        key={q.sig}
                        data-testid={`query-card-${q.sig}`}
                        style={{ marginBottom: 4, border: `1px solid ${theme.border}`, borderRadius: 4, overflow: 'hidden' }}
                    >
                        <div style={cardHeaderStyle} onClick={() => toggleCard(q.sig)}>
                            <span style={{ color: theme.textMuted, fontSize: 9, transform: open ? 'rotate(90deg)' : 'none', display: 'inline-block', transition: 'transform 0.15s', width: 10 }}>▶</span>
                            <span style={{ fontFamily: 'monospace', fontSize: 11, color: theme.text, flex: 1 }}>{q.sig}</span>
                            {isMember && (
                                <span style={{ background: '#1a2a0a', border: '1px solid #4a7a1a', borderRadius: 2, padding: '1px 5px', fontSize: 9, color: '#8ab84a' }}>
                                    ✓ member
                                </span>
                            )}
                            <span style={{ background: theme.accent, borderRadius: 10, padding: '1px 8px', fontSize: 10, color: '#fff' }}>
                                {q.count}
                            </span>
                        </div>
                        {open && (
                            <div style={{ padding: 8, background: theme.bg }}>
                                <div style={{ fontSize: 10, color: theme.textMuted, marginBottom: 4 }}>Members:</div>
                                <div style={{ display: 'flex', flexWrap: 'wrap', gap: 3 }}>
                                    {(q.members ?? []).map((m) => (
                                        <span
                                            key={m}
                                            style={
                                                m === selectedEntityName
                                                    ? { ...chipStyle, borderColor: '#1177bb', color: '#fff', background: theme.accent }
                                                    : chipStyle
                                            }
                                        >
                                            {m}
                                        </span>
                                    ))}
                                </div>
                            </div>
                        )}
                    </div>
                );
            })}
        </div>
    );
}
