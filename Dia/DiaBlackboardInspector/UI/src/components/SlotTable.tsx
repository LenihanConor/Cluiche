import React from 'react';
import { theme } from '@dia/editor-ui';
import type { SlotEntry } from '../types';

function renderValue(val: unknown): React.ReactNode {
    if (val === null) {
        return <span style={{ color: '#b5cea8' }}>null</span>;
    }
    if (typeof val === 'boolean') {
        return <span style={{ color: '#b5cea8' }}>{String(val)}</span>;
    }
    if (typeof val === 'number') {
        return <span style={{ color: '#b5cea8' }}>{val}</span>;
    }
    if (typeof val === 'string') {
        return <span style={{ color: '#ce9178' }}>"{val}"</span>;
    }
    if (Array.isArray(val)) {
        if (val.length === 0) return <span>{'[]'}</span>;
        return (
            <span>
                {'[ '}
                {val.map((item, i) => (
                    <React.Fragment key={i}>
                        {renderValue(item)}
                        {i < val.length - 1 ? ', ' : ''}
                    </React.Fragment>
                ))}
                {' ]'}
            </span>
        );
    }
    if (typeof val === 'object' && val !== null) {
        const keys = Object.keys(val as Record<string, unknown>);
        if (keys.length === 0) return <span>{'{}'}</span>;
        return (
            <span>
                {'{ '}
                {keys.map((k, i) => (
                    <React.Fragment key={k}>
                        <span style={{ color: '#9cdcfe' }}>{k}</span>
                        {': '}
                        {renderValue((val as Record<string, unknown>)[k])}
                        {i < keys.length - 1 ? ', ' : ''}
                    </React.Fragment>
                ))}
                {' }'}
            </span>
        );
    }
    return <span>{String(val)}</span>;
}

interface SlotTableProps {
    slots: SlotEntry[];
}

export function SlotTable({ slots }: SlotTableProps) {
    return (
        <div>
            <div style={{ fontSize: 10, textTransform: 'uppercase', letterSpacing: '0.07em', color: theme.textMuted, padding: '5px 4px 3px' }}>
                Slots
            </div>
            {slots.length === 0 ? (
                <div style={{ padding: '2px 4px', color: theme.textMuted, fontStyle: 'italic', fontSize: 11 }}>—</div>
            ) : (
                slots.map((slot) => (
                    <div
                        key={slot.key}
                        className="prop-row"
                        style={{
                            display: 'flex',
                            alignItems: 'baseline',
                            padding: '2px 0',
                            paddingLeft: 4,
                            minHeight: 22,
                            borderLeft: '2px solid transparent',
                            borderBottom: `1px solid ${theme.borderMuted}`,
                        }}
                    >
                        <span style={{ width: 110, fontSize: 11, color: '#9cdcfe', fontFamily: 'monospace', flexShrink: 0 }}>
                            {slot.key}
                        </span>
                        <span style={{ width: 120, fontSize: 10, color: '#4ec9b0', fontFamily: 'monospace', flexShrink: 0, paddingTop: 1 }}>
                            {slot.type}
                        </span>
                        <span style={{ flex: 1, fontFamily: 'monospace', fontSize: 11, color: theme.text }}>
                            {slot.value === '[no serializer]' ? (
                                <span style={{ color: '#555', fontStyle: 'italic' }}>[no serializer]</span>
                            ) : (
                                renderValue(slot.value)
                            )}
                        </span>
                    </div>
                ))
            )}
        </div>
    );
}
