// Trip records: parsing, lane ranges and list rows. No React or DOM imports.

import type { NamedMs, TripRecord } from '../types';
import type { TripRange } from './laneModel';

const str = (v: unknown, d = ''): string => (typeof v === 'string' ? v : d);
const num = (v: unknown, d = 0): number => (typeof v === 'number' && Number.isFinite(v) ? v : d);

function named(v: unknown): NamedMs[] {
    if (!Array.isArray(v)) return [];
    return v.map((e) => ({ module: str((e as { module?: unknown })?.module, '?'), ms: num((e as { ms?: unknown })?.ms) }));
}

/** Parses one trips.jsonl object (keys of SerializeTripRecord). Null when it is not a usable trip. */
export function parseTripRecord(json: unknown): TripRecord | null {
    if (!json || typeof json !== 'object') return null;
    const o = json as Record<string, unknown>;
    if (typeof o.unit !== 'string' || typeof o.first_frame !== 'number' || typeof o.last_frame !== 'number') return null;
    const w = (o.worst_frame ?? {}) as Record<string, unknown>;
    return {
        unit: o.unit,
        buildConfig: str(o.build_config),
        firstFrame: o.first_frame,
        lastFrame: o.last_frame,
        budgetMs: num(o.budget_ms),
        peakMs: num(o.peak_ms),
        suppressedSinceLastTrip: num(o.suppressed_since_last_trip),
        topModules: named(o.top_modules),
        sections: named(o.sections),
        worstFrame: { index: num(w.index), unitMs: num(w.unit_ms), modules: named(w.modules) },
        recentUnitMs: Array.isArray(o.recent_unit_ms) ? (o.recent_unit_ms as unknown[]).map((x) => num(x)) : [],
    };
}

/**
 * Maps trip records to the lane model's TripRange (absolute coarse entry indexes).
 *
 * ASSUMPTION (unverified): TripRecord.firstFrame/lastFrame are unit frame numbers counted by the trip
 * detector, and one coarse-ring entry is one forward pass of the unit, so entry index == frame number
 * (offset 0). Nothing on the wire proves the two counters start together; `entryOffset` exists so the
 * mapping can be corrected once the trips topic lands. Inverted ranges are dropped.
 */
export function tripRangesFromRecords(records: TripRecord[], entryOffset = 0): TripRange[] {
    const out: TripRange[] = [];
    for (const r of records) {
        if (r.lastFrame < r.firstFrame) continue;
        out.push({ unit: r.unit, firstEntry: r.firstFrame + entryOffset, lastEntry: r.lastFrame + entryOffset });
    }
    return out;
}

export interface TripRow {
    key: string;
    unit: string;
    firstFrame: number;
    lastFrame: number;
    peakMs: number;
    budgetMs: number;
    top: string;
}

export function buildTripRows(records: TripRecord[]): TripRow[] {
    return records.map((r) => ({
        key: `${r.unit}:${r.firstFrame}-${r.lastFrame}`,
        unit: r.unit,
        firstFrame: r.firstFrame,
        lastFrame: r.lastFrame,
        peakMs: r.peakMs,
        budgetMs: r.budgetMs,
        top: r.topModules.slice(0, 3).map((m) => `${m.module} ${m.ms.toFixed(1)}`).join(', '),
    }));
}

/** False when the game lists trips as unavailable (or no timing push has arrived yet). */
export function tripsAvailable(unavailable: string[] | undefined): boolean {
    return Array.isArray(unavailable) && !unavailable.includes('trips');
}
