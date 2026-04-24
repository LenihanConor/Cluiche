import { create } from 'zustand';
import { ValidationResult } from './ValidationPanel';

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
    setManifest: (m: object | null) => void;

    isDirty: boolean;
    setDirty: (d: boolean) => void;

    currentView: ViewMode;
    toggleView: () => void;

    validationResult: ValidationResult | null;
    setValidationResult: (result: ValidationResult) => void;

    selectedNode: string | null;
    setSelectedNode: (nodeId: string | null) => void;
}

export const useManifestStore = create<ManifestState>((set) => ({
    manifestVersion: 0,
    bumpManifestVersion: () => set(s => ({ manifestVersion: s.manifestVersion + 1 })),

    manifest: null,
    setManifest: (m) => set({ manifest: m }),

    isDirty: false,
    setDirty: (d) => set({ isDirty: d }),

    currentView: loadViewPref(),
    toggleView: () => set(s => {
        const next = s.currentView === 'tree' ? 'flow' : s.currentView === 'flow' ? 'lifecycle' : 'tree';
        saveViewPref(next);
        return { currentView: next };
    }),

    validationResult: null,
    setValidationResult: (result) => set({ validationResult: result }),

    selectedNode: null,
    setSelectedNode: (nodeId) => set({ selectedNode: nodeId }),
}));
