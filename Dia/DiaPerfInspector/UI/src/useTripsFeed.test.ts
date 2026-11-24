import { renderHook } from '@testing-library/react';
import { kTripTopic } from './bridge';
import { kMaxTrips, useWindowStore } from './useWindowStore';
import { subscribePerfTrips, useTripsFeed } from './useTripsFeed';
import type { LiveConnectionState } from './types';

function push(topic: string, data: unknown) {
    window.dispatchEvent(new MessageEvent('message', { data: { __dia: true, topic, data } }));
}

/** Keys of Dia SerializeTripRecord (what the game publishes on perf.trip). */
function tripJson(first: number, last = first + 5, unit = 'SimPU') {
    return {
        record_type: 'trip', unit, build_config: 'Debug', first_frame: first, last_frame: last,
        budget_ms: 16.6, peak_ms: 40, suppressed_since_last_trip: 0,
        top_modules: [{ module: 'ModA', ms: 30 }], sections: [],
        worst_frame: { index: 1, unit_ms: 40, modules: [] }, recent_unit_ms: [5, 40],
    };
}

beforeEach(() => { useWindowStore.getState().reset(); });

describe('perf.trip feed', () => {
    it('appends a pushed trip to the store and derives the lane range', () => {
        const off = subscribePerfTrips();
        push(kTripTopic, tripJson(10, 20));
        const s = useWindowStore.getState();
        expect(s.trips).toHaveLength(1);
        expect(s.trips[0].unit).toBe('SimPU');
        expect(s.trips[0].firstFrame).toBe(10);
        expect(s.tripRanges).toEqual([{ unit: 'SimPU', firstEntry: 10, lastEntry: 20 }]);
        off();
    });

    it('keeps newest last and caps the list', () => {
        const off = subscribePerfTrips();
        for (let i = 0; i < kMaxTrips + 5; ++i) push(kTripTopic, tripJson(i));
        const s = useWindowStore.getState();
        expect(s.trips).toHaveLength(kMaxTrips);
        expect(s.trips[0].firstFrame).toBe(5);
        expect(s.trips[kMaxTrips - 1].firstFrame).toBe(kMaxTrips + 4);
        expect(s.tripRanges).toHaveLength(kMaxTrips);
        off();
    });

    it('ignores malformed payloads without throwing', () => {
        const off = subscribePerfTrips();
        expect(() => {
            push(kTripTopic, null);
            push(kTripTopic, 'nope');
            push(kTripTopic, 42);
            push(kTripTopic, { unit: 'SimPU' });
            push(kTripTopic, { unit: 3, first_frame: 'a', last_frame: 1 });
        }).not.toThrow();
        expect(useWindowStore.getState().trips).toHaveLength(0);
        off();
    });

    it('ignores other topics and stops after unsubscribe', () => {
        const off = subscribePerfTrips();
        push('perf_inspector.timing', tripJson(1));
        expect(useWindowStore.getState().trips).toHaveLength(0);
        off();
        push(kTripTopic, tripJson(1));
        expect(useWindowStore.getState().trips).toHaveLength(0);
    });

    it('useTripsFeed collects while connected and clears on disconnect', () => {
        const { rerender, unmount } = renderHook(({ c }: { c: LiveConnectionState }) => useTripsFeed(c), {
            initialProps: { c: 'connected' as LiveConnectionState },
        });
        push(kTripTopic, tripJson(1));
        push(kTripTopic, tripJson(2));
        expect(useWindowStore.getState().trips).toHaveLength(2);
        rerender({ c: 'disconnected' });
        const s = useWindowStore.getState();
        expect(s.trips).toEqual([]);
        expect(s.tripRanges).toEqual([]);
        unmount();
    });
});
