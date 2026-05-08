import React, { useState, useEffect, useCallback, useRef } from 'react';
import CodeMirror from '@uiw/react-codemirror';
import { json } from '@codemirror/lang-json';
import { linter } from '@codemirror/lint';
import { FormView } from './FormView';
import { jsonLinter } from './jsonLinter';
import { useManifestStore } from './ManifestStore';
import type { ManifestData, ModuleData, ProcessingUnitData } from './types';

function sendToPlugin(type: string, data: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

function findModule(manifest: ManifestData, nodeId: string): { pu: ProcessingUnitData; mod: ModuleData } | null {
    // nodeId is "puId_phaseId_moduleId" — module instanceId is the last segment
    const parts = nodeId.split('_');
    if (parts.length < 3) return null;
    const puId     = parts[0];
    const moduleId = parts[parts.length - 1];
    const pu = manifest.processing_units.find(p => p.instance_id === puId);
    if (!pu) return null;
    const mod = pu.modules.find(m => m.instance_id === moduleId);
    if (!mod) return null;
    return { pu, mod };
}

export const ModuleInspector: React.FC = () => {
    const manifest       = useManifestStore(s => s.manifest);
    const selectedNode   = useManifestStore(s => s.selectedNode);
    const setDirty       = useManifestStore(s => s.setDirty);

    const [editMode, setEditMode] = useState<'form' | 'json'>('form');
    const [configText, setConfigText] = useState('{}');
    const [isPending, setIsPending] = useState(false);
    const debounceTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

    const entry = (manifest && selectedNode)
        ? findModule(manifest as ManifestData, selectedNode)
        : null;

    useEffect(() => {
        if (entry?.mod.config) {
            setConfigText(JSON.stringify(entry.mod.config, null, 2));
        } else {
            setConfigText('{}');
        }
        setIsPending(false);
        undoPushed.current = false;
    }, [selectedNode, manifest]);

    const undoPushed = useRef(false);

    const applyConfig = useCallback((text: string, pu: ProcessingUnitData, mod: ModuleData) => {
        try {
            const parsed = JSON.parse(text);
            if (!undoPushed.current) {
                useManifestStore.getState().pushUndo('Edit module config');
                undoPushed.current = true;
            }
            sendToPlugin('module_config_changed', {
                processing_unit: pu.instance_id,
                module_id: mod.instance_id,
                config: parsed,
            });
            setDirty(true);
            setIsPending(false);
        } catch { /* invalid JSON — wait for valid input */ }
    }, [setDirty]);

    const handleChange = useCallback((value: string) => {
        setConfigText(value);
        setIsPending(true);
        if (debounceTimer.current) clearTimeout(debounceTimer.current);
        if (!entry) return;
        debounceTimer.current = setTimeout(() => applyConfig(value, entry.pu, entry.mod), 500);
    }, [entry, applyConfig]);

    const handleFormFieldChange = useCallback((field: string, value: unknown) => {
        try {
            const current = JSON.parse(configText);
            current[field] = value;
            const next = JSON.stringify(current, null, 2);
            handleChange(next);
        } catch { /* ignore */ }
    }, [configText, handleChange]);

    if (!entry) {
        return (
            <div style={{ padding: 12, color: '#555', fontSize: 12 }}>
                {selectedNode ? 'Select a module node to edit its config' : 'Select a node'}
            </div>
        );
    }

    const { mod } = entry;
    const config = (() => { try { return JSON.parse(configText) as Record<string, unknown>; } catch { return {} as Record<string, unknown>; } })();

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div style={headerStyle}>
                <div>
                    <span style={{ fontWeight: 600, color: '#fff', fontSize: 12 }}>{mod.instance_id}</span>
                    <span style={{ color: '#666', fontSize: 11, marginLeft: 8 }}>{mod.type}</span>
                </div>
                {isPending && <span style={{ color: '#e8b25f', fontSize: 11 }}>saving…</span>}
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
