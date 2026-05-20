import React from 'react';
import { TrafficLightDot } from './TrafficLightDot';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { TLState } from './TrafficLightDot';

const STATE_CONFIG: Record<string, { dotState: TLState; label: string; bg: string }> = {
    disconnected: { dotState: 'amber', label: 'Disconnected',  bg: '#333'    },
    connecting:   { dotState: 'amber', label: 'Connecting...', bg: '#444'    },
    connected:    { dotState: 'green', label: 'Connected',     bg: '#1a3a1a' },
};

// Read-only status indicator. The taskbar (CluicheEditor host) owns the
// connect/disconnect action — see Cluiche/CluicheEditor/UI/src/layout/Toolbar.tsx.
export const LiveConnectionButton: React.FC = () => {
    const connectionState = useLiveStoreV2((s) => s.connectionState);
    const cfg = STATE_CONFIG[connectionState];

    return (
        <div
            data-testid="live-connection-indicator"
            data-connection-state={connectionState}
            style={{
                display: 'flex',
                alignItems: 'center',
                gap: 6,
                padding: '4px 10px',
                background: cfg.bg,
                color: '#ccc',
                border: '1px solid #555',
                borderRadius: 4,
                fontSize: 13,
                userSelect: 'none',
            }}
        >
            <TrafficLightDot state={cfg.dotState} size={8} />
            {cfg.label}
        </div>
    );
};
