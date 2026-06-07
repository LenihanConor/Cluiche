import React from 'react';
import { useLiveStoreV2 } from './useLiveStoreV2';

export const ConnectionStatusDot: React.FC = () => {
    const connectionState = useLiveStoreV2((s) => s.connectionState);

    const dotStyle: React.CSSProperties = {
        width: 8,
        height: 8,
        borderRadius: '50%',
        display: 'inline-block',
        flexShrink: 0,
        backgroundColor:
            connectionState === 'connected' ? '#4caf50' :
            connectionState === 'connecting' ? '#ff9800' : '#555',
        boxShadow: connectionState === 'connected' ? '0 0 4px #4caf50' : undefined,
    };

    const label = connectionState === 'connected' ? 'Live' :
                  connectionState === 'connecting' ? 'Connecting...' : 'Offline';

    return (
        <div
            data-testid="connection-status-dot"
            data-connection-state={connectionState}
            style={{ display: 'flex', alignItems: 'center', gap: 5, fontSize: 11, color: '#888', userSelect: 'none' }}
        >
            <span style={dotStyle} />
            {label}
        </div>
    );
};
