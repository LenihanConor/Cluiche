import React from 'react';

interface FormViewProps {
    config: Record<string, unknown>;
    onChange: (field: string, value: unknown) => void;
}

export const FormView: React.FC<FormViewProps> = ({ config, onChange }) => {
    const entries = Object.entries(config);

    if (entries.length === 0) {
        return <div style={{ padding: 8, color: '#666', fontSize: 12 }}>No config properties</div>;
    }

    return (
        <div style={{ padding: '6px 8px', display: 'flex', flexDirection: 'column', gap: 6 }}>
            {entries.map(([key, value]) => {
                if (typeof value === 'boolean') {
                    return (
                        <label key={key} style={labelStyle}>
                            <input
                                type="checkbox"
                                checked={value}
                                onChange={e => onChange(key, e.target.checked)}
                                style={{ marginRight: 6 }}
                            />
                            <span style={keyStyle}>{key}</span>
                        </label>
                    );
                }
                if (typeof value === 'number') {
                    return (
                        <label key={key} style={labelStyle}>
                            <span style={keyStyle}>{key}</span>
                            <input
                                type="number"
                                value={value}
                                onChange={e => onChange(key, parseFloat(e.target.value))}
                                style={inputStyle}
                            />
                        </label>
                    );
                }
                if (typeof value === 'string') {
                    return (
                        <label key={key} style={labelStyle}>
                            <span style={keyStyle}>{key}</span>
                            <input
                                type="text"
                                value={value}
                                onChange={e => onChange(key, e.target.value)}
                                style={inputStyle}
                            />
                        </label>
                    );
                }
                // Object/array — only editable in JSON view
                return (
                    <div key={key} style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 12 }}>
                        <span style={keyStyle}>{key}</span>
                        <span style={{ color: '#666' }}>(edit in JSON view)</span>
                    </div>
                );
            })}
        </div>
    );
};

const labelStyle: React.CSSProperties = { display: 'flex', alignItems: 'center', gap: 8, fontSize: 12, color: '#ccc' };
const keyStyle: React.CSSProperties = { color: '#9cdcfe', minWidth: 100, flexShrink: 0 };
const inputStyle: React.CSSProperties = { background: '#2a2a2a', border: '1px solid #555', color: '#ccc', padding: '2px 6px', borderRadius: 3, fontSize: 12, flex: 1 };
