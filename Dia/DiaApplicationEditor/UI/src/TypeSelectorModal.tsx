import React, { useState, useEffect, useCallback } from 'react';

interface TypeOption {
    type: string;
    description?: string;
}

interface TypeSelectorState {
    puId: string;
    phaseId?: string;
    nodeType: 'phase' | 'module';
    options: TypeOption[];
}

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const TypeSelectorModal: React.FC = () => {
    const [state, setState] = useState<TypeSelectorState | null>(null);
    const [instanceId, setInstanceId] = useState('');
    const [selectedType, setSelectedType] = useState('');
    const [error, setError] = useState('');

    useEffect(() => {
        const handler = (event: MessageEvent) => {
            const msg = event.data;
            if (!msg?.__dia) return;

            if (msg.topic === 'show_type_selector') {
                const d = msg.data as { pu_id: string; phase_id?: string; node_type: string; available_types: TypeOption[] };
                setState({
                    puId: d.pu_id,
                    phaseId: d.phase_id,
                    nodeType: d.node_type as 'phase' | 'module',
                    options: d.available_types ?? [],
                });
                setInstanceId('');
                setSelectedType(d.available_types?.[0]?.type ?? '');
                setError('');
            } else if (msg.topic === 'add_node_error') {
                setError(msg.data?.message ?? 'Failed to add node');
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    const handleConfirm = useCallback(() => {
        if (!state || !selectedType || !instanceId.trim()) return;
        setError('');
        sendToPlugin('add_node_confirmed', {
            pu_id: state.puId,
            phase_id: state.phaseId,
            node_type: state.nodeType,
            type_name: selectedType,
            instance_id: instanceId.trim(),
        });
        setState(null);
    }, [state, selectedType, instanceId]);

    const handleCancel = useCallback(() => { setState(null); setError(''); }, []);

    const handleKeyDown = useCallback((e: React.KeyboardEvent) => {
        if (e.key === 'Enter') handleConfirm();
        if (e.key === 'Escape') handleCancel();
    }, [handleConfirm, handleCancel]);

    if (!state) return null;

    const label = state.nodeType === 'module' ? 'Module' : 'Phase';

    return (
        <div style={overlayStyle} onClick={handleCancel}>
            <div style={modalStyle} onClick={e => e.stopPropagation()} onKeyDown={handleKeyDown}>
                <div style={titleStyle}>Add {label}</div>

                {error && <div style={errorStyle}>{error}</div>}

                <label style={labelStyle}>Type</label>
                {state.options.length === 0 ? (
                    <div style={{ color: '#e88', fontSize: 12, padding: '5px 0' }}>No types available</div>
                ) : (
                    <select
                        value={selectedType}
                        onChange={e => setSelectedType(e.target.value)}
                        style={inputStyle}
                    >
                        {state.options.map(opt => (
                            <option key={opt.type} value={opt.type}>
                                {opt.type}{opt.description ? ` — ${opt.description}` : ''}
                            </option>
                        ))}
                    </select>
                )}

                <label style={labelStyle}>Instance ID</label>
                <input
                    type="text"
                    value={instanceId}
                    onChange={e => setInstanceId(e.target.value)}
                    placeholder={`e.g. My${label}`}
                    style={inputStyle}
                    autoFocus
                />

                <div style={buttonRowStyle}>
                    <button style={btnCancel} onClick={handleCancel}>Cancel</button>
                    <button
                        style={btnConfirm}
                        onClick={handleConfirm}
                        disabled={!instanceId.trim() || !selectedType}
                    >
                        Add
                    </button>
                </div>
            </div>
        </div>
    );
};

const overlayStyle: React.CSSProperties = {
    position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.5)',
    display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 2000,
};
const modalStyle: React.CSSProperties = {
    background: '#1e1e1e', border: '1px solid #555', borderRadius: 6,
    padding: 16, minWidth: 300, maxWidth: 400,
};
const titleStyle: React.CSSProperties = {
    fontSize: 14, fontWeight: 600, color: '#fff', marginBottom: 12,
};
const errorStyle: React.CSSProperties = {
    background: '#3a1a1a', border: '1px solid #a44', borderRadius: 3,
    padding: '6px 10px', fontSize: 11, color: '#f88', marginBottom: 8,
};
const labelStyle: React.CSSProperties = {
    display: 'block', fontSize: 11, color: '#aaa', marginBottom: 4, marginTop: 8,
};
const inputStyle: React.CSSProperties = {
    width: '100%', background: '#2a2a2a', border: '1px solid #444',
    color: '#ccc', padding: '5px 8px', fontSize: 12, borderRadius: 3,
    boxSizing: 'border-box',
};
const buttonRowStyle: React.CSSProperties = {
    display: 'flex', justifyContent: 'flex-end', gap: 8, marginTop: 16,
};
const btnCancel: React.CSSProperties = {
    padding: '5px 14px', fontSize: 12, borderRadius: 3, border: '1px solid #444',
    background: '#2a2a2a', color: '#ccc', cursor: 'pointer',
};
const btnConfirm: React.CSSProperties = {
    padding: '5px 14px', fontSize: 12, borderRadius: 3, border: '1px solid #007acc',
    background: '#007acc', color: '#fff', cursor: 'pointer',
};
