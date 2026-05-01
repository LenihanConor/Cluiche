import React, { useState } from 'react';
import { useManifestStore } from './ManifestStore';

interface ConflictState {
    diskVersion: object;
    localVersion: object;
}

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

interface ConflictBannerProps {
    conflict: ConflictState;
    onDismiss: () => void;
}

export const ConflictBanner: React.FC<ConflictBannerProps> = ({ conflict, onDismiss }) => {
    const [showDiff, setShowDiff] = useState(false);

    const handleKeepLocal = () => {
        sendToPlugin('resolve_conflict', { action: 'keep_local' });
        onDismiss();
    };

    const handleReloadDisk = () => {
        if (!window.confirm('Discard your changes and reload from disk?')) return;
        sendToPlugin('resolve_conflict', { action: 'reload_disk' });
        onDismiss();
    };

    const diskText  = JSON.stringify(conflict.diskVersion, null, 2);
    const localText = JSON.stringify(conflict.localVersion, null, 2);

    return (
        <div style={bannerStyle}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, flexShrink: 0 }}>
                <span style={{ color: '#e8b25f', fontWeight: 600 }}>File modified externally.</span>
                <button style={btnStyle} onClick={handleKeepLocal}>Keep My Changes</button>
                <button style={btnStyle} onClick={handleReloadDisk}>Reload from Disk</button>
                <button style={btnStyle} onClick={() => setShowDiff(d => !d)}>
                    {showDiff ? 'Hide Diff' : 'Show Diff'}
                </button>
            </div>

            {showDiff && (
                <div style={{ display: 'flex', gap: 8, marginTop: 8, fontSize: 11, overflow: 'auto', maxHeight: 200 }}>
                    <div style={diffCol}>
                        <div style={diffHeader}>Disk</div>
                        <pre style={diffPre}>{diskText}</pre>
                    </div>
                    <div style={diffCol}>
                        <div style={diffHeader}>Your Changes</div>
                        <pre style={diffPre}>{localText}</pre>
                    </div>
                </div>
            )}
        </div>
    );
};

// Hook: subscribe to file_conflict_detected and conflict_resolved topics
export function useConflictDetection(): { conflict: ConflictState | null; dismiss: () => void } {
    const [conflict, setConflict] = React.useState<ConflictState | null>(null);

    React.useEffect(() => {
        const handler = (event: MessageEvent<{ __dia?: boolean; topic?: string; data?: unknown }>) => {
            if (!event.data?.__dia) return;
            if (event.data.topic === 'file_conflict_detected') {
                const d = event.data.data as { disk_version: object; local_version: object };
                setConflict({ diskVersion: d.disk_version, localVersion: d.local_version });
            } else if (event.data.topic === 'conflict_resolved') {
                setConflict(null);
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    return { conflict, dismiss: () => setConflict(null) };
}

const bannerStyle: React.CSSProperties = {
    background: '#3a2a00', borderBottom: '1px solid #5a4500', padding: '6px 12px',
    fontSize: 12, color: '#ccc', flexShrink: 0,
};
const btnStyle: React.CSSProperties = {
    fontSize: 11, padding: '2px 8px', borderRadius: 3, border: '1px solid #5a4500',
    background: '#4a3500', color: '#e8b25f', cursor: 'pointer',
};
const diffCol: React.CSSProperties = { flex: 1, minWidth: 0 };
const diffHeader: React.CSSProperties = { fontWeight: 600, color: '#aaa', marginBottom: 2 };
const diffPre: React.CSSProperties = { background: '#1a1a1a', padding: 6, borderRadius: 3, overflow: 'auto', margin: 0, color: '#ccc' };
