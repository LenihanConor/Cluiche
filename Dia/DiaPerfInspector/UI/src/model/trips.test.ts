import { buildTripRows, parseTripRecord, tripRangesFromRecords, tripsAvailable } from './trips';
import type { TripRecord } from '../types';

function rec(over: Partial<TripRecord> = {}): TripRecord {
    return {
        unit: 'Main', buildConfig: 'Debug', firstFrame: 100, lastFrame: 120, budgetMs: 33.3, peakMs: 55.5,
        suppressedSinceLastTrip: 0, topModules: [{ module: 'A', ms: 20 }, { module: 'B', ms: 9 }],
        sections: [], worstFrame: { index: 110, unitMs: 55.5, modules: [] }, recentUnitMs: [], ...over,
    };
}

describe('tripRangesFromRecords', () => {
    it('maps first/last frame straight to entry indexes (assumed 1:1)', () => {
        expect(tripRangesFromRecords([rec(), rec({ unit: 'Sim', firstFrame: 5, lastFrame: 5 })])).toEqual([
            { unit: 'Main', firstEntry: 100, lastEntry: 120 },
            { unit: 'Sim', firstEntry: 5, lastEntry: 5 },
        ]);
    });
    it('applies an optional offset and drops inverted ranges', () => {
        expect(tripRangesFromRecords([rec()], 10)).toEqual([{ unit: 'Main', firstEntry: 110, lastEntry: 130 }]);
        expect(tripRangesFromRecords([rec({ firstFrame: 9, lastFrame: 3 })])).toEqual([]);
    });
});

describe('parseTripRecord', () => {
    it('parses the SerializeTripRecord keys', () => {
        const r = parseTripRecord({
            record_type: 'trip', unit: 'Sim', build_config: 'Debug', first_frame: 10, last_frame: 20,
            budget_ms: 16.6, peak_ms: 40, suppressed_since_last_trip: 2,
            top_modules: [{ module: 'X', ms: 12 }], sections: [],
            worst_frame: { index: 3, unit_ms: 40, modules: [{ module: 'X', ms: 30 }] }, recent_unit_ms: [1, 2],
        });
        expect(r?.unit).toBe('Sim');
        expect(r?.firstFrame).toBe(10);
        expect(r?.topModules[0]).toEqual({ module: 'X', ms: 12 });
        expect(r?.worstFrame.unitMs).toBe(40);
        expect(r?.recentUnitMs).toEqual([1, 2]);
    });
    it('rejects non-trip input', () => {
        expect(parseTripRecord(null)).toBeNull();
        expect(parseTripRecord({ record_type: 'trip' })).toBeNull();
    });
});

describe('buildTripRows / tripsAvailable', () => {
    it('summarises records with the top modules', () => {
        const rows = buildTripRows([rec()]);
        expect(rows).toEqual([{
            key: 'Main:100-120', unit: 'Main', firstFrame: 100, lastFrame: 120, peakMs: 55.5, budgetMs: 33.3,
            top: 'A 20.0, B 9.0',
        }]);
    });
    it('trips are unavailable when listed in the unavailable list', () => {
        expect(tripsAvailable(['latency', 'trips'])).toBe(false);
        expect(tripsAvailable(['latency'])).toBe(true);
        expect(tripsAvailable(undefined)).toBe(false);
    });
});
