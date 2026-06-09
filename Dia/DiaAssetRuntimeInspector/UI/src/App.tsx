import { CSSProperties } from 'react';
import { theme } from '@dia/editor-ui';

export default function App() {
    const rootStyle: CSSProperties = {
        fontFamily: "'Segoe UI', system-ui, sans-serif",
        background: theme.bg,
        color: theme.text,
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        fontSize: 12,
    };

    return (
        <div style={rootStyle}>
            <div style={{ padding: 16, color: theme.textMuted }}>Asset Runtime Inspector</div>
        </div>
    );
}
