import { useEffect } from 'react';
import type { LiveConnectionState } from './types';
import { useLatencyStore } from './useLatencyStore';

/** Poll period for perf.latency (Sim-hosted query). */
export const kLatencyPollIntervalMs = 2000;

/**
 * Polls perf.latency every 2 s while connected and the Frames tab is visible (`active`). Nothing is requested
 * otherwise. Losing the connection clears the store so a stale latency is never shown for a new game.
 */
export function useLatencyPolling(connection: LiveConnectionState, active: boolean): void {
    const connected = connection === 'connected';

    useEffect(() => {
        if (!connected) useLatencyStore.getState().reset();
    }, [connected]);

    useEffect(() => {
        if (!connected || !active) return;
        void useLatencyStore.getState().refresh();
        const id = setInterval(() => { void useLatencyStore.getState().refresh(); }, kLatencyPollIntervalMs);
        return () => clearInterval(id);
    }, [connected, active]);
}
