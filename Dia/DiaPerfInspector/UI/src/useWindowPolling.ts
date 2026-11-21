import { useEffect } from 'react';
import type { LiveConnectionState } from './types';
import { useWindowStore } from './useWindowStore';

/** Poll period. Never faster than 1 Hz: nothing per frame (SD-PERF-2/7). */
export const kPollIntervalMs = 1000;

/**
 * Refreshes the window once per second while connected and not paused. Connecting triggers one
 * immediate refresh; resuming from pause refetches inside the store's togglePause.
 */
export function useWindowPolling(connection: LiveConnectionState): void {
    const connected = connection === 'connected';
    const paused = useWindowStore((s) => s.view.pausedAt !== null);

    useEffect(() => {
        if (connected) void useWindowStore.getState().refresh();
    }, [connected]);

    useEffect(() => {
        if (!connected || paused) return;
        const id = setInterval(() => { void useWindowStore.getState().refresh(); }, kPollIntervalMs);
        return () => clearInterval(id);
    }, [connected, paused]);
}
