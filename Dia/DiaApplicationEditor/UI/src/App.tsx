import React, { useCallback, useEffect } from 'react';
import { Group as PanelGroup, Panel, Separator as PanelResizeHandle } from 'react-resizable-panels';
import { ValidationPanel } from './ValidationPanel';
import { TreeView } from './TreeView';
import { FlowView } from './FlowView';
import { LifecycleView } from './LifecycleView';
import { Toolbar } from './Toolbar';
import { InspectorPanel } from './InspectorPanel';
import { ImportsPanel } from './ImportsPanel';
import { ConflictBanner, useConflictDetection } from './ConflictBanner';
import { TypeSelectorModal } from './TypeSelectorModal';
import { useManifestStore } from './ManifestStore';
import { useUndoStore } from './UndoStore';
import { useValidation } from './useValidation';
import { performUndo, performRedo, performNew } from './undoActions';

function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const App: React.FC = () => {
    const manifestVersion = useManifestStore(s => s.manifestVersion);
    const manifest        = useManifestStore(s => s.manifest);
    const currentView     = useManifestStore(s => s.currentView);
    const selectedNode    = useManifestStore(s => s.selectedNode);
    const isDirty         = useManifestStore(s => s.isDirty);
    useValidation(manifestVersion);

    const { conflict, dismiss } = useConflictDetection();

    const showInspector = manifest && selectedNode;

    // Clear undo history when manifest is loaded or closed externally
    useEffect(() => {
        const handler = (e: MessageEvent) => {
            const payload = e.data?.payload ?? e.data;
            if (!payload || typeof payload !== 'object') return;
            const type = payload.type ?? payload.topic;
            if (type === 'manifest_loaded' || type === 'manifest_closed') {
                useUndoStore.getState().clearHistory();
                useUndoStore.getState().setSavedSnapshot(
                    useManifestStore.getState().manifest as any
                );
            }
            if (type === 'manifest_saved') {
                useUndoStore.getState().markSaved();
            }
        };
        window.addEventListener('message', handler);
        return () => window.removeEventListener('message', handler);
    }, []);

    // Keyboard shortcuts: Ctrl+Z, Ctrl+Y/Shift+Z, Ctrl+N
    useEffect(() => {
        const onKey = (e: KeyboardEvent) => {
            if (e.ctrlKey && e.key === 'z' && !e.shiftKey) {
                e.preventDefault();
                performUndo();
            }
            if (e.ctrlKey && (e.key === 'y' || (e.shiftKey && e.key === 'Z'))) {
                e.preventDefault();
                performRedo();
            }
            if (e.ctrlKey && e.key === 'n') {
                e.preventDefault();
                performNew(isDirty);
            }
        };
        window.addEventListener('keydown', onKey);
        return () => window.removeEventListener('keydown', onKey);
    }, [isDirty]);

    // Prevent CEF from navigating the iframe when a file is dropped anywhere
    // other than the explicit drop zone.
    useEffect(() => {
        const stop = (e: DragEvent) => e.preventDefault();
        document.addEventListener('dragover', stop);
        document.addEventListener('drop', stop);
        return () => {
            document.removeEventListener('dragover', stop);
            document.removeEventListener('drop', stop);
        };
    }, []);

    const handleDragOver = useCallback((e: React.DragEvent) => {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'copy';
    }, []);

    const handleDrop = useCallback((e: React.DragEvent) => {
        e.preventDefault();
        e.stopPropagation();
        const file = e.dataTransfer.files[0];
        if (!file) return;
        const path = (file as File & { path?: string }).path;
        if (path) sendToPlugin('open_manifest', { path });
    }, []);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            <Toolbar />

            <ImportsPanel />
            {conflict && <ConflictBanner conflict={conflict} onDismiss={dismiss} />}
            <TypeSelectorModal />

            <PanelGroup orientation="vertical" id="editor-vertical" style={{ flex: 1, overflow: 'hidden' }}>
                <Panel id="main" defaultSize="75%" minSize="30%">
                    {!manifest ? (
                        <div
                            onDragOver={handleDragOver}
                            onDrop={handleDrop}
                            style={{ height: '100%', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 8, color: '#555', border: '2px dashed transparent', transition: 'border-color 0.15s' }}
                            onDragEnter={e => (e.currentTarget as HTMLDivElement).style.borderColor = '#007acc'}
                            onDragLeave={e => (e.currentTarget as HTMLDivElement).style.borderColor = 'transparent'}
                        >
                            <span style={{ fontSize: 32 }}>&#x1F4C2;</span>
                            <span style={{ fontSize: 13 }}>Drop a .diaapp file here</span>
                            <span style={{ fontSize: 11, color: '#444' }}>or use the Open button above</span>
                        </div>
                    ) : (
                        <PanelGroup orientation="horizontal" id="editor-horizontal" style={{ height: '100%' }}>
                            <Panel id="content" defaultSize="75%" minSize="30%">
                                <div style={{ height: '100%', overflow: 'hidden' }}>
                                    {currentView === 'tree' ? <TreeView /> : currentView === 'flow' ? <FlowView /> : <LifecycleView />}
                                </div>
                            </Panel>
                            {showInspector && (
                                <>
                                    <PanelResizeHandle style={{ width: 4, background: '#333', cursor: 'col-resize' }} />
                                    <Panel id="inspector" defaultSize="25%" minSize="15%">
                                        <div style={{ height: '100%', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                                            <InspectorPanel />
                                        </div>
                                    </Panel>
                                </>
                            )}
                        </PanelGroup>
                    )}
                </Panel>
                <PanelResizeHandle style={{ height: 4, background: '#333', cursor: 'row-resize' }} />
                <Panel id="validation" defaultSize="25%" minSize="10%">
                    <div style={{ height: '100%', overflow: 'hidden' }}>
                        <ValidationPanel />
                    </div>
                </Panel>
            </PanelGroup>
        </div>
    );
};
