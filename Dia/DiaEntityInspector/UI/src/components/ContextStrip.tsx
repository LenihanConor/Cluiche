import { CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';
import type { EntityEntry } from '../types';

interface ContextStripProps {
    entity: EntityEntry | null;
}

// Component tag colours — domain-semantic, keep hardcoded
const TAG_STYLES: Record<string, CSSProperties> = {
    T: { borderColor: '#3a6a8a', color: '#6aa8c8', background: '#151e28' },
    P: { borderColor: '#3a8a4a', color: '#6ac47a', background: '#15201a' },
    R: { borderColor: '#8a5a3a', color: '#c4906a', background: '#201515' },
    A: { borderColor: '#8a8a3a', color: '#c4c46a', background: '#201e15' },
    S: { borderColor: '#6a3a8a', color: '#a46ac4', background: '#1a1525' },
    H: { borderColor: '#8a3a3a', color: '#c46a6a', background: '#201515' },
    C: { borderColor: '#3a8a8a', color: '#6ac4c4', background: '#151e1e' },
};

const defaultTagStyle: CSSProperties = {
    borderColor: theme.border,
    color: theme.textMuted,
    background: theme.bgPanel,
};

export function ContextStrip({ entity }: ContextStripProps) {
    const stripStyle: CSSProperties = {
        background: theme.bgPanel,
        borderBottom: `1px solid ${theme.border}`,
        padding: '5px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: 8,
        flexShrink: 0,
        minHeight: 34,
    };

    const tagStyle = (tag: string): CSSProperties => ({
        fontFamily: 'monospace',
        fontSize: 8.5,
        border: `1px solid ${(TAG_STYLES[tag] ?? defaultTagStyle).borderColor}`,
        borderRadius: 2,
        padding: '1px 2px',
        color: (TAG_STYLES[tag] ?? defaultTagStyle).color,
        background: (TAG_STYLES[tag] ?? defaultTagStyle).background,
    });

    if (!entity) {
        return (
            <div style={stripStyle} data-testid="context-strip">
                <div style={{ display: 'flex', alignItems: 'center', gap: 6, color: theme.borderMuted }}>
                    <span style={{ fontSize: 18 }}>←</span>
                    <span style={{ fontSize: 11 }}>Select an entity</span>
                </div>
            </div>
        );
    }

    return (
        <div style={stripStyle} data-testid="context-strip">
            <span style={{ fontFamily: 'monospace', fontSize: 10, color: theme.textMuted }}>
                [{String(entity.i).padStart(3, '0')}]
            </span>
            <span style={{ fontSize: 13, color: theme.text, fontWeight: 600 }}>{entity.n}</span>
            <div style={{ display: 'flex', gap: 2 }}>
                {entity.t.map((tag) => (
                    <span key={tag} style={tagStyle(tag)}>{tag}</span>
                ))}
            </div>
            <div style={{ marginLeft: 'auto', display: 'flex', gap: 12 }}>
                <span style={{ fontSize: 10, color: theme.textMuted }}>
                    gen: <b style={{ color: '#9cdcfe' }}>g{entity.g}</b>
                </span>
                <span style={{ fontSize: 10, color: theme.textMuted }}>
                    comps: <b style={{ color: '#9cdcfe' }}>{entity.t.length}</b>
                </span>
            </div>
        </div>
    );
}
