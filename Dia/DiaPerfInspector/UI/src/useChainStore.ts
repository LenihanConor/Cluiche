import { create } from 'zustand';
import type { ChainData } from './types';

interface ChainState {
    /** null until the game delivers chain data (no wire for it yet). */
    chain: ChainData | null;
    setChain: (chain: ChainData | null) => void;
}

export const useChainStore = create<ChainState>((set) => ({
    chain: null,
    setChain: (chain) => set({ chain }),
}));
