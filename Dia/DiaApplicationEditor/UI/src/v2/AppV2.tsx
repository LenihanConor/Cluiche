import { useState, useEffect } from 'react';
import { useManifestStoreV2 } from './useManifestStoreV2';
import { useUndoStoreV2 } from './useUndoStoreV2';
import { useValidationStoreV2 } from './useValidationStoreV2';
import { useLiveStoreV2 } from './useLiveStoreV2';
import { GraphView } from './GraphView';
import { ModulePresenceGrid } from './ModulePresenceGrid';
import { StreamsTab } from './StreamsTab';
import { PUInspector } from './PUInspector';
import { StageConfiguration } from './StageConfiguration';
import { ValidationBarV2 } from './ValidationBarV2';
import { LiveConnectionButton } from './LiveConnectionButton';
import { LiveTransitionPanel } from './LiveTransitionPanel';
import type { ManifestStateV2 } from './types';

type Tab = 'graph' | 'presence' | 'streams';
type Selection = { type: 'pu'; id: string } | null;

export const AppV2: React.FC = () => {
    const [activeTab, setActiveTab] = useState<Tab>('graph');
    const [selection, setSelection] = useState<Selection>(null);

    const applyStateSnapshot = useManifestStoreV2((s) => s.applyStateSnapshot);
    const refreshState = useManifestStoreV2((s) => s.refreshState);
    const hasManifest = useManifestStoreV2((s) => s.hasManifest);
    const filePath = useManifestStoreV2((s) => s.filePath);
    const manifest = useManifestStoreV2((s) => s.manifest);
    const applyUndoResponse = useUndoStoreV2((s) => s.applyUndoResponse);
    const setValidationResult = useValidationStoreV2((s) => s.setResult);
    const setConnectionState = useLiveStoreV2((s) => s.setConnectionState);
    const setActiveStage = useLiveStoreV2((s) => s.setActiveStage);
    const updateModuleStates = useLiveStoreV2((s) => s.updateModuleStates);
    const updateStreamStates = useLiveStoreV2((s) => s.updateStreamStates);
    const clearLiveState = useLiveStoreV2((s) => s.clearLiveState);
    const connectionState = useLiveStoreV2((s) => s.connectionState);

    // Pull state from C++ on mount — handles the case where OnLoad fires before React is ready
    useEffect(() => {
        refreshState();
    }, [refreshState]);

    useEffect(() => {
        (window as any).DiaEditor_onDataChanged = (topic: string, data: unknown) => {
            const d = data as any;
            switch (topic) {
                case 'manifest.state':
                    if (d) applyStateSnapshot(d as ManifestStateV2);
                    break;
                case 'manifest.dirty':
                    break;
                case 'history.state':
                    if (d) applyUndoResponse({ canUndo: d.canUndo, canRedo: d.canRedo, isDirty: d.isDirty });
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
    }, [applyStateSnapshot, applyUndoResponse, setValidationResult, setConnectionState, setActiveStage, updateModuleStates, updateStreamStates, clearLiveState]);

    const stages = manifest?.stages?.map(s => s.name) ?? [];
    const isLive = connectionState === 'connected';

    const handlePUSelect = (puId: string | null) => {
        setSelection(puId ? { type: 'pu', id: puId } : null);
    };

    const handleStreamLabelClick = () => {
        setActiveTab('streams');
    };

    const fileBaseName = filePath
        ? filePath.replace(/\\/g, '/').split('/').pop()
        : null;

    const renderSidebar = () => {
        if (selection?.type === 'pu') {
            return <PUInspector puId={selection.id} />;
        }
        return (
            <div style={{ padding: 12 }}>
                <StageConfiguration />
                <div style={{ padding: '8px 0', color: '#888', fontSize: 12 }}>
                    Select a PU to inspect
                </div>
            </div>
        );
    };

    const renderNoManifest = () => (
        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', flexDirection: 'column', gap: 8, color: '#888' }}>
            <div style={{ fontSize: 13 }}>No .diaapp manifest loaded</div>
            <div style={{ fontSize: 11, color: '#666' }}>Load a project from the taskbar to begin</div>
        </div>
    );

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100vh', background: '#1e1e1e', color: '#ccc', fontFamily: 'sans-serif' }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', height: 40, background: '#2d2d2d', borderBottom: '1px solid #444', padding: '0 8px', gap: 8 }}>
                <span style={{ fontWeight: 600, fontSize: 13 }}>Application Flow Editor</span>
                {fileBaseName && (
                    <span style={{ fontSize: 11, color: '#aaa', marginLeft: 4 }}>{fileBaseName}</span>
                )}
                <span style={{ flex: 1 }} />
                {isLive && <LiveTransitionPanel stages={stages} />}
                <LiveConnectionButton />
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
                {!hasManifest ? renderNoManifest() : (
                    <>
                        {/* Tab content */}
                        <div style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
                            {activeTab === 'graph' && (
                                <div id="graph-tab-content" style={{ height: '100%' }}>
                                    <GraphView
                                        onStreamLabelClick={handleStreamLabelClick}
                                        onPUSelect={handlePUSelect}
                                    />
                                </div>
                            )}
                            {activeTab === 'presence' && (
                                <div id="presence-tab-content" style={{ height: '100%' }}>
                                    <ModulePresenceGrid />
                                </div>
                            )}
                            {activeTab === 'streams' && (
                                <div id="streams-tab-content" style={{ height: '100%' }}>
                                    <StreamsTab />
                                </div>
                            )}
                        </div>

                        {/* Sidebar */}
                        <div id="sidebar-container" style={{ width: 280, borderLeft: '1px solid #444', overflow: 'auto', background: '#252526' }}>
                            {renderSidebar()}
                        </div>
                    </>
                )}
            </div>

            {/* Footer */}
            <ValidationBarV2 />
        </div>
    );
};
