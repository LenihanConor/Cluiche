import { create } from 'zustand';
import { bridgeRequest } from './bridge';

interface UndoStoreV2State {
    canUndo: boolean;
    canRedo: boolean;
    count: number;
    isDirty: boolean;

    // Sync state from backend
    syncFromBackend: () => Promise<void>;
    applyUndoResponse: (res: { canUndo: boolean; canRedo: boolean; isDirty: boolean }) => void;

    // Perform undo/redo
    undo: () => Promise<void>;
    redo: () => Promise<void>;
}

export const useUndoStoreV2 = create<UndoStoreV2State>((set) => ({
    canUndo: false,
    canRedo: false,
    count: 0,
    isDirty: false,

    syncFromBackend: async () => {
        const res = await bridgeRequest('history.getState') as any;
        if (res?.ok) {
            set({ canUndo: res.canUndo ?? false, canRedo: res.canRedo ?? false, count: res.count ?? 0, isDirty: res.isDirty ?? false });
        }
    },

    applyUndoResponse: (res) => {
        set({ canUndo: res.canUndo, canRedo: res.canRedo, isDirty: res.isDirty });
    },

    undo: async () => {
        const res = await bridgeRequest('history.undo') as any;
        if (res?.ok) {
            set({ canUndo: res.canUndo ?? false, canRedo: res.canRedo ?? false, isDirty: res.isDirty ?? false });
        }
    },

    redo: async () => {
        const res = await bridgeRequest('history.redo') as any;
        if (res?.ok) {
            set({ canUndo: res.canUndo ?? false, canRedo: res.canRedo ?? false, isDirty: res.isDirty ?? false });
        }
    },
}));
