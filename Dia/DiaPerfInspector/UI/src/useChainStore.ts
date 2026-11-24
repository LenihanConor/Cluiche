import { create } from 'zustand';
import { queryChain } from './bridge';
import { parseChainReply } from './model/chainReply';
import type { ChainData } from './types';

interface ChainState {
    /** null until the game delivers a chain (perf.chain). */
    chain: ChainData | null;
    /** Why there is no chain, when the game said so; null otherwise. */
    reason: string | null;
    setChain: (chain: ChainData | null, reason?: string | null) => void;
    /** Asks the game for the latest chain. An unavailable or unusable reply leaves chain null with the reason. */
    refresh: () => Promise<void>;
    reset: () => void;
}

export const useChainStore = create<ChainState>((set) => ({
    chain: null,
    reason: null,
    setChain: (chain, reason = null) => set({ chain, reason }),
    refresh: async () => {
        let reply;
        try {
            reply = parseChainReply(await queryChain());
        } catch {
            reply = { chain: null, reason: 'perf.chain query failed' };
        }
        set({ chain: reply.chain, reason: reply.reason });
    },
    reset: () => set({ chain: null, reason: null }),
}));
