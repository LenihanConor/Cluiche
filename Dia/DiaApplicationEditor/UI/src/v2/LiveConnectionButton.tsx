import React, { useState } from 'react';
import { TrafficLightDot } from './TrafficLightDot';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { TLState } from './TrafficLightDot';

const STATE_CONFIG: Record<string, { dotState: TLState; label: string; bg: string; disabled: boolean }> = {
    disconnected: { dotState: 'grey',  label: 'Connect',       bg: '#333',    disabled: false },
    connecting:   { dotState: 'amber', label: 'Connecting...', bg: '#444',    disabled: true  },
    connected:    { dotState: 'green', label: 'Disconnect',    bg: '#1a3a1a', disabled: false },
};

export const LiveConnectionButton: React.FC = () => {
    const connectionState = useLiveStoreV2((s) => s.connectionState);
    const connect = useLiveStoreV2((s) => s.connect);
    const disconnect = useLiveStoreV2((s) => s.disconnect);

    const [showPopover, setShowPopover] = useState(false);
    const [host, setHost] = useState('localhost');
    const [port, setPort] = useState(7777);

    const cfg = STATE_CONFIG[connectionState];

    const handleClick = () => {
        if (connectionState === 'disconnected') {
            setShowPopover((v) => !v);
        } else if (connectionState === 'connected') {
            disconnect();
        }
    };

    const handleConnect = () => {
        setShowPopover(false);
        connect(host, port);
    };

    return (
        <div style={{ position: 'relative', display: 'inline-block' }}>
            <button
                data-testid="live-connection-btn"
                data-connection-state={connectionState}
                disabled={cfg.disabled}
                onClick={handleClick}
                style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: 6,
                    padding: '4px 10px',
                    background: cfg.bg,
                    color: '#ccc',
                    border: '1px solid #555',
                    borderRadius: 4,
                    cursor: cfg.disabled ? 'default' : 'pointer',
                    fontSize: 13,
                }}
            >
                <TrafficLightDot state={cfg.dotState} size={8} />
                {cfg.label}
            </button>

            {showPopover && (
                <div
                    data-testid="live-popover"
                    style={{
                        position: 'absolute',
                        top: '100%',
                        left: 0,
                        marginTop: 4,
                        background: '#2d2d2d',
                        border: '1px solid #555',
                        borderRadius: 4,
                        padding: 10,
                        zIndex: 100,
                        display: 'flex',
                        flexDirection: 'column',
                        gap: 6,
                        minWidth: 200,
                    }}
                >
                    <label style={{ color: '#ccc', fontSize: 12 }}>
                        Host
                        <input
                            data-testid="live-host-input"
                            type="text"
                            value={host}
                            onChange={(e) => setHost(e.target.value)}
                            style={{
                                display: 'block',
                                width: '100%',
                                marginTop: 2,
                                background: '#1e1e1e',
                                color: '#ccc',
                                border: '1px solid #555',
                                borderRadius: 3,
                                padding: '2px 6px',
                                fontSize: 12,
                                boxSizing: 'border-box',
                            }}
                        />
                    </label>
                    <label style={{ color: '#ccc', fontSize: 12 }}>
                        Port
                        <input
                            data-testid="live-port-input"
                            type="number"
                            value={port}
                            onChange={(e) => setPort(Number(e.target.value))}
                            style={{
                                display: 'block',
                                width: '100%',
                                marginTop: 2,
                                background: '#1e1e1e',
                                color: '#ccc',
                                border: '1px solid #555',
                                borderRadius: 3,
                                padding: '2px 6px',
                                fontSize: 12,
                                boxSizing: 'border-box',
                            }}
                        />
                    </label>
                    <button
                        data-testid="live-connect-submit"
                        onClick={handleConnect}
                        style={{
                            background: '#0e3460',
                            color: '#ccc',
                            border: '1px solid #555',
                            borderRadius: 3,
                            padding: '4px 8px',
                            cursor: 'pointer',
                            fontSize: 12,
                        }}
                    >
                        Connect
                    </button>
                </div>
            )}
        </div>
    );
};
