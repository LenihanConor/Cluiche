import { CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';

export const TAG_STYLES: Record<string, CSSProperties> = {
    T: { borderColor: '#3a6a8a', color: '#6aa8c8', background: '#151e28' },
    P: { borderColor: '#3a8a4a', color: '#6ac47a', background: '#15201a' },
    R: { borderColor: '#8a5a3a', color: '#c4906a', background: '#201515' },
    A: { borderColor: '#8a8a3a', color: '#c4c46a', background: '#201e15' },
    S: { borderColor: '#6a3a8a', color: '#a46ac4', background: '#1a1525' },
    H: { borderColor: '#8a3a3a', color: '#c46a6a', background: '#201515' },
    C: { borderColor: '#3a8a8a', color: '#6ac4c4', background: '#151e1e' },
};

export const DEFAULT_TAG_STYLE: CSSProperties = {
    borderColor: theme.border,
    color: theme.textMuted,
    background: theme.bgPanel,
};

export function tagChipStyle(tag: string): CSSProperties {
    const base = TAG_STYLES[tag] ?? DEFAULT_TAG_STYLE;
    return {
        fontFamily: 'monospace',
        fontSize: 8.5,
        border: `1px solid ${base.borderColor}`,
        borderRadius: 2,
        padding: '1px 2px',
        color: base.color,
        background: base.background,
    };
}
