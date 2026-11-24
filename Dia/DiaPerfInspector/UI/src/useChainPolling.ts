import { useEffect } from 'react';
import type { LiveConnectionState } from './types';
import { useChainStore } from './useChainStore';

/** Poll period for perf.chain: the chain is built on demand in the game, so keep it slow (SD-PERF-2/7). */
export const kChainPollIntervalMs = 2000;

/**
 * Polls perf.chain every 2 s while connected and the Frames tab is visible (`active`). Nothing is requested
 * otherwise. Losing the connection clears the store so a stale chain is never shown for a new game.
 */
export function useChainPolling(connection: LiveConnectionState, active: boolean): void {
    const connected = connection === 'connected';

    useEffect(() => {
        if (!connected) useChainStore.getState().reset();
    }, [connected]);

    useEffect(() => {
        if (!connected || !active) return;
        void useChainStore.getState().refresh();
        const id = setInterval(() => { void useChainStore.getState().refresh(); }, kChainPollIntervalMs);
        return () => clearInterval(id);
    }, [connected, active]);
}
