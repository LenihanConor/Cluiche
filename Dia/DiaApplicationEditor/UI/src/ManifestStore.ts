import { create } from 'zustand';
import { ValidationResult } from './ValidationPanel';
import { useUndoStore } from './UndoStore';
import type { ManifestData } from './types';

type ViewMode = 'tree' | 'flow' | 'lifecycle';

const PREF_KEY = 'diaApplicationEditor.lastView';

function loadViewPref(): ViewMode {
    try { return (localStorage.getItem(PREF_KEY) as ViewMode) ?? 'tree'; }
    catch { return 'tree'; }
}

function saveViewPref(v: ViewMode): void {
    try { localStorage.setItem(PREF_KEY, v); } catch { /* ignore */ }
}

interface ManifestState {
    manifestVersion: number;
    bumpManifestVersion: () => void;

    manifest: object | null;
    filePath: string | null;
    setManifest: (m: object | null) => void;
    setFilePath: (p: string | null) => void;

    isDirty: boolean;
    setDirty: (d: boolean) => void;

    currentView: ViewMode;
    toggleView: () => void;
    setView: (v: ViewMode) => void;

    validationResult: ValidationResult | null;
    setValidationResult: (result: ValidationResult) => void;

    selectedNode: string | null;
    setSelectedNode: (nodeId: string | null) => void;

    pushUndo: (description: string) => void;
}

export const useManifestStore = create<ManifestState>((set, get) => ({
    manifestVersion: 0,
    bumpManifestVersion: () => set(s => ({ manifestVersion: s.manifestVersion + 1 })),

    manifest: null,
    filePath: null,
    setManifest: (m) => set({ manifest: m }),
    setFilePath: (p) => set({ filePath: p }),

    isDirty: false,
    setDirty: (d) => set({ isDirty: d }),

    currentView: loadViewPref(),
    toggleView: () => set(s => {
        const next = s.currentView === 'tree' ? 'flow' : s.currentView === 'flow' ? 'lifecycle' : 'tree';
        saveViewPref(next);
        useUndoStore.getState().clearHistory();
        return { currentView: next };
    }),
    setView: (v) => set(() => {
        saveViewPref(v);
        useUndoStore.getState().clearHistory();
        return { currentView: v };
    }),

    validationResult: null,
    setValidationResult: (result) => set({ validationResult: result }),

    selectedNode: null,
    setSelectedNode: (nodeId) => set({ selectedNode: nodeId }),

    pushUndo: (description: string) => {
        const { manifest } = get();
        if (manifest) {
            useUndoStore.getState().pushSnapshot(manifest as ManifestData, description);
        }
    },
}));
