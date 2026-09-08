import React from 'react';
import type { StreamState } from './useInspectorStore';

interface Props {
    stream: StreamState;
}

function barColor(fill: number): string {
    if (fill < 60) return '#4caf50';
    if (fill < 85) return '#ff9800';
    return '#f44336';
}

export const StreamBackpressureRow: React.FC<Props> = ({ stream }) => {
    const color = barColor(stream.fillPercent);
    const hasDrops = stream.dropsTotal > 0;

    return (
        <div
            data-testid="stream-backpressure-row"
            style={{
                padding: '6px 8px',
                borderBottom: '1px solid #333',
                fontSize: 12,
            }}
        >
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                <span data-testid="stream-id" style={{ color: '#ccc', fontWeight: 600, minWidth: 120 }}>
                    {stream.streamId}
                </span>
                <span data-testid="stream-msg-rate" style={{ color: '#888', minWidth: 60 }}>
                    {stream.msgPerSec.toFixed(1)} msg/s
                </span>
                <span data-testid="stream-kb-rate" style={{ color: '#888', minWidth: 60 }}>
                    {stream.kbPerSec.toFixed(1)} KB/s
                </span>
                {hasDrops ? (
                    <span
                        data-testid="drops-badge"
                        style={{
                            color: '#f44336',
                            fontWeight: 700,
                            fontSize: 11,
                            marginLeft: 'auto',
                        }}
                    >
                        {stream.dropsTotal} drops
                    </span>
                ) : (
                    <span
                        data-testid="drops-badge"
                        style={{ color: '#555', fontSize: 11, marginLeft: 'auto' }}
                    >
                        0 drops
                    </span>
                )}
            </div>
            <div style={{ marginTop: 4, background: '#333', borderRadius: 2, height: 4, overflow: 'hidden' }}>
                <div
                    data-testid="fill-bar"
                    data-fill={stream.fillPercent}
                    style={{
                        width: `${stream.fillPercent}%`,
                        height: '100%',
                        background: color,
                        transition: 'width 0.3s',
                    }}
                />
            </div>
        </div>
    );
};
