import { CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';
import type { EntityEntry } from '../types';
import { tagChipStyle } from '../tagStyles';

interface ContextStripProps {
    entity: EntityEntry | null;
}

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

    const tagStyle = tagChipStyle;

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
