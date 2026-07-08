import { useState, useEffect, type ReactNode } from 'react';
import { theme } from '@dia/editor-ui';
import type { BoardEntry } from '../types';
import { SlotTable } from './SlotTable';
import { ObserverList } from './ObserverList';

interface BadgeProps {
    children: ReactNode;
}

function Badge({ children }: BadgeProps) {
    return (
        <span
            style={{
                fontSize: 10,
                padding: '1px 6px',
                borderRadius: 8,
                background: '#2d2d2d',
                color: theme.textMuted,
                border: `1px solid ${theme.border}`,
                whiteSpace: 'nowrap',
            }}
        >
            {children}
        </span>
    );
}

interface BoardCardProps {
    board: BoardEntry;
    /** When non-null, forces all cards to expand (true) or collapse (false). Parent resets to null after. */
    expandOverride: boolean | null;
}

export function BoardCard({ board, expandOverride }: BoardCardProps) {
    const [expanded, setExpanded] = useState(true);

    useEffect(() => {
        if (expandOverride !== null) {
            setExpanded(expandOverride);
        }
    }, [expandOverride]);

    const slotCount = board.slots.length;
    const obsCount = board.observers.length;

    return (
        <div style={{ border: `1px solid ${theme.border}`, borderRadius: 4, marginBottom: 6, overflow: 'hidden' }}>
            <div
                onClick={() => setExpanded((v) => !v)}
                style={{
                    background: theme.bgPanel,
                    padding: '4px 10px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: 6,
                    cursor: 'pointer',
                    userSelect: 'none',
                }}
            >
                <span
                    style={{
                        color: theme.textMuted,
                        fontSize: 9,
                        transform: expanded ? '' : 'rotate(-90deg)',
                        transition: 'transform .15s',
                    }}
                >
                    ▶
                </span>
                <span style={{ fontWeight: 600, color: '#9cdcfe', fontSize: 11.5, flex: 1 }}>
                    {board.label || board.id}
                </span>
                <span style={{ color: theme.textMuted, fontSize: 10, fontFamily: 'monospace' }}>
                    {board.id}
                </span>
                <Badge>{slotCount} {slotCount === 1 ? 'slot' : 'slots'}</Badge>
                <Badge>{obsCount} {obsCount === 1 ? 'observer' : 'observers'}</Badge>
            </div>
            {expanded && (
                <div style={{ padding: '4px 6px' }}>
                    <SlotTable slots={board.slots} />
                    <ObserverList observers={board.observers} />
                </div>
            )}
        </div>
    );
}
