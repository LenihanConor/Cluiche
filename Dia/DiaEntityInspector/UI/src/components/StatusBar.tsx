import { CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';

interface StatusBarProps {
    frame: number;
    entityCount: number;
    selectedName: string | null;
    watchCount: number;
}

export function StatusBar({ frame, entityCount, selectedName, watchCount }: StatusBarProps) {
    const barStyle: CSSProperties = {
        background: theme.bgPanel,
        borderTop: `1px solid ${theme.border}`,
        padding: '2px 10px',
        fontSize: 10,
        color: theme.textMuted,
        display: 'flex',
        gap: 14,
        flexShrink: 0,
    };

    return (
        <div style={barStyle} data-testid="status-bar">
            <span>
                Frame <b style={{ color: theme.textMuted }}>{frame > 0 ? `#${frame}` : '-'}</b>
                {entityCount > 0 && <> · <b style={{ color: theme.textMuted }}>{entityCount}</b> entities</>}
            </span>
            <span>
                Selected: <b style={{ color: theme.textMuted }}>{selectedName ?? '—'}</b>
            </span>
            <span>
                watch: <b style={{ color: theme.textMuted }}>{watchCount}</b>
            </span>
        </div>
    );
}
