import React, { useState, useEffect, useCallback, useRef } from 'react';
import CodeMirror from '@uiw/react-codemirror';
import { json } from '@codemirror/lang-json';
import { linter } from '@codemirror/lint';
import { FormView } from './FormView';
import { jsonLinter } from './jsonLinter';
import { useManifestStore } from './ManifestStore';
import type { ManifestData, ProcessingUnitData, PhaseData } from './types';

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

interface PhaseInspectorProps {
    nodeId: string;
}

export const PhaseInspector: React.FC<PhaseInspectorProps> = ({ nodeId }) => {
    const manifest = useManifestStore(s => s.manifest) as ManifestData | null;
    const setDirty = useManifestStore(s => s.setDirty);
    const pushUndo = useManifestStore(s => s.pushUndo);

    const [puId, phaseId] = nodeId.split('_');
    const pu = manifest?.processing_units.find(p => p.instance_id === puId) ?? null;
    const phase = pu?.phases.find(p => p.instance_id === phaseId) ?? null;

    const [editMode, setEditMode] = useState<'form' | 'json'>('form');
    const [configText, setConfigText] = useState('{}');
    const [isPending, setIsPending] = useState(false);
    const debounceTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
    const undoPushed = useRef(false);

    const isInitial = pu?.initial_phase === phaseId;

    useEffect(() => {
        if (phase?.config) {
            setConfigText(JSON.stringify(phase.config, null, 2));
        } else {
            setConfigText('{}');
        }
        setIsPending(false);
        undoPushed.current = false;
    }, [nodeId, phase]);

    const applyConfig = useCallback((text: string) => {
        if (!pu || !phase) return;
        try {
            const parsed = JSON.parse(text);
            if (!undoPushed.current) {
                pushUndo('Edit phase config');
                undoPushed.current = true;
            }
            sendToPlugin('phase_property_changed', {
                pu_id: puId,
                phase_id: phaseId,
                property: 'config',
                value: parsed,
            });
            setDirty(true);
            setIsPending(false);
        } catch { /* invalid JSON */ }
    }, [pu, phase, puId, phaseId, setDirty, pushUndo]);

    const handleChange = useCallback((value: string) => {
        setConfigText(value);
        setIsPending(true);
        if (debounceTimer.current) clearTimeout(debounceTimer.current);
        debounceTimer.current = setTimeout(() => applyConfig(value), 500);
    }, [applyConfig]);

    const handleFormFieldChange = useCallback((field: string, value: unknown) => {
        try {
            const current = JSON.parse(configText);
            current[field] = value;
            const next = JSON.stringify(current, null, 2);
            handleChange(next);
        } catch { /* ignore */ }
    }, [configText, handleChange]);

    const handleInitialToggle = useCallback(() => {
        if (isInitial) return;
        pushUndo('Change initial phase');
        sendToPlugin('initial_phase_changed', { pu_id: puId, phase_id: phaseId });
        setDirty(true);
    }, [isInitial, puId, phaseId, setDirty, pushUndo]);

    if (!phase || !pu) {
        return <div style={{ padding: 12, color: '#555', fontSize: 12 }}>Phase not found</div>;
    }

    const config = (() => { try { return JSON.parse(configText) as Record<string, unknown>; } catch { return {} as Record<string, unknown>; } })();

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div style={headerStyle}>
                <div>
                    <span style={{ fontWeight: 600, color: '#fff', fontSize: 12 }}>Phase</span>
                    <span style={{ color: '#666', fontSize: 11, marginLeft: 8 }}>{phase.type}</span>
                </div>
                {isPending && <span style={{ color: '#e8b25f', fontSize: 11 }}>saving…</span>}
            </div>

            <div style={{ padding: 8, borderBottom: '1px solid #333', flexShrink: 0 }}>
                <FieldGroup label="Instance ID">
                    <span style={valueStyle}>{phase.instance_id}</span>
                </FieldGroup>

                <FieldGroup label="Type">
                    <span style={valueStyle}>{phase.type}</span>
                </FieldGroup>

                <FieldGroup label="Initial Phase">
                    <label style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, color: '#ccc' }}>
                        <input
                            type="checkbox"
                            checked={isInitial}
                            onChange={handleInitialToggle}
                            disabled={isInitial}
                        />
                        {isInitial ? 'This is the initial phase' : 'Set as initial phase'}
                    </label>
                </FieldGroup>
            </div>

            <div style={toolbarStyle}>
                <button style={{ ...tabBtn, ...(editMode === 'form' ? tabActive : {}) }} onClick={() => setEditMode('form')}>Form</button>
                <button style={{ ...tabBtn, ...(editMode === 'json' ? tabActive : {}) }} onClick={() => setEditMode('json')}>JSON</button>
            </div>

            <div style={{ flex: 1, overflow: 'auto' }}>
                {editMode === 'form' ? (
                    <FormView config={config} onChange={handleFormFieldChange} />
                ) : (
                    <CodeMirror
                        value={configText}
                        height="100%"
                        theme="dark"
                        extensions={[json(), linter(jsonLinter)]}
                        onChange={handleChange}
                        basicSetup={{ lineNumbers: true, foldGutter: false }}
                    />
                )}
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
    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
    padding: '4px 8px', borderBottom: '1px solid #333', flexShrink: 0,
};
const toolbarStyle: React.CSSProperties = {
    display: 'flex', gap: 1, padding: '4px 8px', borderBottom: '1px solid #333', flexShrink: 0,
};
const tabBtn: React.CSSProperties = {
    fontSize: 11, padding: '2px 10px', borderRadius: 3, border: '1px solid #444',
    cursor: 'pointer', background: '#2a2a2a', color: '#aaa',
};
const tabActive: React.CSSProperties = { borderColor: '#007acc', color: '#fff', background: '#0e4c7a' };
const valueStyle: React.CSSProperties = { color: '#ccc', fontSize: 12 };
