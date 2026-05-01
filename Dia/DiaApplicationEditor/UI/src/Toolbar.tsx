import React from 'react';
import { useManifestStore } from './ManifestStore';

function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

type ViewMode = 'tree' | 'flow' | 'lifecycle';
const VIEWS: { id: ViewMode; label: string }[] = [
    { id: 'tree',      label: 'Tree' },
    { id: 'lifecycle', label: 'Lifecycle' },
    { id: 'flow',      label: 'Flow' },
];

export const Toolbar: React.FC = () => {
    const manifest    = useManifestStore(s => s.manifest);
    const isDirty     = useManifestStore(s => s.isDirty);
    const currentView = useManifestStore(s => s.currentView);
    const setView     = useManifestStore(s => s.setView);

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

                <div style={styles.divider} />

                <div style={styles.segmented}>
                    {VIEWS.map(({ id, label }, i) => {
                        const isActive = currentView === id;
                        const isFirst  = i === 0;
                        const isLast   = i === VIEWS.length - 1;
                        return (
                            <button
                                key={id}
                                style={{
                                    ...styles.seg,
                                    ...(isActive ? styles.segActive : styles.segInactive),
                                    ...(!manifest ? styles.btnDisabled : {}),
                                    borderRadius: isFirst ? '3px 0 0 3px' : isLast ? '0 3px 3px 0' : 0,
                                    borderRight: isLast ? undefined : 'none',
                                }}
                                onClick={() => manifest && setView(id)}
                                disabled={!manifest}
                                title={`${label} view`}
                            >
                                {label}
                            </button>
                        );
                    })}
                </div>
            </div>
        </div>
    );
};

const styles: Record<string, React.CSSProperties> = {
    bar:        { display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '4px 8px', background: '#2d2d2d', borderBottom: '1px solid #333', flexShrink: 0, height: 32 },
    title:      { fontSize: 12, color: '#bbb', fontWeight: 500 },
    dirty:      { color: '#e8b25f' },
    actions:    { display: 'flex', alignItems: 'center', gap: 6 },
    btn:        { fontSize: 11, padding: '2px 10px', borderRadius: 3, border: '1px solid #555', cursor: 'pointer', background: '#3c3c3c', color: '#ccc' },
    btnActive:  { borderColor: '#007acc', color: '#fff', background: '#0e4c7a' },
    btnDisabled:{ opacity: 0.4, cursor: 'default' },
    divider:    { width: 1, height: 18, background: '#555', margin: '0 2px' },
    segmented:  { display: 'flex' },
    seg:        { fontSize: 11, padding: '2px 10px', border: '1px solid #555', cursor: 'pointer', background: '#3c3c3c', color: '#ccc' },
    segActive:  { borderColor: '#007acc', color: '#fff', background: '#0e4c7a' },
    segInactive:{ borderColor: '#555', color: '#ccc', background: '#3c3c3c' },
};
