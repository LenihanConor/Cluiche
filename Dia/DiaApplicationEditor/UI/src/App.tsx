import React, { useCallback, useEffect } from 'react';
import { ValidationPanel } from './ValidationPanel';
import { TreeView } from './TreeView';
import { FlowView } from './FlowView';
import { LifecycleView } from './LifecycleView';
import { Toolbar } from './Toolbar';
import { ModuleInspector } from './ModuleInspector';
import { ConflictBanner, useConflictDetection } from './ConflictBanner';
import { useManifestStore } from './ManifestStore';
import { useValidation } from './useValidation';

function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const App: React.FC = () => {
    const manifestVersion = useManifestStore(s => s.manifestVersion);
    const manifest        = useManifestStore(s => s.manifest);
    const currentView     = useManifestStore(s => s.currentView);
    const selectedNode    = useManifestStore(s => s.selectedNode);
    useValidation(manifestVersion);

    const { conflict, dismiss } = useConflictDetection();

    const showInspector = manifest && selectedNode && selectedNode.split('_').length >= 3;

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

            {conflict && <ConflictBanner conflict={conflict} onDismiss={dismiss} />}

            <div style={{ flex: 1, overflow: 'hidden', display: 'flex' }}>
                {!manifest ? (
                    <div
                        onDragOver={handleDragOver}
                        onDrop={handleDrop}
                        style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 8, color: '#555', border: '2px dashed transparent', transition: 'border-color 0.15s' }}
                        onDragEnter={e => (e.currentTarget as HTMLDivElement).style.borderColor = '#007acc'}
                        onDragLeave={e => (e.currentTarget as HTMLDivElement).style.borderColor = 'transparent'}
                    >
                        <span style={{ fontSize: 32 }}>&#x1F4C2;</span>
                        <span style={{ fontSize: 13 }}>Drop a .diaapp file here</span>
                        <span style={{ fontSize: 11, color: '#444' }}>or use the Open button above</span>
                    </div>
                ) : (
                    <>
                        <div style={{ flex: 1, overflow: 'hidden' }}>
                            {currentView === 'tree' ? <TreeView /> : currentView === 'flow' ? <FlowView /> : <LifecycleView />}
                        </div>
                        {showInspector && (
                            <div style={{ width: 280, borderLeft: '1px solid #333', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                                <ModuleInspector />
                            </div>
                        )}
                    </>
                )}
            </div>

            <div style={{ height: 140, borderTop: '1px solid #333', flexShrink: 0 }}>
                <ValidationPanel />
            </div>
        </div>
    );
};
