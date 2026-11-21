import { create } from 'zustand';
import { kTimingTopic, onTopic } from './bridge';
import type { LiveConnectionState, PerfTimingPush } from './types';

interface PerfTimingState {
    connectionState: LiveConnectionState;
    timing: PerfTimingPush | null;
    setConnectionState: (s: LiveConnectionState) => void;
    applyTiming: (push: PerfTimingPush) => void;
    clear: () => void;
}

export const usePerfTimingStore = create<PerfTimingState>((set) => ({
    connectionState: 'disconnected',
    timing: null,
    setConnectionState: (connectionState) =>
        set(connectionState === 'connected' ? { connectionState } : { connectionState, timing: null }),
    applyTiming: (timing) => set({ timing }),
    clear: () => set({ connectionState: 'disconnected', timing: null }),
}));

/** Wires the perf.timing push topic into the store. Returns an unsubscribe function. */
export function subscribePerfTiming(): () => void {
    return onTopic(kTimingTopic, (d) => {
        if (d && typeof d === 'object') usePerfTimingStore.getState().applyTiming(d as PerfTimingPush);
    });
}
