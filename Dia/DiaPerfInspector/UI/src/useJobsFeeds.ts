import { useEffect } from 'react';
import { useWindowStore } from './useWindowStore';
import { usePerfTimingStore } from './usePerfTimingStore';
import { useJobsStore } from './useJobsStore';

/**
 * Derives the jobs store from data the Frames tab already receives: the perf.window reply (tier 2) and the perf.timing
 * push (tier 1). No requests are made here; only the store raw fetch (tier 3) ever queries the game.
 */
export function useJobsFeeds(): void {
    useEffect(() => {
        const applyWindow = (reply: ReturnType<typeof useWindowStore.getState>['reply']) => useJobsStore.getState().applyWindowReply(reply);
        const applyTiming = (t: ReturnType<typeof usePerfTimingStore.getState>['timing']) => useJobsStore.getState().applyTiming(t);
        applyWindow(useWindowStore.getState().reply);
        applyTiming(usePerfTimingStore.getState().timing);
        const offW = useWindowStore.subscribe((s, prev) => { if (s.reply !== prev.reply) applyWindow(s.reply); });
        const offT = usePerfTimingStore.subscribe((s, prev) => { if (s.timing !== prev.timing) applyTiming(s.timing); });
        return () => { offW(); offT(); };
    }, []);
}
