import { create } from 'zustand';
import { queryLatency } from './bridge';
import { parseLatencyReply } from './model/latency';
import type { LatencyAction } from './types';

interface LatencyState {
    /** null until the game delivers latency data; [] when the probe is live with no samples yet. */
    actions: LatencyAction[] | null;
    /** Why there is no data, when known (e.g. a Release game); null otherwise. */
    reason: string | null;
    setActions: (actions: LatencyAction[] | null, reason?: string | null) => void;
    /** Asks the game (perf.latency). An error reply leaves actions null with a reason. */
    refresh: () => Promise<void>;
    reset: () => void;
}

export const useLatencyStore = create<LatencyState>((set) => ({
    actions: null,
    reason: null,
    setActions: (actions, reason = null) => set({ actions, reason }),
    refresh: async () => {
        let reply;
        try {
            reply = parseLatencyReply(await queryLatency());
        } catch {
            reply = { actions: null, reason: 'perf.latency query failed' };
        }
        set({ actions: reply.actions, reason: reply.reason });
    },
    reset: () => set({ actions: null, reason: null }),
}));
