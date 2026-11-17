// Payload types for the game-side perf commands and topics. Plain data only.

export type LiveConnectionState = 'disconnected' | 'connecting' | 'connected';

export interface PerfUnitTiming {
    unit: string;
    hz: number;
    samples: number;
    /** [ewma, p95] of the unit tick, in ms. */
    tick: [number, number];
    modules: string[];
    ewma: number[];
    p95: number[];
}

/** Payload of the `perf.timing` topic (10 Hz push). */
export interface PerfTimingPush {
    scale: number;
    windowSec: number;
    units: PerfUnitTiming[];
    selfMs?: number;
    unavailable: string[];
}

/** Reply envelope for relayed `perf.window` / `perf.frame` queries. */
export interface PerfQueryReply<T = unknown> {
    success: boolean;
    result: T;
}
