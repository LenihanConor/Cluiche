// Chrome-trace JSON export for Perfetto compatibility.
// Pure, deterministic, no DOM.

import type { Lane } from './laneModel';

export interface ChromeTraceEvent {
    ph: string; // phase: M (metadata), X (complete), etc.
    pid: number;
    tid: number;
    ts: number; // microseconds
    name: string;
    dur?: number; // microseconds, for X events
    args?: Record<string, unknown>;
}

export interface ChromeTraceJSON {
    traceEvents: ChromeTraceEvent[];
    displayTimeUnit: 'ms';
}

export function buildChromeTrace(lanes: Lane[]): ChromeTraceJSON {
    const events: ChromeTraceEvent[] = [];

    // Metadata events: process and thread names first
    events.push({
        ph: 'M',
        pid: 1,
        tid: 0,
        ts: 0,
        name: 'process_name',
        args: { name: 'Cluiche' },
    });

    // Add thread_name metadata for each lane (use lane index as tid)
    for (let tidIdx = 0; tidIdx < lanes.length; tidIdx++) {
        const lane = lanes[tidIdx];
        events.push({
            ph: 'M',
            pid: 1,
            tid: tidIdx,
            ts: 0,
            name: 'thread_name',
            args: { name: lane.unit },
        });
    }

    // Add tick and module slice events for each lane
    for (let tidIdx = 0; tidIdx < lanes.length; tidIdx++) {
        const lane = lanes[tidIdx];
        const m = lane.modules.length;

        for (let i = 0; i < lane.count; i++) {
            // Skip gap entries
            if (lane.gap[i]) continue;

            const tSeconds = lane.t[i];
            const tsUs = tSeconds * 1e6;
            const tickMs = lane.tickMs[i];
            const durUs = tickMs * 1000;

            // Emit tick event
            const tickEvent: ChromeTraceEvent = {
                ph: 'X',
                pid: 1,
                tid: tidIdx,
                ts: tsUs,
                dur: durUs,
                name: `${lane.unit} tick`,
                args: { is_tick: true },
            };

            if (lane.clamped[i]) {
                tickEvent.args!.clamped = true;
            }

            events.push(tickEvent);

            // Emit module slices laid back-to-back
            let sliceStartUs = tsUs;
            for (let k = 0; k < m; k++) {
                const moduleMs = lane.moduleMs[k * lane.count + i];
                // Skip zero-ms modules
                if (moduleMs === 0) continue;

                // Over-attributed entries keep their real durations, so slices may run past the tick end.
                const sliceDurUs = moduleMs * 1000;

                const moduleEvent: ChromeTraceEvent = {
                    ph: 'X',
                    pid: 1,
                    tid: tidIdx,
                    ts: sliceStartUs,
                    dur: sliceDurUs,
                    name: lane.modules[k].name,
                };

                events.push(moduleEvent);
                sliceStartUs += sliceDurUs;
            }
        }
    }

    // Sort: metadata first (ts === 0), then by ts ascending
    events.sort((a, b) => {
        // Metadata (ph === 'M') sorts first
        const aIsMeta = a.ph === 'M';
        const bIsMeta = b.ph === 'M';
        if (aIsMeta !== bIsMeta) {
            return aIsMeta ? -1 : 1;
        }
        // Then by ts
        const ats = a.ts ?? 0;
        const bts = b.ts ?? 0;
        return ats - bts;
    });

    return {
        traceEvents: events,
        displayTimeUnit: 'ms',
    };
}
