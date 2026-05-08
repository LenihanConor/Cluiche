import React, { useState } from 'react';
import { useManifestStore } from './ManifestStore';
import type { ManifestData } from './types';

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const ImportsPanel: React.FC = () => {
    const manifest = useManifestStore(s => s.manifest) as ManifestData | null;
    const [expanded, setExpanded] = useState(false);

    if (!manifest) return null;
    const imports = manifest.imports ?? [];
    if (imports.length === 0 && !expanded) return null;

    return (
        <div style={styles.container}>
            <button
                style={styles.toggle}
                onClick={() => setExpanded(!expanded)}
            >
                Imports ({imports.length}) {expanded ? '▾' : '▸'}
            </button>

            {expanded && (
                <div style={styles.list}>
                    {imports.map((imp, i) => (
                        <div key={i} style={styles.item}>
                            <span style={styles.filename}>{imp}</span>
                            <button
                                style={styles.openBtn}
                                onClick={() => sendToPlugin('open_manifest', { path: imp })}
                                title="Open in editor"
                            >
                                Open
                            </button>
                            <button
                                style={styles.removeBtn}
                                onClick={() => {
                                    useManifestStore.getState().pushUndo('Remove import');
                                    sendToPlugin('remove_import', { path: imp });
                                    useManifestStore.getState().setDirty(true);
                                }}
                                title="Remove import"
                            >
                                ×
                            </button>
                        </div>
                    ))}
                    <button
                        style={styles.addBtn}
                        onClick={() => sendToPlugin('add_import')}
                    >
                        + Add Import
                    </button>
                </div>
            )}
        </div>
    );
};

const styles: Record<string, React.CSSProperties> = {
    container: { borderBottom: '1px solid #333', flexShrink: 0 },
    toggle: {
        width: '100%', background: 'none', border: 'none', color: '#aaa',
        fontSize: 11, padding: '3px 8px', textAlign: 'left', cursor: 'pointer',
    },
    list: { padding: '0 8px 6px' },
    item: {
        display: 'flex', alignItems: 'center', gap: 4, padding: '2px 0',
        fontSize: 11, color: '#ccc',
    },
    filename: { flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' },
    openBtn: {
        background: 'none', border: '1px solid #555', borderRadius: 2,
        color: '#aaa', fontSize: 10, padding: '1px 4px', cursor: 'pointer',
    },
    removeBtn: {
        background: 'none', border: 'none', color: '#f44336', fontSize: 14,
        cursor: 'pointer', padding: '0 2px',
    },
    addBtn: {
        background: 'none', border: '1px dashed #555', borderRadius: 3,
        color: '#888', fontSize: 11, padding: '3px 8px', cursor: 'pointer',
        marginTop: 4, width: '100%',
    },
};
