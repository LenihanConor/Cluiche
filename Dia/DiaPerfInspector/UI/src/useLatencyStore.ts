import { create } from 'zustand';
import type { LatencyAction } from './types';

interface LatencyState {
    /** null until the game delivers latency data (no live channel yet: the probe lives on Sim, Debug-only). */
    actions: LatencyAction[] | null;
    setActions: (actions: LatencyAction[] | null) => void;
}

export const useLatencyStore = create<LatencyState>((set) => ({
    actions: null,
    setActions: (actions) => set({ actions }),
}));
