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
    /** Tier 1 job aggregates (added 2026-11-12); absent when the game does not publish jobs. */
    jobs?: { units: JobsTier1Unit[] };
    unavailable: string[];
}

/** Tier 1 per-owner job aggregate on `perf.timing` (PerfTimingPublisher BuildJobs). */
export interface JobsTier1Owner {
    name: string;
    crc: number;
    count: number;
    busyMs: number;
    waitP50Ms: number;
    waitP95Ms: number;
    worstWaitMs: number;
}

/** Tier 1 pool counters. capUse / timeAtCapMs / peakQueueDepth are published as 0 and listed in `unavailable`. */
export interface JobsTier1Pool {
    capUse: number;
    timeAtCapMs: number;
    peakQueueDepth: number;
    inlineFallbacks: number;
    inboxDrops: number;
}

export interface JobsTier1Unit { unit: string; owners: JobsTier1Owner[]; pool: JobsTier1Pool }

/** `jobs` key of a `perf.window` reply (PackPerfWindowJobs). frameAgg arrays are base64 uint16 LE, owner-major. */
export interface JobsWindowWireUnit {
    unit: string;
    unitCrc: number;
    perJobRecording: boolean;
    hz: number;
    firstFrame: number;
    framesPerBucket: number;
    bucketCount: number;
    owners: { crc: number; name: string }[];
    frameAgg: { count: string; busyMs: string; worstWaitMs: string };
}

export interface JobsWindowWire {
    bucketMs: number;
    perJobRecording: boolean;
    units: JobsWindowWireUnit[];
    steps: unknown[];
}

/**
 * Tier 2 deferred-step summary (spec "Jobs and workers", one per step). The game sends none today
 * (DiaDeferredStep is not built), so the wire names below are the spec field list, camel-cased: a PROVISIONAL shape.
 * Times are steady-clock ns; the *Ms fields are plain decimals.
 */
export interface StepSummary {
    /** Unit that hosts the channel; optional, the first unit with per-job recording is used when absent. */
    unit?: string;
    channel: string;
    kickKey: number;
    applyKey: number;
    kick: number;
    firstStart: number;
    ready: number;
    apply: number;
    stallMs: number;
    cpuMs: number;
    capWaitMs: number;
    workerWaitMs: number;
    ranges: number;
    /** Sim-side capture / apply cost; optional, the critical-path cost is unavailable without both. */
    captureMs?: number;
    applyMs?: number;
}

/** `perf.jobs` reply: packed base64 arrays one per field, little-endian (PackPerfJobsRecords). */
export interface RawJobsWire {
    v: number;
    unit: string;
    unitCrc: number;
    fromNs: number;
    toNs: number;
    perJobRecording: boolean;
    truncated: boolean;
    count: number;
    startNs: string;
    durNs: string;
    owner: string;
    tag: string;
    waitUs: string;
    worker: string;
    range: string;
    rangeCount: string;
    flags: string;
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

/**
 * One action's end-to-end latency, as a future bridge would deliver it. Derived from the Sim-side `perf.latency`
 * query reply (VisualDebuggerModule::HandleLatencyQuery: actions[name] = {count, p50Ms, p95Ms, minMs, maxMs}, scope
 * "sim_receive_to_snapshot_published"). Action names are "<domain>.<command>". The game carries no targets; the
 * only target the spec names is 100 ms for slider-drag-to-visible, so targetMs is supplied by the client.
 */
export interface LatencyAction {
    action: string;
    targetMs: number;
    p50Ms: number;
    p95Ms: number;
    /** Completions in the probe ring (max 40). */
    samples: number;
}
