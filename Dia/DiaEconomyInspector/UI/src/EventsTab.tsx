import React from 'react';
import { theme } from '@dia/editor-ui';
import { useEconomyStore } from './useEconomyStore';
import type { EconomyEvent } from './useEconomyStore';

// ---------------------------------------------------------------------------
// Type badge
// ---------------------------------------------------------------------------

const TYPE_BADGE_COLORS: Record<EconomyEvent['type'], { bg: string; color: string }> = {
    Earn:     { bg: '#2d6a2d', color: '#89d185' },
    Spend:    { bg: '#1a3a5c', color: '#4da6ff' },
    Clamped:  { bg: '#5c3a00', color: '#cca700' },
    Transfer: { bg: '#3a1a5c', color: '#c084fc' },
};

interface TypeBadgeProps {
    type: EconomyEvent['type'];
}

const TypeBadge: React.FC<TypeBadgeProps> = ({ type }) => {
    const colors = TYPE_BADGE_COLORS[type];
    return (
        <span
            data-testid="event-type-badge"
            style={{
                padding: '1px 6px',
                borderRadius: 3,
                background: colors.bg,
                color: colors.color,
                fontSize: 10,
                fontWeight: 600,
                textTransform: 'uppercase',
                whiteSpace: 'nowrap',
            }}
        >
            {type}
        </span>
    );
};

// ---------------------------------------------------------------------------
// Detail cell
// ---------------------------------------------------------------------------

function getDetail(event: EconomyEvent): string {
    if (event.type === 'Transfer' && event.destination) {
        return `→ ${event.destination}`;
    }
    return '';
}

// ---------------------------------------------------------------------------
// Main tab
// ---------------------------------------------------------------------------

export const EventsTab: React.FC = () => {
    const events = useEconomyStore((s) => s.events);

    return (
        <div style={{ height: '100%', overflow: 'auto' }}>
            <table
                style={{
                    width: '100%',
                    borderCollapse: 'collapse',
                    fontSize: 11,
                    color: theme.text,
                }}
            >
                <thead>
                    <tr
                        style={{
                            background: theme.bgPanel,
                            borderBottom: `1px solid ${theme.border}`,
                            position: 'sticky',
                            top: 0,
                        }}
                    >
                        {['Frame', 'Instance', 'Type', 'Resource', 'Amount', 'Detail'].map((h) => (
                            <th
                                key={h}
                                style={{
                                    padding: '5px 8px',
                                    textAlign: 'left',
                                    color: theme.textMuted,
                                    fontWeight: 600,
                                    whiteSpace: 'nowrap',
                                }}
                            >
                                {h}
                            </th>
                        ))}
                    </tr>
                </thead>
                <tbody>
                    {events.length === 0 ? (
                        <tr>
                            <td colSpan={6} style={{ padding: 8, color: theme.textMuted, textAlign: 'center' }}>
                                No events
                            </td>
                        </tr>
                    ) : (
                        events.map((event, idx) => {
                            const isClamped = event.type === 'Clamped';
                            const detail = getDetail(event);
                            return (
                                <tr
                                    key={idx}
                                    data-testid="event-row"
                                    style={{
                                        borderBottom: `1px solid ${theme.border}22`,
                                        background: isClamped ? '#3a1e0033' : 'transparent',
                                    }}
                                >
                                    <td style={{ padding: '3px 8px', color: theme.textMuted }}>{event.frame}</td>
                                    <td style={{ padding: '3px 8px' }}>{event.instance}</td>
                                    <td style={{ padding: '3px 8px' }}>
                                        <TypeBadge type={event.type} />
                                    </td>
                                    <td style={{ padding: '3px 8px' }}>{event.resource}</td>
                                    <td style={{ padding: '3px 8px', color: theme.textMuted }}>
                                        {event.amount !== undefined
                                            ? event.amount.toFixed(2)
                                            : event.attempted !== undefined
                                            ? `${event.attempted.toFixed(2)} → ${(event.actual ?? 0).toFixed(2)}`
                                            : ''}
                                    </td>
                                    <td style={{ padding: '3px 8px', color: theme.textMuted }}>{detail}</td>
                                </tr>
                            );
                        })
                    )}
                </tbody>
            </table>
        </div>
    );
};
