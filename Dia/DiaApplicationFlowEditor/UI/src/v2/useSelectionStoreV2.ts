import { create } from 'zustand';

interface SelectionStoreV2State {
    puId: string | null;
    streamId: string | null;
    setPU: (puId: string | null) => void;
    setStream: (streamId: string | null) => void;
    clear: () => void;
}

export const useSelectionStoreV2 = create<SelectionStoreV2State>((set) => ({
    puId: null,
    streamId: null,
    setPU: (puId) => set({ puId }),
    setStream: (streamId) => set({ streamId }),
    clear: () => set({ puId: null, streamId: null }),
}));
