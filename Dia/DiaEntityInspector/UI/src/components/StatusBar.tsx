import { CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';

interface StatusBarProps {
    connected: boolean;
    frame: number;
    entityCount: number;
    selectedName: string | null;
    watchCount: number;
}

export function StatusBar({ connected, frame, entityCount, selectedName, watchCount }: StatusBarProps) {
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

    const connStyle: CSSProperties = {
        color: connected ? theme.success : theme.warning,
    };

    return (
        <div style={barStyle} data-testid="status-bar">
            <span style={connStyle}>
                {connected ? '● Live' : '● Disconnected'}
            </span>
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
