import { useManifestStore } from './ManifestStore';
import { useUndoStore } from './UndoStore';
import type { ManifestData } from './types';

function sendToPlugin(type: string, data: object = {}): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data } }, '*');
}

export const BLANK_MANIFEST: ManifestData = {
    version: 1,
    imports: [],
    processing_units: [
        {
            instance_id: 'NewProcessingUnit',
            type: 'NewProcessingUnit',
            frequency_hz: 30.0,
            dedicated_thread: false,
            initial_phase: 'InitPhase',
            phases: [{ instance_id: 'InitPhase', type: 'InitPhase', config: {} }],
            transitions: [],
            modules: [],
            config: {},
        },
    ],
};

export function performUndo(): void {
    const manifest = useManifestStore.getState().manifest as ManifestData | null;
    if (!manifest) return;
    const snapshot = useUndoStore.getState().undo(manifest);
    if (!snapshot) return;
    useManifestStore.getState().setManifest(snapshot);
    useManifestStore.getState().bumpManifestVersion();
    useManifestStore.getState().setDirty(useUndoStore.getState().isDirtyVsSaved());
    sendToPlugin('manifest_restored', { manifest: snapshot });
}

export function performRedo(): void {
    const manifest = useManifestStore.getState().manifest as ManifestData | null;
    if (!manifest) return;
    const snapshot = useUndoStore.getState().redo(manifest);
    if (!snapshot) return;
    useManifestStore.getState().setManifest(snapshot);
    useManifestStore.getState().bumpManifestVersion();
    useManifestStore.getState().setDirty(useUndoStore.getState().isDirtyVsSaved());
    sendToPlugin('manifest_restored', { manifest: snapshot });
}

export function performNew(isDirty: boolean): void {
    if (isDirty) {
        if (!window.confirm('Unsaved changes will be lost. Continue?')) return;
    }
    useUndoStore.getState().clearHistory();
    useManifestStore.getState().setManifest(BLANK_MANIFEST);
    useManifestStore.getState().setFilePath(null);
    useManifestStore.getState().setDirty(true);
    useUndoStore.getState().setSavedSnapshot(null);
    sendToPlugin('new_manifest', { manifest: BLANK_MANIFEST });
}
