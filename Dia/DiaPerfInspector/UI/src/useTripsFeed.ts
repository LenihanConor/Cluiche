import { useEffect } from 'react';
import { kTripTopic, onTopic } from './bridge';
import { parseTripRecord } from './model/trips';
import { useWindowStore } from './useWindowStore';
import type { LiveConnectionState } from './types';

/** Wires the perf.trip push topic into the window store. Malformed payloads are ignored. Returns an unsubscribe function. */
export function subscribePerfTrips(): () => void {
    return onTopic(kTripTopic, (d) => {
        const record = parseTripRecord(d);
        if (record) useWindowStore.getState().appendTrip(record);
    });
}

/** Collects pushed trips while connected; clears them when the connection drops. */
export function useTripsFeed(connection: LiveConnectionState): void {
    useEffect(() => subscribePerfTrips(), []);
    useEffect(() => {
        if (connection !== 'connected') useWindowStore.getState().setTrips([]);
    }, [connection]);
}
