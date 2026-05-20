import React, { useState } from 'react';
import { bridgeRequest } from './bridge';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { StreamV2 } from './types';

function isSystemStream(id: string): boolean {
    return id.startsWith('$');
}

// ────────────────────────────────────────────────────────────
// StreamDetailInspector
// ────────────────────────────────────────────────────────────

interface StreamDetailInspectorProps {
    stream: StreamV2;
    msgPerSec?: number;
}

const StreamDetailInspector: React.FC<StreamDetailInspectorProps> = ({ stream, msgPerSec }) => {
    const readonly = isSystemStream(stream.id);

    const sendCommand = (commandType: string, value: unknown) => {
        bridgeRequest('manifest.applyCommand', { commandType, streamId: stream.id, value });
    };

    const fieldStyle: React.CSSProperties = {
        background: '#1e1e1e',
        color: '#ccc',
        border: '1px solid #444',
        borderRadius: 3,
        padding: '2px 6px',
        fontSize: 12,
        width: '100%',
        boxSizing: 'border-box',
    };

    const labelStyle: React.CSSProperties = {
        color: '#888',
        fontSize: 11,
        marginBottom: 2,
        display: 'block',
    };

    const rowStyle: React.CSSProperties = {
        marginBottom: 8,
    };

    return (
        <div
            data-testid="stream-detail"
            style={{
                width: 260,
                flexShrink: 0,
                background: '#2d2d2d',
                borderLeft: '1px solid #444',
                padding: 12,
                overflowY: 'auto',
                display: 'flex',
                flexDirection: 'column',
            }}
        >
            <div style={rowStyle}>
                <span style={labelStyle}>Stream ID</span>
                <span data-testid="stream-id-label" style={{ color: '#ccc', fontSize: 13, fontStyle: readonly ? 'italic' : 'normal' }}>
                    {stream.id}
                </span>
            </div>

            {msgPerSec !== undefined && (
                <div style={{ ...rowStyle, color: '#3cb370', fontSize: 12 }}>
                    {msgPerSec} msg/s
                </div>
            )}

            <div style={rowStyle}>
                <label style={labelStyle}>Kind</label>
                <input
                    style={fieldStyle}
                    value={stream.kind}
                    disabled={readonly}
                    onChange={(e) => sendCommand('SetStreamKind', e.target.value)}
                />
            </div>

            <div style={rowStyle}>
                <label style={labelStyle}>Payload Type</label>
                <input
                    style={fieldStyle}
                    value={stream.payloadType}
                    disabled={readonly}
                    onChange={(e) => sendCommand('SetStreamPayloadType', e.target.value)}
                />
            </div>

            <div style={rowStyle}>
                <label style={labelStyle}>From PU</label>
                <input
                    style={fieldStyle}
                    value={stream.fromPU}
                    disabled={readonly}
                    onChange={(e) => sendCommand('SetStreamFromPU', e.target.value)}
                />
            </div>

            <div style={rowStyle}>
                <label style={labelStyle}>To PU</label>
                <input
                    style={fieldStyle}
                    value={stream.toPU}
                    disabled={readonly}
                    onChange={(e) => sendCommand('SetStreamToPU', e.target.value)}
                />
            </div>

            <div style={rowStyle}>
                <label style={labelStyle}>Capacity</label>
                <input
                    type="number"
                    style={fieldStyle}
                    value={stream.capacity}
                    disabled={readonly}
                    onChange={(e) => sendCommand('SetStreamCapacity', Number(e.target.value))}
                />
            </div>

            <div style={rowStyle}>
                <label style={labelStyle}>Max Readers</label>
                <input
                    type="number"
                    style={fieldStyle}
                    value={stream.maxReaders}
                    disabled={readonly}
                    onChange={(e) => sendCommand('SetStreamMaxReaders', Number(e.target.value))}
                />
            </div>
        </div>
    );
};

// ────────────────────────────────────────────────────────────
// StreamsTab
// ────────────────────────────────────────────────────────────

export const StreamsTab: React.FC = () => {
    const manifest = useManifestStoreV2((s) => s.manifest);
    const connectionState = useLiveStoreV2((s) => s.connectionState);
    const liveStreams = useLiveStoreV2((s) => s.streams);

    const [selectedId, setSelectedId] = useState<string | null>(null);

    const streams: StreamV2[] = manifest?.streams ?? [];
    const selectedStream = streams.find((s) => s.id === selectedId) ?? null;

    const getLiveMsgPerSec = (streamId: string): number | undefined => {
        if (connectionState !== 'connected') return undefined;
        const ls = liveStreams.find((l) => l.streamId === streamId);
        return ls?.msgPerSec;
    };

    const thStyle: React.CSSProperties = {
        textAlign: 'left',
        padding: '4px 8px',
        color: '#888',
        fontSize: 11,
        fontWeight: 'normal',
        borderBottom: '1px solid #444',
        whiteSpace: 'nowrap',
    };

    return (
        <div
            data-testid="streams-tab"
            style={{
                display: 'flex',
                flex: 1,
                overflow: 'hidden',
                background: '#1e1e1e',
            }}
        >
            {/* Table */}
            <div style={{ flex: 1, overflowY: 'auto' }}>
                {streams.length === 0 ? (
                    <div style={{ color: '#666', padding: 16, fontSize: 13 }}>No streams defined.</div>
                ) : (
                    <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 13 }}>
                        <thead>
                            <tr>
                                <th style={thStyle}>ID</th>
                                <th style={thStyle}>Kind</th>
                                <th style={thStyle}>Payload Type</th>
                                <th style={thStyle}>From PU</th>
                                <th style={thStyle}>To PU</th>
                                <th style={thStyle}>Capacity</th>
                                <th style={thStyle}>Max Readers</th>
                            </tr>
                        </thead>
                        <tbody>
                            {streams.map((stream) => {
                                const sys = isSystemStream(stream.id);
                                const selected = selectedId === stream.id;
                                return (
                                    <tr
                                        key={stream.id}
                                        data-testid="stream-row"
                                        data-stream-id={stream.id}
                                        onClick={() => setSelectedId(stream.id)}
                                        style={{
                                            background: selected ? '#0e3460' : 'transparent',
                                            cursor: 'pointer',
                                            opacity: sys ? 0.6 : 1,
                                        }}
                                    >
                                        <td style={{ padding: '3px 8px', color: '#ccc', fontStyle: sys ? 'italic' : 'normal' }}>{stream.id}</td>
                                        <td style={{ padding: '3px 8px', color: '#ccc' }}>{stream.kind}</td>
                                        <td style={{ padding: '3px 8px', color: '#ccc' }}>{stream.payloadType}</td>
                                        <td style={{ padding: '3px 8px', color: '#ccc' }}>{stream.fromPU}</td>
                                        <td style={{ padding: '3px 8px', color: '#ccc' }}>{stream.toPU}</td>
                                        <td style={{ padding: '3px 8px', color: '#ccc' }}>{stream.capacity}</td>
                                        <td style={{ padding: '3px 8px', color: '#ccc' }}>{stream.maxReaders}</td>
                                    </tr>
                                );
                            })}
                        </tbody>
                    </table>
                )}
            </div>

            {/* Sidebar */}
            {selectedStream ? (
                <StreamDetailInspector
                    stream={selectedStream}
                    msgPerSec={getLiveMsgPerSec(selectedStream.id)}
                />
            ) : (
                <div
                    data-testid="stream-detail"
                    style={{
                        width: 260,
                        flexShrink: 0,
                        background: '#2d2d2d',
                        borderLeft: '1px solid #444',
                        padding: 12,
                        color: '#666',
                        fontSize: 13,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                    }}
                >
                    Select a stream
                </div>
            )}
        </div>
    );
};
