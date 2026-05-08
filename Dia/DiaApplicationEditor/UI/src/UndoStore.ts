import { create } from 'zustand';
import type { ManifestData } from './types';

interface HistoryEntry {
    snapshot: ManifestData;
    description: string;
}

interface UndoState {
    undoStack: HistoryEntry[];
    redoStack: HistoryEntry[];
    savedSnapshot: ManifestData | null;
    savedVersion: number;
    currentVersion: number;

    pushSnapshot: (manifest: ManifestData, description: string) => void;
    undo: (currentManifest: ManifestData) => ManifestData | null;
    redo: (currentManifest: ManifestData) => ManifestData | null;
    clearHistory: () => void;
    setSavedSnapshot: (manifest: ManifestData | null) => void;
    markSaved: () => void;
    isDirtyVsSaved: () => boolean;
    canUndo: boolean;
    canRedo: boolean;
}

const MAX_HISTORY = 8;

function deepClone<T>(obj: T): T {
    return JSON.parse(JSON.stringify(obj));
}

export const useUndoStore = create<UndoState>((set, get) => ({
    undoStack: [],
    redoStack: [],
    savedSnapshot: null,
    savedVersion: 0,
    currentVersion: 0,
    canUndo: false,
    canRedo: false,

    pushSnapshot: (manifest: ManifestData, description: string) => {
        set(s => {
            const stack = [...s.undoStack, { snapshot: deepClone(manifest), description }];
            if (stack.length > MAX_HISTORY) stack.shift();
            return {
                undoStack: stack,
                redoStack: [],
                canUndo: true,
                canRedo: false,
                currentVersion: s.currentVersion + 1,
            };
        });
    },

    undo: (currentManifest: ManifestData) => {
        const { undoStack, redoStack } = get();
        if (undoStack.length === 0) return null;
        const entry = undoStack[undoStack.length - 1];
        const newUndo = undoStack.slice(0, -1);
        set(s => ({
            undoStack: newUndo,
            redoStack: [...redoStack, { snapshot: deepClone(currentManifest), description: 'redo' }],
            canUndo: newUndo.length > 0,
            canRedo: true,
            currentVersion: s.currentVersion + 1,
        }));
        return deepClone(entry.snapshot);
    },

    redo: (currentManifest: ManifestData) => {
        const { undoStack, redoStack } = get();
        if (redoStack.length === 0) return null;
        const entry = redoStack[redoStack.length - 1];
        const newRedo = redoStack.slice(0, -1);
        set(s => ({
            undoStack: [...undoStack, { snapshot: deepClone(currentManifest), description: 'undo' }],
            redoStack: newRedo,
            canUndo: true,
            canRedo: newRedo.length > 0,
            currentVersion: s.currentVersion + 1,
        }));
        return deepClone(entry.snapshot);
    },

    clearHistory: () => {
        set({ undoStack: [], redoStack: [], canUndo: false, canRedo: false });
    },

    setSavedSnapshot: (manifest: ManifestData | null) => {
        set({ savedSnapshot: manifest ? deepClone(manifest) : null });
    },

    markSaved: () => {
        set(s => ({ savedVersion: s.currentVersion }));
    },

    isDirtyVsSaved: () => {
        const { savedVersion, currentVersion } = get();
        return savedVersion !== currentVersion;
    },
}));
