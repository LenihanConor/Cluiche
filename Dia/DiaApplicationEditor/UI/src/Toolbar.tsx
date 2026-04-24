import React, { useEffect } from 'react';
import { useManifestStore } from './ManifestStore';

function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const Toolbar: React.FC = () => {
    const manifest   = useManifestStore(s => s.manifest);
    const isDirty    = useManifestStore(s => s.isDirty);
    const currentView = useManifestStore(s => s.currentView);
    const toggleView = useManifestStore(s => s.toggleView);

    // Ctrl+T keyboard shortcut
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            if (e.ctrlKey && e.key === 't') { e.preventDefault(); toggleView(); }
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, [toggleView]);

    const handleOpen = () => sendToPlugin('open_manifest');
    const handleSave = () => sendToPlugin('save_manifest');

    return (
        <div style={styles.bar}>
            <span style={styles.title}>
                {manifest ? 'Manifest Editor' : 'No manifest'}
                {isDirty && <span style={styles.dirty}> *</span>}
            </span>
            <div style={styles.actions}>
                <button
                    style={{ ...styles.btn, ...styles.btnActive }}
                    onClick={handleOpen}
                    title="Open .diaapp file"
                >
                    Open
                </button>
                <button
                    style={{ ...styles.btn, ...(isDirty ? styles.btnActive : styles.btnDisabled) }}
                    onClick={handleSave}
                    disabled={!isDirty}
                    title="Save (Ctrl+S)"
                >
                    Save
                </button>
                <button
                    style={{ ...styles.btn, ...(manifest ? styles.btnActive : styles.btnDisabled) }}
                    onClick={toggleView}
                    disabled={!manifest}
                    title="Toggle view (Ctrl+T)"
                >
                    {currentView === 'tree' ? 'Flow View' : currentView === 'flow' ? 'Lifecycle View' : 'Tree View'}
                </button>
            </div>
        </div>
    );
};

const styles: Record<string, React.CSSProperties> = {
    bar:        { display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '4px 8px', background: '#2d2d2d', borderBottom: '1px solid #333', flexShrink: 0, height: 32 },
    title:      { fontSize: 12, color: '#bbb', fontWeight: 500 },
    dirty:      { color: '#e8b25f' },
    actions:    { display: 'flex', gap: 6 },
    btn:        { fontSize: 11, padding: '2px 10px', borderRadius: 3, border: '1px solid #555', cursor: 'pointer', background: '#3c3c3c', color: '#ccc' },
    btnActive:  { borderColor: '#007acc', color: '#fff', background: '#0e4c7a' },
    btnDisabled:{ opacity: 0.4, cursor: 'default' },
};
