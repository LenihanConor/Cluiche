import React, { useState, useEffect } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useUndoStoreV2 } from './useUndoStoreV2';
import { useValidationStoreV2 } from './useValidationStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import type { ManifestStateV2 } from './types';

type Tab = 'graph' | 'presence' | 'streams';

export const AppV2: React.FC = () => {
    const [activeTab, setActiveTab] = useState<Tab>('graph');
    const applyStateSnapshot = useManifestStoreV2((s) => s.applyStateSnapshot);
    const syncUndoFromBackend = useUndoStoreV2((s) => s.syncFromBackend);
    const setValidationResult = useValidationStoreV2((s) => s.setResult);
    const setConnectionState = useLiveStoreV2((s) => s.setConnectionState);
    const setActiveStage = useLiveStoreV2((s) => s.setActiveStage);
    const updateModuleStates = useLiveStoreV2((s) => s.updateModuleStates);
    const updateStreamStates = useLiveStoreV2((s) => s.updateStreamStates);
    const clearLiveState = useLiveStoreV2((s) => s.clearLiveState);

    useEffect(() => {
        (window as any).DiaEditor_onDataChanged = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
                case 'manifest.state':
                    if (d) applyStateSnapshot(d as ManifestStateV2);
                    break;
                case 'manifest.dirty':
                    // handled via full manifest.state push; nothing extra needed
                    break;
                case 'history.state':
                    if (d) syncUndoFromBackend({ canUndo: d.canUndo, canRedo: d.canRedo, count: d.count, isDirty: d.isDirty });
                    break;
                case 'validation.result':
                    if (d) setValidationResult({ errorCount: d.errorCount ?? 0, warningCount: d.warningCount ?? 0, issues: d.issues ?? [] });
                    break;
                case 'live.connected':
                    setConnectionState('connected');
                    if (d?.activeStage !== undefined) setActiveStage(d.activeStage);
                    break;
                case 'live.disconnected':
                    clearLiveState();
                    break;
                case 'live.appState':
                    if (d?.activeStage !== undefined) setActiveStage(d.activeStage);
                    break;
                case 'live.moduleStates':
                    if (Array.isArray(d)) updateModuleStates(d);
                    break;
                case 'live.streamStates':
                    if (Array.isArray(d)) updateStreamStates(d);
                    break;
            }
        };
    }, [applyStateSnapshot, syncUndoFromBackend, setValidationResult, setConnectionState, setActiveStage, updateModuleStates, updateStreamStates, clearLiveState]);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100vh', background: '#1e1e1e', color: '#ccc', fontFamily: 'sans-serif' }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', height: 40, background: '#2d2d2d', borderBottom: '1px solid #444', padding: '0 8px', gap: 8 }}>
                <span style={{ fontWeight: 600, fontSize: 13 }}>Application Flow Editor</span>
                <span style={{ flex: 1 }} />
                {/* LiveConnectionButton placeholder */}
                <span id="live-connection-slot" />
            </div>

            {/* Tab bar */}
            <div style={{ display: 'flex', background: '#252526', borderBottom: '1px solid #444' }}>
                {(['graph', 'presence', 'streams'] as Tab[]).map(tab => (
                    <button
                        key={tab}
                        onClick={() => setActiveTab(tab)}
                        style={{
                            padding: '6px 16px',
                            background: activeTab === tab ? '#1e1e1e' : 'transparent',
                            border: 'none',
                            borderBottom: activeTab === tab ? '2px solid #007acc' : '2px solid transparent',
                            color: activeTab === tab ? '#fff' : '#ccc',
                            cursor: 'pointer',
                            fontSize: 12,
                            textTransform: 'capitalize',
                        }}
                    >
                        {tab === 'graph' ? 'Graph' : tab === 'presence' ? 'Presence' : 'Streams'}
                    </button>
                ))}
            </div>

            {/* Main content + sidebar */}
            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
                {/* Tab content */}
                <div style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
                    {activeTab === 'graph' && <div id="graph-tab-content" style={{ height: '100%' }}>Graph view (coming soon)</div>}
                    {activeTab === 'presence' && <div id="presence-tab-content" style={{ height: '100%' }}>Presence grid (coming soon)</div>}
                    {activeTab === 'streams' && <div id="streams-tab-content" style={{ height: '100%' }}>Streams tab (coming soon)</div>}
                </div>

                {/* Sidebar */}
                <div id="sidebar-container" style={{ width: 280, borderLeft: '1px solid #444', overflow: 'auto', background: '#252526' }}>
                    <div style={{ padding: 12, color: '#888', fontSize: 12 }}>Select a node to inspect</div>
                </div>
            </div>

            {/* Footer: ValidationBar placeholder */}
            <div id="validation-bar-slot" style={{ height: 28, background: '#007acc', display: 'flex', alignItems: 'center', padding: '0 8px', fontSize: 11 }}>
                No manifest loaded
            </div>
        </div>
    );
};
