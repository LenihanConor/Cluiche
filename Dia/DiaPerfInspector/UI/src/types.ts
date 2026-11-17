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

/** One module (or section) cost entry, as the game serialises them in trip records. */
export interface NamedMs { module: string; ms: number }

/**
 * One over-budget trip. Mirrors the keys of Dia::Observation::Profile::SerializeTripRecord
 * (trips.jsonl), camel-cased. firstFrame/lastFrame are UNIT frame numbers as counted by the trip detector;
 * mapping them to coarse-ring entry indexes is an unverified assumption (see tripRangesFromRecords).
 */
export interface TripRecord {
    unit: string;
    buildConfig: string;
    firstFrame: number;
    lastFrame: number;
    budgetMs: number;
    peakMs: number;
    suppressedSinceLastTrip: number;
    topModules: NamedMs[];
    sections: NamedMs[];
    worstFrame: { index: number; unitMs: number; modules: NamedMs[] };
    recentUnitMs: number[];
}

/**
 * Timeline event marker. PROVISIONAL: the game has no marker wire format yet, so this is the minimal
 * shape the events row needs. Frame is an entry index of `unit`.
 */
export interface Marker { unit: string; frame: number; label: string; colour?: string }

/** One module line of a `perf.frame` reply (ms are plain decimals, not 0.01 ms fixed point). */
export interface FrameModuleCost { crc: number; name: string; ms: number }

/** Parsed `perf.frame` reply, modules in schedule order (the reply order). */
export interface FrameDetailData {
    unit: string;
    frame: number;
    unitMs: number;
    flags: number;
    modules: FrameModuleCost[];
    /** null when sections are available (not produced by the game today). */
    sectionsUnavailableReason: string | null;
    sections: { name: string; ms: number }[];
}

/** One span of a cross-thread chain. Times are ms on the shared steady clock (SpanRecord start/end steady ns / 1e6). */
export interface ChainStageData {
    /** Span identity; edges refer to it (SpanRecord.spanId, as a decimal string). */
    id: string;
    unit: string;
    name: string;
    startMs: number;
    endMs: number;
}

/**
 * Stream hop between two stages: the reader span flow link (SpanRecord.linkSpanId) points at fromId.
 * Fields are the per-stream edge metrics (stream.<id>.edge.*) already converted to ms / per second.
 */
export interface ChainEdgeData {
    stream: string;
    fromId: string;
    toId: string;
    /** Bytes of one write (edge.bytes / edge.writes). */
    bytes: number;
    /** Writes per second (rate of edge.writes). */
    ratePerSec: number;
    /** edge.data_age_ns in ms. */
    dataAgeMs: number;
    /** edge.write_cost_ns in ms. */
    writeCostMs: number;
}

/** Shape a future perf.chain reply or topic would deliver. Stages may arrive in any order. */
export interface ChainData {
    name: string;
    stages: ChainStageData[];
    edges: ChainEdgeData[];
}
