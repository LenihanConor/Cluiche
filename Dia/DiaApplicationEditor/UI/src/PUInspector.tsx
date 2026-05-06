import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useManifestStore } from './ManifestStore';
import { FormView } from './FormView';
import type { ManifestData, ProcessingUnitData } from './types';

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

interface PUInspectorProps {
    puId: string;
}

export const PUInspector: React.FC<PUInspectorProps> = ({ puId }) => {
    const manifest = useManifestStore(s => s.manifest) as ManifestData | null;
    const setDirty = useManifestStore(s => s.setDirty);
    const pushUndo = useManifestStore(s => s.pushUndo);

    const pu = manifest?.processing_units.find(p => p.instance_id === puId) ?? null;

    const [instanceId, setInstanceId] = useState(pu?.instance_id ?? '');
    const [frequencyHz, setFrequencyHz] = useState(pu?.frequency_hz ?? 30);
    const [dedicatedThread, setDedicatedThread] = useState(pu?.dedicated_thread ?? false);
    const [initialPhase, setInitialPhase] = useState(pu?.initial_phase ?? '');
    const debounceTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

    useEffect(() => {
        if (pu) {
            setInstanceId(pu.instance_id ?? '');
            setFrequencyHz(pu.frequency_hz ?? 30);
            setDedicatedThread(pu.dedicated_thread ?? false);
            setInitialPhase(pu.initial_phase ?? '');
        }
    }, [puId, pu]);

    const sendChange = useCallback((property: string, value: unknown) => {
        if (debounceTimer.current) clearTimeout(debounceTimer.current);
        debounceTimer.current = setTimeout(() => {
            pushUndo(`Edit PU ${property}`);
            sendToPlugin('pu_property_changed', { pu_id: puId, property, value });
            setDirty(true);
        }, 500);
    }, [puId, setDirty, pushUndo]);

    const handleFrequencyChange = useCallback((v: number) => {
        setFrequencyHz(v);
        sendChange('frequency_hz', v);
    }, [sendChange]);

    const handleThreadChange = useCallback((v: boolean) => {
        setDedicatedThread(v);
        sendChange('dedicated_thread', v);
    }, [sendChange]);

    const handleInstanceIdChange = useCallback((v: string) => {
        setInstanceId(v);
        sendChange('instance_id', v);
    }, [sendChange]);

    const handleInitialPhaseChange = useCallback((v: string) => {
        setInitialPhase(v);
        pushUndo('Change initial phase');
        sendToPlugin('initial_phase_changed', { pu_id: puId, phase_id: v });
        setDirty(true);
    }, [puId, setDirty, pushUndo]);

    if (!pu) {
        return <div style={{ padding: 12, color: '#555', fontSize: 12 }}>PU not found</div>;
    }

    const phaseOptions = pu.phases.map(p => p.instance_id);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div style={headerStyle}>
                <span style={{ fontWeight: 600, color: '#fff', fontSize: 12 }}>Processing Unit</span>
                <span style={{ color: '#666', fontSize: 11, marginLeft: 8 }}>{pu.type}</span>
            </div>

            <div style={{ flex: 1, overflow: 'auto', padding: 8 }}>
                <FieldGroup label="Instance ID">
                    <input
                        type="text"
                        value={instanceId}
                        onChange={e => handleInstanceIdChange(e.target.value)}
                        style={inputStyle}
                    />
                </FieldGroup>

                <FieldGroup label="Type">
                    <span style={valueStyle}>{pu.type}</span>
                </FieldGroup>

                <FieldGroup label="Frequency (Hz)">
                    <input
                        type="number"
                        value={frequencyHz}
                        onChange={e => handleFrequencyChange(parseFloat(e.target.value) || 0)}
                        style={inputStyle}
                        step={1}
                        min={0}
                    />
                </FieldGroup>

                <FieldGroup label="Dedicated Thread">
                    <input
                        type="checkbox"
                        checked={dedicatedThread}
                        onChange={e => handleThreadChange(e.target.checked)}
                    />
                </FieldGroup>

                <FieldGroup label="Initial Phase">
                    <select
                        value={initialPhase}
                        onChange={e => handleInitialPhaseChange(e.target.value)}
                        style={inputStyle}
                    >
                        {phaseOptions.map(p => (
                            <option key={p} value={p}>{p}</option>
                        ))}
                    </select>
                </FieldGroup>
            </div>
        </div>
    );
};

const FieldGroup: React.FC<{ label: string; children: React.ReactNode }> = ({ label, children }) => (
    <div style={{ marginBottom: 8 }}>
        <label style={{ display: 'block', fontSize: 10, color: '#888', marginBottom: 2 }}>{label}</label>
        {children}
    </div>
);

const headerStyle: React.CSSProperties = {
    display: 'flex', alignItems: 'center', padding: '4px 8px',
    borderBottom: '1px solid #333', flexShrink: 0,
};
const inputStyle: React.CSSProperties = {
    width: '100%', background: '#2a2a2a', border: '1px solid #444',
    color: '#ccc', padding: '3px 6px', fontSize: 12, borderRadius: 3,
};
const valueStyle: React.CSSProperties = { color: '#ccc', fontSize: 12 };
